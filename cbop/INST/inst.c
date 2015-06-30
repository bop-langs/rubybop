#include <stdlib.h>
#include <sys/mman.h>  /* for mprotect */
#include <errno.h>     /* for errno */

/* mspaces */
#include "external/malloc.h"
extern mspace map_space, serial_space, priv_heap;

#include "bop_api.h"
#include "bop_io.h"
#include "utils.h"

#include "commit.h"

#include "postwait/postwait.h"  // for combine_pw_sent_recv
extern map_t *pw_sent;

/* per task read/write maps, freed at the start of each task */
#include "bop_map.h"
map_t *read_map = NULL; 
map_t *write_map = NULL;
extern map_t *data_map, *priv_vars, *priv_objs;

// exported variables
Status volatile myStatus = SEQ;
int mySpecOrder = 0;  /* a serial number, increasing with the speculation, -1 for undy, 0 for main/control*/

int ppr_index = 0;     /* PPR id, unique for every task, e.g. if an understudy and a spec execute the same PPR instance, it has the same ppr_index.  */

int bopgroup;
int timer_pid;

/* largest group size allowed, can be adjusted at run time (will cause speculation group to close and restart) */
int curr_group_cap;    /* between 1 and MAX_GROUP_CAP */

// local variables
static int specPid;
static int undy_created = FALSE;  /* supporting hard abort */

static int undyWorkCount;  /* count to specDepth-1 before finish */
static int pprID;      /* programmer provided id of the PPR region */

/* Rotating BOP flags */
enum BOP_TYPE bopType = RotatingBOP;

/* Variables and data types for process coordination */
/* Signal data */
struct sigaction action;

/* Counters for undy/spec successes */
int numSpecWins = 0;
int numUndyWins = 0;
int numMainWins = 0;

/* For statistics */
Stats bop_stats = {0,0,0,0,0,0};

sigset_t sigMaskUsr1, sigMaskUsr2, sigMaskUsr12;

void SigTimerAlarmExit( int signo ){
  bop_msg( 1,"Program exits abnormally (%d).  Cleanse remaining processes. ", signo );
  kill(0, SIGKILL);
}

void sig_TimerTermExit( int signo ){
  assert( SIGTERM == signo );
  bop_msg(1,"Program exits normally.  Cleanse remaining processes. ");
  // signal( SIGTERM, SIG_IGN );
  // kill(0, SIGTERM);
  BOP_set_verbose( 0 );
  exit( 0 );
}

/* The arbitration channel for parallel-sequential race between
   speculation processes and understudy.

   All processes have the same SIGUSR1 and SIGUSR2 handlers.

   There are three synchronization tasks.  The first is to ensure Undy
   is created.  The second is to abort speculation processes when Undy
   completes.  The third is to signal Undy and wait for its surrender
   when speculation completes.

   The first is done by a UndyCreatedPipe, which is read by every
   commiting process.  The second is accomplished by spec process
   self-destructing upon receiving SIGUSR2.  The third is by Undy upon
   receiving SIGUSR1, sending a token through UndyConcedesPipe.  

   The use of the two pipes makes the speculative path slow.  It would
   be nice to avoid using the pipes.  */ 
void SigUsr1(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR1 == signo );
  assert( cntxt );

  if (siginfo->si_pid == getpid())
    /* Two cases: 
       Committing Spec seeing its signal, SIGUSR1.  No action. */
    return;

  if (myStatus != UNDY) return;

  bop_msg(1,"Understudy concedes the race (sender pid %d)", siginfo->si_pid);
  /* sending a symbolic value */
  post_undy_conceded(ppr_index);
  abort( );
}

void SigUsr2(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR2 == signo );
  assert( cntxt );
  if (siginfo->si_pid == getpid()) return;  /* Must be Undy */
  bop_msg(1,"Exit upon receiving SIGUSR2");
  abort( );
}

void BOP_set_group_size(int x) {
  if (myStatus!=SEQ) {
    bop_msg(1,"Cannot re-set group size in a parallel region.  Command ignored.");
    return;
  }
  assert(x>=1 && x<=MAX_GROUP_CAP);
  bop_msg(1,"Changing group size to %d",x);
  curr_group_cap = x;
  
}

