#include <assert.h> /* for assert */
#include <err.h>
#include <stdlib.h> /* for NULL */
#include <unistd.h> /* for pipe */
#include <stdlib.h>
#include <stdio.h>

#include "postwait.h"
#include "utils.h"
#include "bop.h"

typedef short bool;
#include "atomic_ops.h"

#define LABEL_HASH_TABLE_SIZE 79 /* make it a prime number */
#define MAX_ACTIVE_PW_LABELS LABEL_HASH_TABLE_SIZE/2
#define PAGE_HASH_TABLE_SIZE LABEL_HASH_TABLE_SIZE*17+2
#define MAX_ACTIVE_PW_PAGES PAGE_HASH_TABLE_SIZE/2

/* label hash table */
static LabelHashEntry **pwLabelHashTable;
/* one lock per hash bucket (covers an entire chain) */
static tatas_lock_t *pwLabelHashLocks;
/* the space to store label entries */
static LabelHashEntry *pwLabelEntries=NULL;

/* the pipe that allocates and recollects the label entries */
static int labelQueue[2];

/* page hash roots */
static PageHashEntry *pageEntries=NULL;
static PageHashEntry **pageHashTable;
static tatas_lock_t  *pageHashLocks;
static int pageQueue[2];

// static function declarations
static void LabelHashInit(void);
static void PageHashInit(void);
static void LabelHashDelete(unsigned pwLabel);
static LabelHashEntry *LabelHashSearchNoInsert(unsigned pwLabel);
static PageHashEntry *PageHashInsert(unsigned pageid, LabelHashEntry *labelEntry);
static void PageHashClearLabel(LabelHashEntry *lentry);

void* bop_shmmap(size_t length) {
  return mmap(NULL, length ,PROT_READ | PROT_WRITE, 
              MAP_ANONYMOUS | MAP_SHARED, -1 , 0);
}

static void LabelHashInit(void) {
  int i, hasError=0;
  
  /* allocate the hash table for the active labels */
  pwLabelHashTable = bop_shmmap(sizeof(LabelHashEntry*)*LABEL_HASH_TABLE_SIZE);
  if (((int)pwLabelHashTable)==-1) 
    err( 1, "allocating pwLabelHashTable" );

  for (i=0; i<LABEL_HASH_TABLE_SIZE; i++) pwLabelHashTable[i]=NULL;

  pwLabelHashLocks = bop_shmmap(sizeof(tatas_lock_t)*LABEL_HASH_TABLE_SIZE);
  if ( (int)pwLabelHashLocks == -1 ) err( 1, "allocating pwLabelHashLocks" );
 
  /* allocate the resource array for the active labels */
  pwLabelEntries = bop_shmmap( sizeof(LabelHashEntry)*MAX_ACTIVE_PW_LABELS );
  if (((int)pwLabelEntries)==-1)
    err( 1, "allocating pwLabelEntries");

  /* allocate a pipe for each active label resource */
  for (i=0; i<MAX_ACTIVE_PW_LABELS; i++) {
    /* the label is inactive if senderSpec=receiverSpec=1 */
    pwLabelEntries[i].senderSpec = pwLabelEntries[i].receiverSpec = -1;
    hasError |= pipe(pwLabelEntries[i].pipe);
  }

  /* initialize the resource pool */
  hasError |= pipe(labelQueue);

  if (hasError) {
      perror(" post wait label pipes creation failed: ");
      bop_error();
      return;
  }

  /* add resources */
  for (i=0; i<MAX_ACTIVE_PW_LABELS; i++) {
    LabelHashEntry *p = &pwLabelEntries[i];
    //bop_msg(4, "adding label %x to label queue", p);
    SAFE(write(labelQueue[1], &p, sizeof(void*)));
  }

}

