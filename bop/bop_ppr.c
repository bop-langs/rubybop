#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <semaphore.h> //BOP_msg locking
#include <fcntl.h>

#include <unistd.h>
//#include <sys/siginfo.h>
#include <ucontext.h>
#include <execinfo.h>

#include "bop_api.h"
#include "bop_ports.h"
#include "bop_ppr_sync.h"
#include "utils.h"


#define UNDY_SLEEP(s) sleep(s) //NOTE remember to get rid of this

#ifndef NDEBUG
#define VISUALIZE(s)
//#define UNDY_SLEEP(s)
#else
#define VISUALIZE(s) bop_msg(1,s);

#endif

#define PIPE(x) if(pipe((x)) == -1) { bop_msg(1, "ERROR making the pipe"); abort();}
#define WRITE(a, b, c) if(write((a), (b), (c)) == -1) {bop_msg(1, "ERROR: pipe write"); abort();}
#define READ(a, b, c) if(read((a), (b), (c)) == -1) {bop_msg(1, "ERROR: pipe read"); abort();}

extern bop_port_t bop_io_port;
extern bop_port_t bop_merge_port;
extern bop_port_t postwait_port;
extern bop_port_t bop_ordered_port;
extern bop_port_t bop_alloc_port;

volatile task_status_t task_status = SEQ;
volatile ppr_pos_t ppr_pos = GAP;
bop_mode_t bop_mode = PARALLEL;
int spec_order = 0;
int ppr_index = 0;
static int ppr_static_id;

stats_t bop_stats = { 0 };

static int bopgroup;
static int monitor_group = 0; //the process group that PPR tasks are using

static void _ppr_group_init( void ) {
  bop_msg( 3, "task group starts (gs %d)", BOP_get_group_size() );

  /* setup bop group id.*/
  bopgroup = getpid();
  assert( getpgrp() == -monitor_group );
  spec_order = 0;
  ppr_sync_data_reset( );
  ppr_group_init( );
  task_status = MAIN;  // at the end so monitoring is disabled until this point
}

/* Called at the ppr start of a main task and the end of the prior ppr
   of a spec task. */
static void _ppr_task_init( void ) {
  switch (task_status) {
  case MAIN:
    assert( spec_order == 0);
    break;
  case SPEC:
    spec_order += 1;
    break;
  case UNDY:
  case SEQ:
    assert(0);
  }

  bop_msg( 2, "ppr task %d (%d total) starts. pgrp = %d", spec_order, BOP_get_group_size(), getpgrp() );

  //assert( setpgid(0, bopgroup)==0 );
  assert (getpgrp() == -monitor_group);
  ppr_task_init( );
}

int _BOP_ppr_begin(int id) {
  ppr_pos_t old_pos = ppr_pos;
  ppr_pos = PPR;
  ppr_index ++;
  VISUALIZE("!");

  switch (task_status) {
  case UNDY:
    return 0;
  case SEQ:
    _ppr_group_init( );

    /* NO BREAK. either the ppr task was started by SPEC earlier or
       now it is SPEC continuing into the next PPR region */
  case SPEC:
    if (task_status == SPEC && old_pos != GAP) {
      bop_msg(2, "Nested begin PPR (region %d inside region %d) ignored", id, ppr_static_id);
      return 0;
    }

    ppr_static_id = id;

    /* no spawn if sequential or reaching group size */
    if (bop_mode == SERIAL || spec_order >= partial_group_get_size( ) - 1) {
      if (bop_mode == SERIAL) _ppr_task_init( );
      return 0;
    }

    int fid = fork( );

    if (fid == -1) {
      bop_msg (2, "OS unable to fork more tasks" );
      if ( task_status == MAIN) {
      	task_status = SEQ;
      	bop_mode = SERIAL;
      }
      else
	     partial_group_set_size( spec_order + 1 );

      return 0;
    }

    if (fid > 0) { /* main or senior spec continues */
      if (task_status == MAIN) _ppr_task_init( );
      return 0;
    }

    /* new spec task */
    task_status = SPEC;
    ppr_pos = GAP;
    _ppr_task_init( );
    return 1;

  case MAIN:
    bop_msg(2, "Nested begin PPR (region %d inside region %d) ignored", id, ppr_static_id);
    return 0;

  default:
    assert(0);
  }
  return 0;
}