void BOP_set_verbose(int x) {
  if ( x >=0 ) {
    VERBOSE = x;
    bop_msg( 1, "VERBOSE is set to %d\n", x);
  }
  else {
    VERBOSE = 0;
    bop_msg( 1, "%d is ignored by BOP_SetVerbose, must not be negative", x);
  }
}

int BOP_ppr_index( void ) {
  return ppr_index;
}

int BOP_spec_order( void ) {
  return mySpecOrder;
}

static void BOP_end(void);

void sig_SEG_H(int signo, siginfo_t *sginfo, ucontext_t *cntxt);

/* Initialize allocation map.  Installs the timer process. */
void __attribute__ ((constructor)) BOP_init(void) {
  static int init_done = 0;
  char *envItem;
  if(init_done) return;

  init_done = 1;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  bop_stats.bop_start_time = tv.tv_sec + (tv.tv_usec/1000000.0);

  /* malloc init must come before anything that requires mspace allocation */
  bop_malloc_init( 2 );
  private_space_init( );
  bop_io_init( MAX_GROUP_CAP );

  alloc_pw_meta_data( );
  create_switch_board( );
  create_exp_board( );

  VERBOSE = get_int_from_env("BOP_Verbose", 0, 6, 0);
  bop_msg(1,"Verbose level is %d (setenv BOP_Verbose to change)", VERBOSE);

  create_group_meta( );

  read_map = new_merge_map( );
  write_map = new_merge_map( );
  alloc_pw_meta_data( );

  /* install the seg fault handler */
  /* void sig_SEG_H(int signo, siginfo_t *sginfo, ucontext_t *cntxt); */
  sigaction(SIG_MEMORY_FAULT, NULL, &action);
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = (void *)sig_SEG_H;
  sigaction(SIG_MEMORY_FAULT, &action, NULL);
  
  /* Read environment variables: BOP_SpecDepth, BOP_Type */
  curr_group_cap = get_int_from_env("BOP_GroupSize", 1, MAX_GROUP_CAP, 2);
  
  envItem = getenv("BOP_Type");
  if (envItem!=NULL) {
    bop_msg(2,"Environment variable BOP_Type is defined as %s", envItem);
    if (!strcmp(envItem, "RotatingBOP"))
      bopType = RotatingBOP;
    else
      bopType = BatchBOP;
  }
  else bopType=BatchBOP;

  if (bopType==RotatingBOP && curr_group_cap==2) {
    bop_msg(1,"Rotating BOP is fruitless when group size is 2.  Use batch BOP.");
    bopType = BatchBOP;
  }

  /* init the speculation depth, can be reset later */
  bop_msg(1,"Speculation group size is %d (%d max, setenv BOP_GroupSize or call BOP_SetGroupSize to change).  Speculation type is %s.", curr_group_cap, MAX_GROUP_CAP, (bopType==RotatingBOP)? "RotatingBOP":"BatchBOP");
  

  /* register BOP_End at exit */
  if (atexit(BOP_end)) {
    perror("Failed to register exit-time BOP_end call");
  }

  /* initialization stops here if we are running BOP sequential */
#ifdef BOP_SEQ
  return;
#endif

  /* prepare related signals */
  signal( SIGINT, SigTimerAlarmExit );
  signal( SIGQUIT, SigTimerAlarmExit );
  signal( SIGUSR1, SIG_DFL );
  signal( SIGUSR2, SIG_DFL );

  /* Pre-made for signal blocking and unblocking */
  sigemptyset(&sigMaskUsr1); 
  sigaddset(&sigMaskUsr1, SIGUSR1);
  sigemptyset(&sigMaskUsr2);
  sigaddset(&sigMaskUsr2, SIGUSR2);
  sigemptyset(&sigMaskUsr12);
  sigaddset(&sigMaskUsr12, SIGUSR1);
  sigaddset(&sigMaskUsr12, SIGUSR2);
 
  timer_pid = getpid( );

  /* create the timer process */
  int fid = fork();
  // int fid = 0;
  
  switch (fid) {
  case -1:
    bop_error();
    return;
  case 0: /* the child, the new control */
    myStatus = SEQ;
    bop_msg(3,"SEQ starts");
    return;
  default: 
    /* setup SIGALRM */
    signal(SIGALRM, (void *) SigTimerAlarmExit);
    signal(SIGTERM, (void *) sig_TimerTermExit);
    /* timer must ignore other signals including SIGUSR by main/spec */
    bop_msg(1,"Timer starts.");
    while (1) 
      pause();  /* Timer waits for the program to end */
  }
}