static void PageHashInit(void) {
  int i, hasError=0;

  /* allocate the hash table for the active labels */
  pageHashTable = (PageHashEntry **) mmap(NULL,
		    sizeof(PageHashEntry*)*PAGE_HASH_TABLE_SIZE, 
		    PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1 , 0);
  if (((int)pageHashTable)==-1) {
    perror("allocating pageHashTable");
    assert(0);
  }
  for (i=0; i<PAGE_HASH_TABLE_SIZE; i++) pageHashTable[i]=NULL;
 
  pageHashLocks = bop_shmmap( sizeof(tatas_lock_t) * PAGE_HASH_TABLE_SIZE );
  if( (int) pageHashLocks == -1) err( 1, "allocation page hash locks" );

  /* allocate the resource array for the active labels */
  pageEntries = (PageHashEntry *) 
    bop_shmmap(sizeof(PageHashEntry)*MAX_ACTIVE_PW_PAGES); 
  if (((int)pageEntries)==-1) {
    perror("allocating pageEntries");
    assert(0);
  }

  /* initialize the resource pool */
  hasError |= pipe(pageQueue);

  if (hasError) {
      perror(" post wait page pipes creation failed: ");
      bop_error();
      return;
  }

  /* add resources */
  for (i=0; i<MAX_ACTIVE_PW_PAGES; i++) {
    PageHashEntry * resp = &(pageEntries[i]);
    SAFE(write(pageQueue[1], &resp, sizeof(void*)));
  }
}

/* To ensure proper initialization of post-wait, this must be called
   in BOP_Init. */
void BOP_PostWaitInit(void) {
  LabelHashInit();
  PageHashInit();
}

/* Reset must be called before starting new speculative executions
   after a previous failure or completion. */
void BOP_PostWaitReset(void) {
  int i;

  /* clear all the locks */
  for (i=0; i<LABEL_HASH_TABLE_SIZE; i++) pwLabelHashLocks[i]=0;
  for (i=0; i<PAGE_HASH_TABLE_SIZE; i++) pageHashLocks[i]=0;

  /* We will find all active labels in the hash table, reset their
     pipes, and remove them from the hash. */
  for (i=0; i<MAX_ACTIVE_PW_LABELS; i++) {
    if (pwLabelEntries[i].senderSpec==-1 && 
	pwLabelEntries[i].receiverSpec==-1)
      continue;
    LabelHashEntry *entry = LabelHashSearchNoInsert(pwLabelEntries[i].label);
    if (entry!=NULL) {
      DeactivatePostWait(entry);
    }
  }
}

static void CancelPostWait(LabelHashEntry *);

/* At the end of a spec, clear labels whose receiverSpec has ended. */
void RemoveStalePostWait(void) {
  int i, rearwindow;
  switch (bopType) {
  case BatchBOP:
    rearwindow = mySpecOrder;
    break;
  case RotatingBOP:
    rearwindow = lastGroupMemberRec[(myGroupID-1) & 0x3];
    break;
  default:
    rearwindow = 0; /* silence the compiler warning */
    assert(0);
  }
  for (i=0; i<MAX_ACTIVE_PW_LABELS; i++) {
    LabelHashEntry *entry = &pwLabelEntries[i];
    if (entry->senderSpec==-1 && entry->receiverSpec==-1)
      continue;
    if (entry->receiverSpec > mySpecOrder) 
      continue;
    if (entry->receiverSpec==-1 && entry->senderSpec > rearwindow)
      continue;
    if (entry->receiverSpec==-1) {
      /* Stale post but not received yet.  Empty the send list. */
      bop_msg(3, "Cancel label %d", entry->label);
      CancelPostWait(entry);
    }
    else {
      /* Stale post/wait.  Remove every trace of it. */
      bop_msg(3, "Remove label %d", entry->label);
      DeactivatePostWait(entry);
    }
  } // for i
}


LabelHashEntry* ActivatePostWait(unsigned label, unsigned* pages, unsigned numPages) {
  unsigned i;
  PageHashEntry *lastpentry=NULL;  /* to silence compiler warning */

  /* Get the label entry, which may have already been inserted by the
     receiver. Set the sender field. */
  LabelHashEntry *lentry = LabelHashSearch(label);
  PageHashEntry *pentry = NULL;  /* to silience compiler warning */
  /* One can post *0* pages */
  lentry->firstPage = NULL;
  assert(lentry->senderSpec == -1);
  lentry->senderSpec = mySpecOrder;
  for (i=0; i<numPages; i++) {

    /* create a new page */
    pentry = PageHashInsert((pages[i]>>PAGESIZEX), lentry);

    if (pentry==NULL) {
      /* a conflict with another label. return a place holder. */
      PageHashClearLabel(lentry);
      return lentry;
    }

    pentry->labelEntry = lentry;
    if (i==0) 
      /* add the first entry */
      lentry->firstPage = pentry;
    else
      /* chain the next entry */
      lastpentry->nextInPostChain = pentry;
    lastpentry = pentry;
  }
  /* terminate the list */
  pentry->nextInPostChain = NULL;
  return lentry;
}

