#ifndef H_BOP_POSTWAIT
#define H_BOP_POSTWAIT

typedef struct LabelHashEntry_s LabelHashEntry;
typedef struct PageHashEntry_s PageHashEntry;

struct LabelHashEntry_s
{
  unsigned label;
  int senderSpec, lastReceiver;  
  int pipe[2];  /* indicating whether the channel has been posted */
  LabelHashEntry* nextInHashChain;
  PageHashEntry* firstPage;
};

struct PageHashEntry_s
{
  unsigned pageid;  /* virtual address w/o the last PAGESIZEX bits */
  LabelHashEntry *labelEntry; /* NULL if the page is deactivated */
  PageHashEntry *nextInHashChain;
  PageHashEntry *nextInPostChain;  /* next page in the post sequence */
  char page[PAGESIZE];  /* the posted page */
};

void BOP_Post(unsigned label, unsigned* pages, unsigned numPages);
void BOP_Wait(unsigned label);
void BOP_PostWaitInit(void);
void BOP_PostWaitReset(void);

LabelHashEntry *LabelHashSearch(unsigned pwLabel);

/* Find the entry with pageid that has the desired posterSpec and
   waiterSpec, unless they are -1, which means any match. */
PageHashEntry *PageHashSearch(unsigned pageid, int posterSpec, 
			      int waiterSpec);

LabelHashEntry *ActivatePostWait(unsigned label, 
				 unsigned* pages, unsigned numPages);
void DeactivatePostWait(LabelHashEntry *labelEntry);

/* At the end of a spec, clear labels whose waiterSpec has ended. */
void RemoveStalePostWait(void);

void PostWaitInit(void);
void PostWaitReset(void);

#endif

/* To do:
   
   (Done.  See PageHashInsert and ActivatePostWait.) If multiple
   post-waits involve the same page, we must ensure the compatibility,
   i.e. no overlap unless it has the same posterSpec.
   
   We use a queue to manage page and label entries.  A stack would be
   more cache friendly.

*/