inline static mem_range_t *map_check_add( map_t *map, memaddr_t base, size_t size ) {
  mem_range_t *range = map_contains( write_map, base );
  if ( range == NULL ) {
    map_add_range( map, base, size, ppr_index, NULL );
    return NULL;
  }

  /* Dlmalloc objects may have overlapping memory ranges since
     dlmalloc may put two small allocations on the same chunk.  */
  char changed = 0;
  if ( base < range->base ) {
    range->base = base;
    changed = 1;
  }
  memaddr_t upper;
  if ( (base + size ) > (range->base + range->size) ) {
    upper = base + size;
    range->size = upper - range->base;
    changed = 1;
  }
  if (changed)
    bop_msg(5,"an overlapping memory range is expanded to base %p for %d bytes", 
	    range->base, range->size);

  return range;
}

void BOP_record_write(void *addr, size_t size) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;
  memaddr_t base = (memaddr_t) addr;

  if  (map_check_add( write_map, base, size )==NULL) {
    bop_msg(5,"write address %p for %d bytes, pageN=%llu", base, size,
	  base>>PAGESIZEX);

    /* In byte granularity, should check whether the intersection is
     empty between the range and the complement of the read_map.  Here
     is a compromise that may miss an opportunity and lead to a spec
     failure that could be avoided.  On the other hand, the byte range
     should be a single number, so the test should be sufficient.  */
    if ( map_contains( read_map, base ) == NULL )
      check_postwait( base, size );
  }
  else { /* a recurring write */
    if ( pw_sent != NULL && pw_sent->sz > 0 )
      sender_conflict_check( base );
  }
}

void BOP_record_read(void* addr, size_t size) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;
  memaddr_t base = (memaddr_t) addr;
  if ( map_check_add( read_map, base, size ) != NULL ) return;
  bop_msg(6,"read address %p for %d bytes, pageN=%llu", base, size,
	  base>>PAGESIZEX);

  /* Do this before changing the permission to read only */
  check_postwait( base, size );

}

void BOP_record_write_page( void *addr ) {
  if ( map_contains( write_map, (memaddr_t) addr ) != NULL ) return;
  BOP_protect_range( (void*) PAGESTART(addr), PAGESIZE, PROT_WRITE );
  BOP_record_write( (void*) PAGESTART(addr), PAGESIZE );
}

void BOP_record_read_page( void *addr ) {
  BOP_record_read( (void*) PAGESTART(addr), PAGESIZE );
  BOP_protect_range( (void*) PAGESTART(addr), PAGESIZE, PROT_READ );
}

/* Spec or main gets a segmentation fault. */ 
void sig_SEG_H(int signo, siginfo_t *sginfo, ucontext_t *cntxt)
{
  memaddr_t faultAddr = (memaddr_t)(sginfo->si_addr);

  if ( ! map_contains( data_map, faultAddr ) ) {
     bop_msg(1, "fault addr %llx (page %lld) not in data map", faultAddr, faultAddr>>PAGESIZEX);
     map_inspect(1, data_map, "data_map");
     assert( 0 );
  }

  assert( SIG_MEMORY_FAULT == signo );
  if (sginfo->si_code == SEGV_ACCERR) {
    if (WRITEOPT(cntxt)) 
      BOP_record_write_page((void*) faultAddr);
    else
      BOP_record_read_page((void*) faultAddr);
  } else {
    bop_msg(1,"spec/main: unexpected segmentation fault at page %lld",
	      faultAddr>>PAGESIZEX );
    assert(0);
  }
}

void ppr_group_init(void) {
  /* setup bop group id.*/
  bopgroup = getpid();
  setpgid(0, bopgroup);

  bop_io_group_init( curr_group_cap );
  group_meta_data_init( curr_group_cap );
  bop_malloc_init( curr_group_cap );

  init_switch_board_pre_spec_group( );
  init_exp_board_pre_spec_group( );

  set_last_member( curr_group_cap - 1 );
  mySpecOrder = 0;
  undy_created = FALSE;
}

