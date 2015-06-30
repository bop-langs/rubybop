#include <stdlib.h>  // for exit
#include <errno.h>   // for errno
#include "../atomic.h"  // for locking
#include "../data_depot.h"  // for data store, collections, and vm segs
#include "../bop_map.h"   // for shm_map_t
#include "postwait.h"

extern map_t *pw_sent, *pw_received, *data_map, *read_map, *write_map;

map_t *local_exp_to_send;

typedef struct _exp_board_t {
  bop_lock_t lock;

  mspace exp_space;
  shm_map_t *tasks;  /* exposed data by each task */
  data_depot_t *data_depot;
} exp_board_t;

typedef struct _exp_task_t {
  shm_map_t *exposed;
  shm_map_t *current;
} exp_task_t;

exp_board_t *exp_board;

/* Done at the start of execution */
void create_exp_board( void ) {
  char is_shared_mem = 1;  /* true */
  char use_lock = 1; 
  size_t init_size = 2000000; /* 2M init size */
  mspace new_space = create_mspace( init_size, use_lock, is_shared_mem );
  bop_msg( 3, "created expose/expect area (shared memory) mspace at %llx", new_space );

  exp_board = (exp_board_t *) 
    mspace_calloc( new_space, 1, sizeof(exp_board_t) );

  exp_board->exp_space = new_space;
  exp_board->tasks = new_shm_hash( new_space );
  exp_board->data_depot = create_data_depot( "expose-expect data depot" );

  local_exp_to_send = new_merge_map( );

  init_exp_board_pre_spec_group( );
}

void clear_local_exp_meta_data( void ) {
  map_clear( local_exp_to_send );
}

static void free_depot_data( mem_range_t *r ) {
  exp_task_t *trec = (exp_task_t *) r->rec;
  assert( trec != NULL );
  map_free( trec->exposed );
  map_free( trec->current );
}

static inline void lock_exp_board( void ) {
  bop_msg( 4, "locking exp_board");
  bop_lock_acquire( &exp_board->lock );
  bop_msg( 3, "locked exp_board");
}

static inline void unlock_exp_board( void ) {
  bop_lock_release( &exp_board->lock );
}

/* Before a task group starts, we need to clear the lock, all active
   channel records, and move them to past posts. */
void init_exp_board_pre_spec_group( void ) {
  int i;
  bop_lock_clear( & exp_board->lock );

  map_foreach( exp_board->tasks, free_depot_data );
  map_clear( exp_board->tasks );

  map_clear( local_exp_to_send );
}

static inline exp_task_t *get_task_record( unsigned task_id ) {
  /* must operate in exclusive mode */
  assert( exp_board->lock == 1 );
  exp_task_t *trec = 
    (exp_task_t *) map_search_key( exp_board->tasks, task_id );

  if ( trec == NULL ) {
    trec = (exp_task_t *) mspace_calloc( exp_board->exp_space, 
					 1, sizeof( exp_task_t ) );
    trec->exposed = new_shm_no_merge_map( exp_board->exp_space );
    /* needed to interface with data_depot_t */
    trec->current = new_shm_no_merge_map( exp_board->exp_space );

    map_add_key_obj( exp_board->tasks, (memaddr_t) task_id, trec );
  }
  return trec;
}

void exp_board_inspect( int verbose, unsigned task_id ) {
  lock_exp_board( );
  exp_task_t *ch = get_task_record( task_id );
  unlock_exp_board( );

  bop_msg( verbose, "Exposed data by task %u", task_id );
  map_inspect( verbose, ch->exposed, "exposed and received by the next spec");
  map_inspect( verbose, ch->current, "exposed but not yet received" );

}

void exp_board_expose_now( unsigned task, map_t *data ) {
  bop_msg( 3, "Exp. board: new data sent by task %u", task );
  if ( data == NULL || data->sz == 0 ) return;

  lock_exp_board( );

  exp_task_t *trec = get_task_record( task );
  shm_map_t *r = depot_add_collection_with_data( exp_board->data_depot,
						 trec->current, data );
  assert( r == trec->current );

  unlock_exp_board( );

  bop_msg( 3, "exp_board_expose_now: new data added from task %u", task);
  map_inspect( 5, data, "new exposed data" );
  map_inspect( 5, trec->current, "augmented data" );

}

