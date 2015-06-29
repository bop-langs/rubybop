#include <stdlib.h> /* for NULL */
#include "bop_api.h" // for TRUE
#include "atomic.h"  // for locking
#include "external/malloc.h" // for mspace
#include "bop_ppr_sync.h"

extern int spec_order;
extern int task_status;

typedef struct {
  /* steps of commit */
  bop_lock_t spec_checked;
  int spec_correct;
  bop_lock_t data_committed;

  char space[64];  /* avoid cache block sharing */
} sync_status_t;

typedef struct {
  bop_lock_t lock;

  int alloc_size; 
  int group_size;
  int partial_size; /* used during a parallel run to support partial
		       completion */

  sync_status_t *sync_status;

  bop_lock_t undy_created;
  pid_t undy_pid;
  bop_lock_t undy_conceded;
} sync_data_t;
  
static mspace sync_space = NULL;
static sync_data_t *sync_data = NULL;

void partial_group_set_size( int psize ) {
  assert( psize >= 1 && psize <= 100 );
     
  if (sync_data->partial_size > psize) {
    bop_lock_acquire( & sync_data->lock );
    sync_data->partial_size = psize;
    bop_lock_release( & sync_data->lock );
  }
}

int partial_group_get_size( void ) {
  return sync_data->partial_size;
}

int ppr_group_get_size( void ) {
  if ( sync_data == NULL ) return 1;
  else return sync_data->group_size;
}

void ppr_sync_data_reset( void ) {
  int i;
  for (i = 0; i < sync_data->alloc_size; i ++ ) {
    sync_status_t *sync_status = & sync_data->sync_status[i];
    bop_lock_clear(   & sync_status->spec_checked );
    bop_lock_acquire( & sync_status->spec_checked );
    bop_lock_clear(   & sync_status->data_committed );
    bop_lock_acquire( & sync_status->data_committed );
  }

  bop_lock_clear(   & sync_data->undy_created );
  bop_lock_acquire( & sync_data->undy_created );
  bop_lock_clear(   & sync_data->undy_conceded );
  bop_lock_acquire( & sync_data->undy_conceded );

  bop_lock_clear( & sync_data->lock );

  sync_data->partial_size = sync_data->group_size;
}

/* Called before a parallel execution to initialize the sync meta data
   and during a parallel execution to signal early termination. */
void ppr_group_set_size( int gsize ) {
  assert( gsize >= 1 && gsize <= 100 );

  if ( task_status == SEQ )
    bop_msg( 3, "setting group size to %d", gsize );
  else {
    bop_msg( 3, "setting group size is not allowed unless by SEQ" );
    return;
  }

  if (sync_data == NULL) {
    /* allocate sync space */
    char is_shared_mem = TRUE;  /* true */
    char use_lock = TRUE;
    size_t init_size = 20000; /* 20KB init size */
    sync_space = create_mspace( init_size, use_lock, is_shared_mem );

    sync_data = (sync_data_t *)
      mspace_calloc( sync_space, 1, sizeof( sync_data_t ) );

    sync_data->sync_status = 
      mspace_calloc( sync_space, gsize, sizeof( sync_status_t ) );
    sync_data->alloc_size = gsize;
  }
  
  /* Initialization */
  if ( sync_data->group_size < gsize ) {
    sync_data->sync_status = (sync_status_t *)
      mspace_realloc( sync_space, sync_data->sync_status, 
		      gsize*sizeof( sync_status_t ) );
    sync_data->alloc_size = gsize;
  }
  sync_data->group_size = gsize;
  ppr_sync_data_reset( );
}

int BOP_get_group_size( void ) {
	return ppr_group_get_size();
}

void BOP_set_group_size( int size ) {
   ppr_group_set_size ( size );
}

void signal_check_done( int passed ) {
  sync_data->sync_status[ spec_order ].spec_correct = passed;
  bop_lock_release( & sync_data->sync_status[ spec_order ].spec_checked );
}

void signal_commit_done( void ) {
  bop_lock_release( & sync_data->sync_status[ spec_order ].data_committed );
  bop_msg( 4, "signal commit (%d, partial group_size %d)", spec_order, partial_group_get_size() );
}

void wait_prior_check_done( void ) {
  assert( spec_order >= 1 );
  bop_lock_acquire( & sync_data->sync_status[ spec_order - 1 ].spec_checked );
}

void wait_next_commit_done( void ) {
  bop_msg( 4, "wait commit (%d, partial_group_size %d)", spec_order+1, partial_group_get_size() );
  bop_lock_acquire( & sync_data->sync_status[ spec_order+1 ].data_committed );
  bop_lock_release( & sync_data->sync_status[ spec_order+1 ].data_committed );
}

void wait_group_commit_done( void ) {
  int i;
  bop_msg( 4, "wait group commit (%d, partial_group_size %d)", spec_order, partial_group_get_size() );
  for ( i = 0; i < partial_group_get_size() - 1; i ++ ) 
    bop_lock_acquire( & sync_data->sync_status[ i ].data_committed );
}

void signal_undy_created( int pid ) {
  sync_data->undy_pid = pid;
  bop_lock_release( & sync_data->undy_created );
}

int get_undy_pid( void ) {
  return sync_data->undy_pid;
}

void signal_undy_conceded( void ) {
  bop_lock_release( & sync_data->undy_conceded );
}

void wait_undy_created( void ) {
  bop_lock_acquire( & sync_data->undy_created );
}

void wait_undy_conceded( void ) {
  bop_lock_acquire( & sync_data->undy_conceded );
}