void ppr_group_fini(void) {
  group_meta_data_free( );
  bop_io_group_close( curr_group_cap );
  //for BOP_malloc
  BOP_reset();
}

void ppr_task_init(void) {
  ppr_index ++;
  bop_msg(1,"Begin ppr task %d (static id %d)", ppr_index, pprID);

  /* the GAP case is needed for BOP_SEQ */
  if (myStatus == SPEC || myStatus == GAP) mySpecOrder ++;

  /* clear local variables */
  priv_var_reset( );

  map_inspect(3, data_map, "data_map");

  map_union( read_map, write_map );
  map_intersect( read_map, data_map );
  BOP_protect_range_set( read_map, PROT_NONE );
  map_clear( read_map );
  map_clear( write_map );

  clear_local_pw_meta_data( );
  clear_local_exp_meta_data( );

  setpgid(0, bopgroup);
}

static void PostPPR_commit( void );
static void PostPPR_main( void );

static void ppr_map_inspect( map_t *map, char *map_nm ) {
  if ( map->sz <= 5 )
    map_inspect(1, map, map_nm);
  else {
    bop_msg(1, "%s has %d items", map_nm, map->sz);
    map_inspect(2, map, map_nm);
  }
}

void ppr_task_fini(void) {
  bop_msg(1,"End ppr task %d (static id %d)", ppr_index, pprID);
  ppr_map_inspect(write_map, "write_map");
  ppr_map_inspect(read_map, "read_map");

  if ( myStatus == MAIN )
    PostPPR_main( );
  else
    PostPPR_commit( );
}

/* The return value is 1 if this is spec */ 
int BOP_pre_ppr(int id)
{
  int fid;

  switch (myStatus) {
  case UNDY:
    return 0;
  case SEQ:
    pprID = id;
    ppr_group_init( );
    ppr_task_init( );
    myStatus = MAIN;
#ifdef BOP_SEQ
    BOP_protect_range_set(data_map, PROT_READ);
    return 0;
#endif

    /* two user signals for sequential-parallel race arbitration */
    sigaction(SIGUSR1, NULL, &action);
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = (void *) SigUsr1;
    sigaction(SIGUSR1, &action, NULL);
    action.sa_sigaction = (void *) SigUsr2;
    sigaction(SIGUSR2, &action, NULL);
    bop_msg (4, "SEQ finished signal setup.");
    /* NO BREAK! Start spec from main or start new spec from a spec */
  case GAP:  /* the ppr task was started earlier and now continues into the next PPR region */
    if (myStatus == GAP)
      myStatus = SPEC;

#ifdef BOP_SEQ
    pprID = id;  /* so to know when to finish */
    return 0;
#endif

    /* for batch BOP, stop spawning new tasks if specDepth is reached */
    if (mySpecOrder == curr_group_cap - 1 ) {
      pprID = id;  /* so to know when to finish */
      return 0;
    }
    bop_msg (4, "SPEC about to fork.");
    fid = fork();

    if (fid == -1) {
      bop_msg (1, "OS unable to fork more tasks" );
      BOP_abort_spec( "OS unable to fork more tasks" );
      if ( myStatus == MAIN) {
	ppr_group_fini( );
	myStatus = SEQ;
	return 0;
      }
      abort( );
    }

    if(fid > 0)
    {
      pprID = id;  /* so to know when to finish */

     /* main or senior spec continues */
      /* so main and undy will know specPid */
      specPid = fid;

      /* monitor only write access in main */
      if (myStatus==MAIN) BOP_protect_range_set(data_map, PROT_READ);
      return 0;
    }
    
    /* No spec child until I fork */
    specPid = 0;

    /* this is the child, the spec */
    myStatus = GAP;
    ppr_task_init( );
      
    bop_msg(2,"Spec %d created", mySpecOrder);
      
    if (mySpecOrder==1) {  /* set this up once */
      BOP_protect_range_set(data_map, PROT_NONE);
    }

    return 1;
    
  case MAIN:
  case SPEC:
    if (id == pprID) 
      bop_msg(4, "Nested begin PPR (ID %d inside PPR %d) ignored", id, pprID);
    break;
  default:
    assert(0);
  }
  return 0;
}

int BOP_PrePPR(int id) {
  return BOP_pre_ppr( id );
}