inline void BOP_expose_later( void *basep, size_t size ) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;

  memaddr_t base = (memaddr_t) basep;
  if ( map_contains( data_map, base ) != NULL ) {
    base = PAGESTART( base );
    size = PAGEFILLEDSIZE2( base, size );
    bop_msg( 3, "BOP_expose: change base and size to %llx, %u for monitored data", base, size );
  }
    
  map_add_range( local_exp_to_send, base, size, ppr_index, NULL );
}

inline void BOP_expose_now( void ) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;

  unsigned orig = local_exp_to_send->sz;
  map_intersect( local_exp_to_send, write_map );
  unsigned after = local_exp_to_send->sz;
  if ( orig > after ) {
    bop_msg( 2, "BOP_expose_now: %d out of %d range(s) are not in the write_map and removed from sent data", orig - after, orig );
    map_inspect( 2, write_map, "write_map" );
  }
  exp_board_expose_now( ppr_index, local_exp_to_send );
  map_union( pw_sent, local_exp_to_send );
  BOP_protect_range_set( local_exp_to_send, PROT_READ );
  map_clear( local_exp_to_send );

}

void BOP_expose( void *base, size_t size ) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;
  BOP_expose_later( base, size );
  BOP_expose_now( );
}

void exp_board_expect( unsigned requester ) {
  assert( myStatus != MAIN && myStatus != SEQ );
  lock_exp_board( );

  exp_task_t *trec = get_task_record( requester - 1 );
  map_t *tmp_map = new_no_merge_map( );
  map_union( tmp_map, trec->current );
  map_union( trec->exposed, trec->current );
  map_clear( trec->current );

  unlock_exp_board( );

  map_union( pw_received, tmp_map );
  bop_msg( 4, "exp_board_expect( %d ): new data added", requester );
  map_inspect( 4, tmp_map, "new data" );
  map_free( tmp_map );
}

void BOP_expect( void *basep, size_t size ) {
  if ( myStatus == SEQ || myStatus == UNDY ) return;

  memaddr_t base = (memaddr_t) basep;
  if ( map_contains( data_map, base ) != NULL ) {
    base = PAGESTART( base );
    size = PAGEFILLEDSIZE2( base, size );
    bop_msg( 3, "BOP_expect: change base and size to %llx, %u for monitored data", base, size );
  }

  if ( map_contains( read_map, base ) != NULL ||
       map_contains( write_map, base ) != NULL ) {
    mem_range_t *recv = map_contains( pw_received, base );
    if ( recv != NULL ) {
      bop_msg( 2, "BOP_expect: repeated request for data %llx, %u ignored (received range %llx, %u)", base, size, recv->base, recv->size );
      return;
    }
    else {
      bop_msg( 2, "BOP_expect: requested data %llx, %u already read or modified", base, size );
      BOP_abort_spec( "receiver conflict in BOP_expect" );
    }
  }

  mem_range_t exp_range;
  exp_range.base = base;
  exp_range.size = size;

  char completed = 0;
  int b = 1;
  while ( !completed ) {
    exp_board_expect( ppr_index );

    if ( map_overlaps( pw_received, &exp_range, NULL ) ) {
      mem_range_t *range = map_contains( pw_received, exp_range.base );
      if ( ! (range->base <= base && 
	      base+size <= range->base+range->size ) ) {
	bop_msg( 1, "BOP_expect: expected range (%llx, %u) not entirely nested in a received range (%llx, %u).  Trigger a failure.", base, size, range->base, range->size);
	BOP_abort_spec( "BOP_expect error" );
      }

      BOP_protect_range( (void*) base, size, PROT_WRITE );
      memcpy( (char*) base, 
	      (char *) range->rec + ((memaddr_t) base - range->base), 
	      size );
      BOP_protect_range( (void*) base, size, PROT_READ );
      map_add_range( read_map, base, size, ppr_index, NULL );      

      completed = 1;
    }

    assert( completed );
    if ( !completed ) {
      bop_msg( 2, "waiting in BOP_expect, %d", b );
      backoff( &b );
    }
  }
}