/* Chop off the send list.  Clear the pipe. 

   What happens if this happens when a wait is up?  Because we remove
   the page list before clearing the pipe, the receiver can't receive
   wrong data.  */
static void CancelPostWait(LabelHashEntry *entry) {
  int hasError = 0;

  /* Delete the page entries */
  entry->senderSpec = entry->receiverSpec = -1;
  PageHashClearLabel(entry);

  /* Renew the post-wait pipe */
  hasError |= close(entry->pipe[0]) | close(entry->pipe[1]);
  hasError |= pipe(entry->pipe);
  if (hasError) {
    perror(" post wait label pipe reset failed: ");
    bop_error();
  }
}

void DeactivatePostWait(LabelHashEntry *entry) {

  CancelPostWait(entry);

  /* Delete the label entry.  Insert it to labelQueue as a result of
     the call. */
  LabelHashDelete(entry->label);
}

/* Find the entry with pageid that has the desired senderSpec and
   receiverSpec, unless they are -1, which means any match. */
PageHashEntry *PageHashSearch(unsigned pageid, int senderSpec, 
			      int receiverSpec) {
  PageHashEntry *entry, *found=NULL;
  int key = pageid % PAGE_HASH_TABLE_SIZE;

  /* The following call takes a lot of time. */
  bop_msg(4, "PageHashSearch start");

  tas_acquire( &pageHashLocks[key] );
  entry = pageHashTable[key];
  while (entry != NULL && entry->pageid != pageid ) 
    entry = entry->nextInHashChain;
  while (entry != NULL && entry->pageid == pageid) {
    if (entry->labelEntry==NULL) continue; /* an inactive entry */

    char check1 = senderSpec==-1? TRUE: 
      (entry->labelEntry->senderSpec==senderSpec);
    char check2 = receiverSpec=-1? TRUE: 
      (entry->labelEntry->receiverSpec==receiverSpec);

    if (check1 && check2) {
      found = entry;
      break;
    }
    entry = entry->nextInHashChain;
  }
  tas_release( &pageHashLocks[key] );
  return found;
}

/* Whether a page is involved in two improperly overlapped
   post-waits */
static char PageHasLabelConflict(PageHashEntry *page) {
  char hasConflict = FALSE;
  LabelHashEntry *lentry=page->labelEntry; /*cannot be NULL*/
  PageHashEntry *entry;

  int key = page->pageid % PAGE_HASH_TABLE_SIZE;
  tas_acquire( &pageHashLocks[key] );
  entry = pageHashTable[key];
  while (entry != NULL && entry->pageid != page->pageid )
    entry = entry->nextInHashChain;

  /* check for conflicts */
  /* Overlap checking: entries for the same page are allowed only if
   the following is true: 
   A. their senderSpec is the same, or

   B. the senderSpec of the later label (measured by the senderSpec)
   is the same as the latest receiverSpec of all earlier labels.  If an
   earlier label has an unknown receiverSpec, it is considered as
   infinite and the check fails.

   If two entries have different senderSpec, and the receiverSpec of the
   earlier label is unknown, this is considered a conflict, which is a
   conservative assumption.  It is possible that the receiverSpec of the
   earlier label happens before the senderSpec of the later label, but
   we don't wait for this possibility.
  */
  while (entry!=NULL && entry->pageid==page->pageid) {
      if (entry->labelEntry==NULL) continue; /* an inactive entry */
      if (entry->labelEntry->senderSpec!=lentry->senderSpec) {
	/* The use of ">" instead of "!=" is debatable.  Should come back to this?? */
	if (entry->labelEntry->receiverSpec > lentry->senderSpec) {
bop_msg(4, "entry->labelEntry->receiverSpec %d, entry->labelEntry->senderSpec %d, lentry->senderSpec %d", entry->labelEntry->receiverSpec, entry->labelEntry->senderSpec, lentry->senderSpec);
	  hasConflict = TRUE;
	  break;
	}
      }
      entry = entry->nextInHashChain;
  } 
  tas_release( &pageHashLocks[key] );
  if (hasConflict) {
      bop_msg(1, "label %d has a conflict with previous label %d on page %d, ignore", lentry->label, entry->labelEntry->label, page->pageid);
      return TRUE;
  }
  return FALSE;
}