/* called by the last task in the group */
static void terminate_spec_group(void) {
  /* BOP_protect_range_set(PROT_READ|PROT_WRITE); // tradeoff page faults in gap to re-protecting at the next spec group */

  if (myStatus == MAIN) 
    BOP_hard_abort( "terminate_spec_group is called by MAIN" );

  bop_msg(2, "Speculation group finishes");

  /* can be done more efficiently but this way is simpler */
  copyout_write_map( mySpecOrder, write_map, NULL );
  copyout_written_pages( mySpecOrder );

  /* already passed correctness checking */
  /* wait for all preds to finish data updates */
  wait_data_done(mySpecOrder - 1);

  /* copy update pages */
  int pred;
  for (pred = 0 ; pred <= mySpecOrder ; pred ++ ) {
    copyin_update_pages( pred );

    bop_msg(4, "Copied modified pages from group member %d.", pred);
  }

  bop_msg(3,"Copied all updates to the last Spec task (%d)", mySpecOrder);

  bop_msg(1,"Last spec (%d) has succeeded", mySpecOrder);

#ifndef BOP_SEQ
  /* The last spec ensures the existence of undy because the first spec will 
     not finish and become main equivalent without receiving the UndyReady 
     signal. */
  int undy_pid = wait_undy_created( );
  bop_msg(3,"UndyCreated (pid %d) check passed", undy_pid);
    
  kill(undy_pid, SIGUSR1);  /* spec ready to go */

  // wait for acknowledgement from the understudy that spec won
  wait_undy_conceded( );
  bop_msg(2,"UndyConcedes signal received.");

  /* kill other spec */
   kill(-bopgroup, SIGUSR2);
  /*kill the MAIN if current process wasn't*/
   if (specPid!=0)  
   kill(specPid, SIGKILL);
#endif
    
  BOP_DumpGroupOutput();  /* it needs myStatus still be SPEC */

  ppr_group_fini( );

  bop_msg(2,"Spec wins");

  numSpecWins += mySpecOrder;

  numMainWins ++;

  myStatus = SEQ;
  bop_msg(2,"Changed status to control");

  /* don't move it earlier since undy_conceded channel is still in use */
  /* ppr_group_init( ); */
}

/* Commit protocol for MAIN-SPEC and SPEC-SPEC commits */ 
static void PostPPR_commit(void) {
  fflush( NULL );

  if (myStatus == SPEC ) { /* not the first task */
    /* wait for the write map of preds and check for conflicts */
    wait_write_map_done(mySpecOrder - 1);
    int passed = check_correctness( mySpecOrder, read_map, write_map );
    if (! passed) 
      BOP_abort_spec( "correctness checking failed" );  /* exit if not passed */
    else {
      bop_msg(2,"Spec %d incurred no conflicts",  mySpecOrder);
      post_check_done( mySpecOrder, ppr_index );
    }
  }
  else if (myStatus == MAIN)
    assert( check_correctness( mySpecOrder, read_map, write_map ) );
  
  if (is_last_member( mySpecOrder )) 
    return terminate_spec_group( );

  /* post my write map for successors */
  map_t *pw_pages = combine_pw_sent_recv( );
  copyout_write_map( mySpecOrder, write_map, pw_pages );  /* setting up data space but not copying data */
  post_write_map_done( mySpecOrder, ppr_index );

  if (is_last_member( mySpecOrder )) 
    return terminate_spec_group( );
  
  /* post my updates (in parallel with others) */
  copyout_written_pages( mySpecOrder );
  bop_msg(4, "Copied modified pages");

  if (myStatus == SPEC) {
    wait_data_done( mySpecOrder - 1 );
    /* to do: remove the next 2 lines */
    post_data_done( mySpecOrder - 1 ,ppr_index - 1);
    if (! is_last_member( mySpecOrder )) 
      post_data_done( mySpecOrder, ppr_index );
  }
  else
    post_data_done( mySpecOrder, ppr_index );

#if defined(BOP_SEQ)
  return;  /* cannot call wait_check_done in the sequential mode */
#endif

  if (! is_last_member( mySpecOrder))
    wait_check_done( mySpecOrder + 1 );  /* last check for early termination */

  if (is_last_member( mySpecOrder ))
    return terminate_spec_group( );
    
  //for BOP_malloc
  BOP_reset();

  abort( );
}