/* Not supported because of too many cases we have to worry depending
   on when this called.  Also there is no need for this call.
   BOP_abort_spec should suffice.  In any case, sending SIGUSR2 will
   break the system unless UNDY is running. */

// void BOP_abort_spec_group( char *msg ) {
//  bop_msg(2, "Abort all speculation because %s", msg );
//  kill( 0, SIGUSR2 );
// }

void BOP_abort_spec( const char *msg ) {
  if (task_status == SEQ
      || task_status == UNDY || bop_mode == SERIAL)
    return;

  if (task_status == MAIN)  { /* non-mergeable actions have happened */
    if ( partial_group_get_size() > 1 ) {
      bop_msg(2, "Abort main speculation because %s", msg);
      partial_group_set_size( 1 );
    }
  }
  else {
    bop_msg(2, "Abort alt speculation because %s", msg);
    partial_group_set_size( spec_order );
    signal_commit_done( );
    abort( );  /* die silently */
  }
}

void BOP_abort_next_spec( char *msg ) {
  if (task_status == SEQ
      || task_status == UNDY || bop_mode == SERIAL)
    return;

  bop_msg(2, "Abort next speculation because %s", msg);
  if (task_status == MAIN)  /* non-mergeable actions have happened */
    partial_group_set_size( 1 );
  else
    partial_group_set_size( spec_order + 1 );
}

static int undy_ppr_count;

void post_ppr_undy( void ) {
  undy_ppr_count ++;

  if (undy_ppr_count < partial_group_get_size() - 1) {
    bop_msg(3,"Undy reaches EndPPR %d times", undy_ppr_count);
    return;
  }

  /* Ladies and Gent: This is the finish line.  If understudy lives to block
     off SIGUSR2 (without being aborted by it before), then it wins the race
     (and thumb down for parallelism).*/
  bop_msg(3,"Understudy finishes and wins the race");
  UNDY_SLEEP(10);
  // indicate the success of the understudy
  kill(0, SIGUSR2);
  kill(-monitor_group, SIGUSR1); //main requires a special signal?

  undy_succ_fini( );

  task_status = SEQ;
  ppr_pos = GAP;
  bop_stats.num_by_undy += undy_ppr_count;
}

/* Return true if it is UNDY (or SEQ in the rare case). */
int spawn_undy( void ) {

  int fid = fork( );
  switch( fid ) {
  case -1:
    bop_msg(3,"OS cannot fork more process.");
    /* Turn main into the understudy and effectively abort all
       speculation (by not committing).*/
    task_status = SEQ;
    bop_mode = SERIAL;
    return TRUE;
  case 0:
    task_status = UNDY;
    ppr_pos = GAP;
    spec_order = -1;
    //assert( setpgid(0, bopgroup) == 0 );
    assert (getpgrp() == -monitor_group);
    bop_msg(3,"Understudy starts pid %d pgrp %d", getpid(), getpgrp());

    signal_undy_created( fid );

    bop_stats.num_by_main += 1;
    undy_ppr_count = 0;

    undy_init( );

    return TRUE;
  default:
    return FALSE;
  }
}

/* It won't return if not correct. */
static void _ppr_check_correctness( void ) {
  if ( task_status != MAIN )
    wait_prior_check_done( );

  int passed = ppr_check_correctness( );
  if ( task_status == MAIN ) assert( passed );

  signal_check_done( passed );

  if ( !passed )
    BOP_abort_spec( "correctness check failed.");
}

/* Called when spec group is succeeding. */
void _task_group_commit( void ) {
  if (task_status == MAIN) {
    if (bop_mode == SERIAL) {
      task_group_commit( );
      task_group_succ_fini( );
      bop_stats.num_by_main += 1;
      task_status = SEQ;
      return;
    }
    else abort( );  /* UNDY has run ahead */
  }

  wait_group_commit_done( );
  task_group_commit( );
  signal_commit_done( );

  bop_msg(2,"The task group (0 to %d) has succeeded", spec_order);

  if (bop_mode == PARALLEL) {
    wait_undy_created( );
    kill( 0, SIGUSR1 );  /* not just get_undy_pid( ) */
    wait_undy_conceded( );
  }

  task_group_succ_fini( );

  bop_msg(5,"Spec wins");

  bop_stats.num_by_spec += spec_order;
  bop_stats.num_by_main += 1;

  task_status = SEQ;
}