static PageHashEntry *PageHashInsert(unsigned pageid, LabelHashEntry *lentry) {
  PageHashEntry *entry;
  /* First create a page entry */
  PageHashEntry *newpage;
  bop_msg(4, "PageHashInsert start");

  SAFE(read(pageQueue[0], &newpage, sizeof(void*)));
  newpage->pageid = pageid;
  newpage->labelEntry = lentry;

  /* Find place to insert.  Find the first page instance if it exists. */ 
  int key = pageid % PAGE_HASH_TABLE_SIZE;
  tas_acquire( &pageHashLocks[key] );
  entry = pageHashTable[key];
  while (entry != NULL && entry->pageid != pageid )
    entry = entry->nextInHashChain;
  if (entry==NULL) {
    /* first instance of this page */
    newpage->nextInHashChain = pageHashTable[key];
    pageHashTable[key] = newpage;
  }
  else {
    /* save the hook for insertion */
    PageHashEntry *anchor = entry;

    /* insert after the anchor */
    newpage->nextInHashChain = anchor->nextInHashChain;
    anchor->nextInHashChain = newpage;
  }
  tas_release( &pageHashLocks[key] );
  return newpage;
}  

static void PageHashClearLabel(LabelHashEntry *lentry) {
  PageHashEntry *entry, *pred;
  PageHashEntry *page = lentry->firstPage;
  lentry->firstPage = NULL;

  bop_msg(4, "PageHashClearLabel start");

  while (page != NULL) {
    int key = page->pageid % PAGE_HASH_TABLE_SIZE;
    tas_acquire( &pageHashLocks[key] );
    entry = pageHashTable[key];
    if (entry==page) {
      /* remove from the hash */
      pageHashTable[key] = page->nextInHashChain;
    }
    else {
      pred = entry;
      while (pred->nextInHashChain != NULL && (pred->nextInHashChain!=page))
	pred = pred->nextInHashChain;
      if (pred->nextInHashChain == NULL) {
	bop_msg(1, "page hash incorrect (page %d, label %d) should exist but not", page->pageid, lentry->label);
	assert(0);
      }
      /* remove from the hash */
      pred->nextInHashChain = page->nextInHashChain;
    }
    /* move to the next page */
    PageHashEntry *oldpage = page;
    page = page->nextInPostChain;

    oldpage->nextInHashChain = NULL;
    oldpage->nextInPostChain = NULL;
    tas_release( &pageHashLocks[key] );
    SAFE(write(pageQueue[1], &oldpage, sizeof(void*)));
  }
}

static LabelHashEntry *LabelHashSearchNoInsert(unsigned pwLabel) {
  LabelHashEntry *entry;
  int key = pwLabel % LABEL_HASH_TABLE_SIZE;

  bop_msg(4, "LabelHashSearchNoInsert start");

  tas_acquire( &pwLabelHashLocks[key] );
  entry = pwLabelHashTable[key];
  while (entry != NULL && entry->label != pwLabel )
    entry = entry->nextInHashChain;
  tas_release( &pwLabelHashLocks[key] );
  //bop_msg(4, "LabelHashSearchNoInsert end");
  return entry;
}

/* A label entry is inserted if the label is not found */
LabelHashEntry *LabelHashSearch(unsigned pwLabel) {
  LabelHashEntry *entry;
  int key = pwLabel % LABEL_HASH_TABLE_SIZE;

  bop_msg(4, "LabelHashSearch start");

  entry = LabelHashSearchNoInsert(pwLabel);

  /* obtain a new entry */
  if (entry==NULL) {
    /* be careful to ensure no deadlock caused by this? */
    SAFE(read(labelQueue[0], &entry, sizeof(void*)));
    //bop_msg(5, "entry is on page %d (%x)", ((unsigned) entry)>>PAGESIZEX, (int) entry);
    entry->label = pwLabel;
    entry->receiverSpec = -1;
    entry->senderSpec = -1;
    entry->firstPage = NULL;
    /* no need to reset the pipe under normal use */

    /* insert into the hash table */
    tas_acquire( &pwLabelHashLocks[key] );
    entry->nextInHashChain = pwLabelHashTable[key];
    pwLabelHashTable[key] = entry;
    tas_release( &pwLabelHashLocks[key] );
  }
  //bop_msg(4, "LabelHashSearch end");

  return entry;
}