static void PostPPR_main(void) {
  numMainWins ++;

#ifdef BOP_SEQ
  return PostPPR_commit( );
#endif

  fflush(NULL);
  undy_created = TRUE;

  /* start undy */  
  int fid = fork();
  switch (fid) {
  case -1: 
    bop_msg (1, "OS unable to fork more tasks" );
    ppr_group_fini( );
    myStatus = SEQ;
    break;
  case 0: /* the child, the understudy */
    /* status */
    myStatus = UNDY;
    setpgid(0, bopgroup);
    mySpecOrder = -1;
    bop_msg(1,"Understudy starts");

    // open page protection
    BOP_protect_range_set(data_map, PROT_READ|PROT_WRITE);
  
    // set the segfault handler back to the default
    signal( SIG_MEMORY_FAULT, SIG_DFL );

    /* tell spec that undy is ready */
    post_undy_created( ppr_index );

    /* uncomment to make undy slow */
    //sleep(20);
    break;
    
  default: /* main continues */
    PostPPR_commit();
    break;
  }
}

// BOP_PostPPR for the understudy
static void PostPPR_undy(void) {

  undyWorkCount ++;
  ppr_index ++;

  if (!get_hard_abort_flag() && undyWorkCount < (curr_group_cap - 1)) {
    bop_msg(1,"Undy reaches EndPPR %d times", undyWorkCount);
    return;
  }
  /* Kill undy here if we want its overhead but don't want it succeed */
  /* exit(0); */

  /* Ladies and Gent: This is the finish line.  If understudy lives to block 
     off SIGUSR2 (without being aborted by it before), then it wins the race 
     (and thumb down for parallelism).*/
  /* turn off sigUsr2 for undy until myStatus is changed. */
  sigprocmask(SIG_BLOCK, &sigMaskUsr1, NULL);
  bop_msg(1,"Understudy finishes and wins the race");
  // indicate the success of the understudy
  kill(-bopgroup, SIGUSR2);
  /* the first spec does not take UndySucceeds from SIGUSR2 */
  kill(specPid, SIGKILL);
  // BOP_DumpGroupOutput();  /* it needs myStatus still be UNDY */

  ppr_group_fini( );

  myStatus=SEQ;
  numUndyWins += undyWorkCount;
  undyWorkCount = 0;

/* install the seg fault handler */
  void sig_SEG_H(int signo, siginfo_t *sginfo, ucontext_t *cntxt);
  sigaction(SIG_MEMORY_FAULT, NULL, &action);
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = (void *)sig_SEG_H;
  sigaction(SIG_MEMORY_FAULT, &action, NULL);
  
  sigprocmask(SIG_UNBLOCK, &sigMaskUsr1, NULL);
}

void BOP_post_ppr(int id) {
  switch (myStatus) {
  case SEQ:
  case GAP:
    bop_msg(4, "End PPR (ID %d) outside a PPR task.  Ignored.", id);
    return;
  case MAIN:
  case SPEC:
    /* in case of a hard abort */
    if (get_hard_abort_flag( )) abort( );

    if (id != pprID) {
      bop_msg(4, "Nested end PPR (ID %d inside PPR %d) ignored", id, pprID);
      return;
    }

    ppr_task_fini( );  /* call PostPPR_commit( ); */
#ifdef BOP_SEQ
    if ( myStatus != SEQ) myStatus = GAP;
    ppr_task_init();
    if (mySpecOrder==1) {  /* set this up once */
      BOP_protect_range_set(data_map, PROT_NONE);
    }
#endif
    return;
  case UNDY:
    return PostPPR_undy();
  default:
    assert(0);
  }
  return;
}

void BOP_PostPPR(int id) {
  return BOP_post_ppr( id );
}