/* MAIN-SPEC and SPEC-SPEC commits */
void ppr_task_commit( void ) {
  bop_msg( 4, "ppr task commits" );

  /* Earlier spec aborted further tasks */
  if ( spec_order >= partial_group_get_size( ) ) {
  	  bop_msg( 4, "ppr task outside group size" );
  	  abort( );
  }

  _ppr_check_correctness( );

  if ( spec_order < partial_group_get_size()-1 ) {
    /* not the last task */
    data_commit( );

    signal_commit_done( );
    if (bop_mode == SERIAL) return;
    wait_next_commit_done( );
    bop_msg( 4, "the next ppr has finished" );
  }

  /* check again in case the next ppr fails after my last check */
  if ( spec_order == partial_group_get_size()-1 ) {
    bop_msg( 3, "task group commits." );
    _task_group_commit( );
    return;
  }

  abort( );
}

void _BOP_ppr_end(int id) {
  VISUALIZE("?");
  if (ppr_pos == GAP || ppr_static_id != id)  {
    bop_msg(4, "Unmatched end PPR (region %d in/after region %d) ignored", id, ppr_static_id);
    return;
  }

  switch (task_status) {
  case SEQ:
    return;
  case MAIN:
    if ( bop_mode != SERIAL && spawn_undy( ) )
      return; /* UNDY */
    /* still MAIN, fall through */
  case SPEC:
    ppr_task_commit( );
    break;
  case UNDY:
    post_ppr_undy( );
    ppr_pos = GAP;
    return;
  default:
    assert(0);
  }
}

/* The arbitration channel for parallel-sequential race between
   speculation processes and understudy.

   There are two needs for "poking" a task when it runs.  The first is
   to abort speculation processes when Undy completes.  The second is
   to signal Undy and wait for its surrender when speculation
   completes.  The first is accomplisned by Undy sending SIGUSR1 to
   SPEC group.  The second is by the succeeding SPEC sending SIGUSR2
   to Undy.

   All processes have the same SIGUSR1 and SIGUSR2 handlers.

*/
void SigUsr1(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR1 == signo );

  if (task_status == UNDY) {
    bop_msg(3,"Understudy concedes the race (sender pid %d)", siginfo->si_pid);
    signal_undy_conceded( );
    abort( );
  }
  if (task_status == SPEC || task_status == MAIN) {
    if ( spec_order == partial_group_get_size() - 1 ) return;
    bop_msg(3,"Quiting after the last spec succeeds", siginfo->si_pid);
    abort( );
  }
  // assert ( 0 );
}

/* Used to remove all SPEC tasks and MAIN when UNDY wins, when a spec group
   finishes partially, or BOP_abort_spec_group is called. */
void SigUsr2(int signo, siginfo_t *siginfo, ucontext_t *cntxt) {
  assert( SIGUSR2 == signo );
  assert( cntxt );
  if (task_status == SPEC || task_status == MAIN) {
    bop_msg(3,"Exit upon receiving SIGUSR2");
    abort( );
  }
}

void SigBopExit( int signo ){
  bop_msg( 3,"Program exits (%d).  Cleanse remaining processes. ", signo );
  exit(0);
}
/* Initial process heads into this code before forking.
 *
 * Waits until all children have exited before exiting itself.
 */

static void wait_process() {
  int status;
  pid_t child;
  bop_msg(3, "Monitoring waiting to receive pgid");
  while(monitor_group == 0){
    nop();
  }
  bop_msg(3, "Monitoring pg %d from pid %d (group %d)", monitor_group, getpid(), getpgrp());
  while (((child = waitpid(monitor_group, &status, WUNTRACED)) != -1)) {
    if (WIFSIGNALED(status)) {
      bop_msg(1, "Child %d was terminated by signal %d", child, WTERMSIG(status));
    }
    //sleep(3); //FIXME this is a dirty hack that should be avoided
  }

  /* We expect to get ECHILD, others are an error */
  if (errno != ECHILD) {
    perror("wait in initial process");
    exit(-1);
  }
  bop_msg(1, "Monitoring process ending");
  msg_destroy();
  exit(0);
}

static void BOP_fini(void);