static void LabelHashDelete(unsigned pwLabel) {
  LabelHashEntry *entry, *pred;
  int key = pwLabel % LABEL_HASH_TABLE_SIZE;

  bop_msg(4, "LabelHashDelete start");

  tas_acquire( &pwLabelHashLocks[key] );
  entry = pwLabelHashTable[key];
  if (entry->label == pwLabel) {
    pwLabelHashTable[key] = entry->nextInHashChain;
  } else {
    pred = entry;
    while (pred->nextInHashChain != NULL && 
	   pred->nextInHashChain->label != pwLabel ) 
      pred = pred->nextInHashChain;
    if (pred->nextInHashChain == NULL) {
      bop_msg(2, "trying to delete non-exist entry (%d) in post-wait hash table", pwLabel);
      return;
    }
    pred->nextInHashChain = pred->nextInHashChain->nextInHashChain;
    entry = pred->nextInHashChain;
  }
  entry->senderSpec = entry->receiverSpec = -1;
  //bop_msg(4, "returning label %x to label queue", entry);
  SAFE(write(labelQueue[1], &entry, sizeof(void*)));
  tas_release( &pwLabelHashLocks[key] );

}

void BOP_Wait(unsigned pwLabel)
{
  if (myStatus!=SPEC) return;  /* no need to wait */
  bop_msg(1, "Waiting for post %d", pwLabel);

  LabelHashEntry* entry = LabelHashSearch(pwLabel);
  entry->receiverSpec = mySpecOrder;
  int senderspec;
  SAFE( read( entry->pipe[0], &senderspec, sizeof(int)));

  /* Copy in the posted pages.  Wait if needed. */
  PageHashEntry* page = entry->firstPage;
  while (page!=NULL) {
    if (PageHasLabelConflict(page)) {
      /* we must deactivate this page */
      page->labelEntry = NULL;
      /* drop the page */
      DropPageFromPipe(entry->pipe[0], 1);
    }
    else {
      void *pageAddr = (void*) (page->pageid << PAGESIZEX);
      if (BOP_CheckAccess(pageAddr)) {
	/* another way is not to copy */
	bop_msg(1, "page %d has been accessed before the posted copy arrives", page->pageid);
	BOP_AbortSpec();
      }
      CopyPageFromPipe(page->pageid, entry->pipe[0], 1, 1);
      bop_msg(3, "received page %d from %d", page->pageid, senderspec);
      record_access((page->pageid) << PAGESIZEX ,WRITE); 
    }
    page = page->nextInPostChain;
  }

  /* Do not delete the label.  The meta data is needed at the end of
     the PPR for informed correctness checking. */
  /* That's all, folks. */
}

void BOP_Post(unsigned pwLabel, unsigned* pages, unsigned numPages)
{
  unsigned i;
  unsigned *pcopies = (unsigned *) malloc( numPages * sizeof( unsigned ) );

  if (!(myStatus==MAIN || myStatus==SPEC)) return;

  bop_msg(1, "Posting label %d with %d page(s).", pwLabel, numPages);

  /* Take out pages that have no write access */
  unsigned p;
  int lastValid = -1;
  for ( p = 0; p < numPages; p++ ) 
    if (BOP_CheckWriteAccess((void *) pages[p])) {
      pcopies[lastValid+1] = pages[p];
      lastValid += 1;
    }
  
  if ((unsigned) (lastValid+1) < numPages) {
    bop_msg(1, "%d pages have no modification and removed from the list.", 
	    numPages - lastValid - 1);
    numPages = lastValid + 1;
  }

  LabelHashEntry* entry = ActivatePostWait(pwLabel, pcopies, numPages);

  /* Send an integer so the receiverSpec can start waiting for the pipe
     even before the senderSpec has registered the label.*/
  SAFE( write( entry->pipe[1], &mySpecOrder, sizeof(int)));

  /* To not to stall the sender, we will fork a process if the number
     of pages to be sent is more than 3. */
  int pid = 1;
  if ( numPages > 2 ) {
    pid = fork();  /* child's pid is 0 */
    assert( pid != -1 );
  }
  
  if ( numPages <= 2 || (numPages > 2 && pid == 0) ) {
    for( i = 0; i < numPages; i++ ) {
      bop_msg(3, "posting page %d (%x, page start %x)", pcopies[i]>>PAGESIZEX, pcopies[i], (pcopies[i]>>PAGESIZEX)<<PAGESIZEX);
      PushPageToPipe( (pcopies[i]>>PAGESIZEX), entry->pipe[1], 1 );
    }
    if ( pid == 0 ) exit(0);  /* child sender quits */
  }

  free(pcopies);
}