static void BOP_end(void) {
  FILE *resultFile;
  struct timeval tv;

  bop_msg(1, "An exit is reached");

  switch (myStatus) {
  case SPEC:
  case GAP:
    BOP_abort_spec( "SPEC or GAP reached an exit" );  /* will abort */
#ifdef BOP_SEQ
    BOP_DumpGroupOutput(); 
    exit( 0 );
#endif

  case UNDY:
    sigprocmask(SIG_BLOCK, &sigMaskUsr1, NULL);
	// indicate the success of the understudy
   kill(-bopgroup, SIGUSR2);
  /* the first spec does not take UndySucceeds from SIGUSR2 */
   kill(specPid, SIGKILL);
  
    BOP_DumpGroupOutput(); 
    if( -1 == kill(timer_pid, SIGTERM) ) perror("failed to kill the timer");
    exit( 0 );

  case MAIN: case SEQ:
    gettimeofday(&tv, NULL);
    double bop_end_time = tv.tv_sec+(tv.tv_usec/1000000.0);

    bop_msg( 1, "\n\n***BOP Report***\n The total run time is %.2lf seconds.  There were %d ppr tasks, %d executed speculatively and %d non-speculatively.  A total of %llu pages in %d range(s) were monitored, including %d global variable(s) and %d malloc object(s). %llu page(s) were not monitored (but re-initialized with each task).  %d page(s) were modified by speculative task(s) and copied out. The speculation group size was capped at %d.\n\n",
	    bop_end_time - bop_stats.bop_start_time, ppr_index,
	    numSpecWins, numUndyWins + undyWorkCount + numMainWins, 
	     map_size_in_bytes(data_map)>>PAGESIZEX, (int) data_map->sz,
	    bop_stats.num_global_var, bop_stats.num_malloc,
	     (map_size_in_bytes(priv_vars) + map_size_in_bytes(priv_objs))>>PAGESIZEX,
	    bop_stats.pages_pushed,
	    curr_group_cap);

#if defined(BOP_SEQ) 
    return;
#endif

    /* global (normal) exit, kill the timer process */
    if( -1 == kill(timer_pid, SIGTERM) ) perror("failed to kill the timer");
    break;
  default:
    assert(0);
  }
}

void BOP_hard_abort( char *msg ) {
#if defined(BOP_SEQ) 
   return;
#endif

  /* allows only 2 cases, instead of 3 as in BOP_AbortSpec */
  if (myStatus == SEQ || myStatus == UNDY)
     return;

  if (myStatus == MAIN && ! undy_created) {
    /* hard abort in MAIN before it finishes */
	
	kill(-bopgroup, SIGUSR2);
  	/* the first spec does not take UndySucceeds from SIGUSR2 */
  	kill(specPid, SIGKILL);
	BOP_DumpGroupOutput();
	ppr_group_fini( );
	myStatus = SEQ;
    BOP_protect_range_set(data_map, PROT_READ|PROT_WRITE);
    bop_msg(1, "Hard abort because %s.  Main changes status to SEQ.", msg);
    return;
  }

  bop_msg(1, "Hard abort because %s. Speculation group will terminate.", msg);
  set_hard_abort_flag( );
  abort( );  /* die silently */
}
 
void BOP_HardAbort(void) {
  BOP_hard_abort( "(no reason given)" );
}

/* Called by a speculation process in case of error. 
   FailMe: the current spec has failed.
   FailNextSpec: the current spec is the last to succeed.
*/
void BOP_abort_spec( char *msg ) {
  if (myStatus == MAIN || myStatus == SEQ || myStatus == UNDY)
    return;
  bop_msg(1, "Abort speculation because %s", msg);
  set_last_member(mySpecOrder - 1);
  post_check_done(mySpecOrder, ppr_index);
#if defined(BOP_SEQ) 
  return;
#endif
  abort( );  /* die silently */
}

void BOP_AbortSpec(void) {
  BOP_abort_spec( "(old interface, no reason given)");
}

void BOP_abort_next_spec( char *msg ) {
  bop_msg(1,"Abort the next spec because %s", msg);
  set_last_member(mySpecOrder - 1);
  post_check_done(mySpecOrder, ppr_index);
#if defined(BOP_SEQ)
  return;
#endif
  if (specPid!=0)  /* See if the next spec exists */
    kill(specPid, SIGKILL);
  /* to do: is it possible the next spec just hang or even commit? */
}

/* Returns true/false depending whether the page has been read/written
   as recorded by the access map. */
char BOP_check_read(void* addr) {
  return map_contains(read_map, (memaddr_t) addr) != NULL;
}

char BOP_check_write(void* addr) {
  return map_contains(write_map, (memaddr_t) addr) != NULL;
}

char BOP_check_access(void* addr) {
  return BOP_check_read(addr) || BOP_check_write(addr);
}