extern int bop_verbose;
extern int errno;
char *strerror(int errnum);
/* Initialize allocation map.  Insta4lls the timer process. */
void __attribute__ ((constructor)) BOP_init(void) {

  /* Read environment variables: BOP_GroupSize, BOP_Verbose */
  bop_verbose = get_int_from_env("BOP_Verbose", 0, 6, 0);


  int g = get_int_from_env("BOP_GroupSize", 1, 100, 2);
  BOP_set_group_size( g );
  bop_mode = g<2? SERIAL: PARALLEL;
  msg_init();
  /* malloc init must come before anything that requires mspace allocation */
  // bop_malloc_init( 2 );

  /* start the time */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  bop_stats.start_time = tv.tv_sec + (tv.tv_usec/1000000.0);


  /* setting up the timing process and initialize the SEQ task */
  if (bop_mode != SERIAL) {
    /* create a process to allow the use of time command */
    int pipe_fd[2]; //r/w file discribtors
    PIPE(pipe_fd);
    int fd = fork();

    switch (fd) {
    case -1:
      perror("fork() for timer process");
      exit(-1);
    case 0:
      /* Child process continues after switch */
      //We must first set up process group
      close(pipe_fd[0]); //close read end
      //set up a new group
      if (setpgid(0, 0) != 0) {
        perror("setpgid");
        exit(-1);
      }
      monitor_group = -getpid(); //negative-> work on process group
      WRITE(pipe_fd[1], &monitor_group, sizeof(monitor_group));
      close(pipe_fd[1]);
      break;
    default:
      close(pipe_fd[1]);   //close write end
      READ(pipe_fd[0], &monitor_group, sizeof(monitor_group));
      close(pipe_fd[0]);
      wait_process();
      abort(); /* Should never get here */
    }

    /* the child process continues */

    /* Ensure we are always in our own process group. */
    /*if (setpgid(0, 0) != 0) {
      perror("setpgid");
      exit(-1);
    } */

    /* register BOP_End at exit */
    if (atexit(BOP_fini)) {
      perror("Failed to register exit-time BOP_end call");
    }

    /* two user signals for sequential-parallel race arbitration, block
     for SIGUSR2 initially */
    struct sigaction action;
    sigaction(SIGUSR1, NULL, &action);
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = (void *) SigUsr1;
    sigaction(SIGUSR1, &action, NULL);
    action.sa_sigaction = (void *) SigUsr2;
    sigaction(SIGUSR2, &action, NULL);

    /*
    sigset_t mask;
    sigemptyset( &mask );
    sigaddset( &mask, SIGUSR2 );
    sigprocmask( SIG_BLOCK, &mask, NULL );
    */
  }
  assert(getpgrp() == -monitor_group);
  task_status = SEQ;

  /* prepare related signals */
  signal( SIGINT, SigBopExit );
  signal( SIGQUIT, SigBopExit );
  signal( SIGTERM, SigBopExit );

  /* Load ports */
  register_port(&bop_merge_port, "Copy-n-merge Port");
  register_port(&postwait_port, "Post-wait Port");
  register_port(&bop_ordered_port, "Bop Ordered Port");
  register_port(&bop_io_port, "I/O Port");
  register_port(&bop_alloc_port, "Malloc Port");
}

static void BOP_fini(void) {

  bop_msg(3, "An exit is reached");

  switch (task_status) {
  case SPEC:
    BOP_abort_spec( "SPEC reached an exit");  /* will abort */
    kill(0, SIGUSR2); //send SIGUSR to spec group -> own group

    if (bop_mode == SERIAL) {
      data_commit( );
      partial_group_set_size( 1 );
      task_group_commit( );
      task_group_succ_fini( );
    }
    exit( 0 );

  case UNDY:
    kill(0, SIGUSR2);
    kill(-monitor_group, SIGUSR1); //main requires a special signal
    undy_succ_fini( );
    bop_stats.num_by_undy += undy_ppr_count;
    break;

  case MAIN:
  case SEQ:
    break;

  default:
    assert(0); //should never get here
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  double bop_end_time = tv.tv_sec+(tv.tv_usec/1000000.0);

  bop_msg( 1, "\n***BOP Report***\n The total run time is %.2lf seconds.  There were %d ppr tasks, %d executed speculatively and %d non-speculatively (%d by main and %d by understudy).\n",
	   bop_end_time - bop_stats.start_time, ppr_index,
	   bop_stats.num_by_spec, bop_stats.num_by_undy + bop_stats.num_by_main, bop_stats.num_by_main, bop_stats.num_by_undy);

  bop_msg( 3, "A total of %d bytes are copied during the commit and %d bytes are posted during parallel execution. The speculation group size is %d.\n\n",
	   bop_stats.data_copied, bop_stats.data_posted,
	   BOP_get_group_size( ));
}
