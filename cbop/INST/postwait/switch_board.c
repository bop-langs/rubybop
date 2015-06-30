#include "../atomic.h"  // for locking
#include "../data_depot.h"  // for data store, collections, and vm segs
#include "../bop_map.h"   // for shm_map_t
#include "postwait.h"

extern map_t *pw_sent;

mspace pw_space;

typedef struct _switch_board_t {
  bop_lock_t lock;

  shm_map_t *channels;  /* active channels */
  data_depot_t *data_depot;
  shm_map_t *past_posts;

} switch_board_t;

switch_board_t *switch_board;

/* Done at the start of execution */
void create_switch_board( void ) {
  char is_shared_mem = 1;  /* true */
  char use_lock = 1; 
  size_t init_size = 2000000; /* 2M init size */
  pw_space = create_mspace( init_size, use_lock, is_shared_mem );
  bop_msg( 3, "created post-wait (shared memory) mspace at %llx", pw_space );

  switch_board = (switch_board_t *) 
    mspace_calloc( pw_space, 1, sizeof(switch_board_t) );

  switch_board->channels = new_shm_hash( pw_space );
  switch_board->past_posts =  new_shm_hash( pw_space );
  switch_board->data_depot = create_data_depot( "postwait data depot" );

  init_switch_board_pre_spec_group( );
}

static inline void lock_switch_board( void ) {
  bop_msg( 4, "locking switch board");
  bop_lock_acquire( &switch_board->lock );
  bop_msg( 3, "locked switch board");
}

static inline void unlock_switch_board( void ) {
  bop_lock_release( &switch_board->lock );
  bop_msg( 3, "unlocked switch board");
}

/* Before a task group starts, we need to clear the lock, all active
   channel records, and move them to past posts. */
void init_switch_board_pre_spec_group( void ) {
  bop_lock_clear( & switch_board->lock );

  mem_range_t *ranges;
  int num, i;
  map_to_array( switch_board->channels, &ranges, &num );
  for ( i = 0; i < num; i ++ ) {
    pw_channel_t *chrec = (pw_channel_t *) ranges[i].rec;
    if ( chrec->is_posted ) {
      map_add_key_obj( switch_board->past_posts, ranges[i].base, NULL );
      empty_depot_collection( chrec->collection );
    }
    else
      assert( chrec->collection == NULL );
  }
  free( ranges );
  map_clear( switch_board->channels );
}

/* marking the channel record as unused */
static inline void clear_channel_record( pw_channel_t * chrec ) {
  memset( (void*) chrec, sizeof( pw_channel_t ), 0 );
}

static inline pw_channel_t *get_channel_record( memaddr_t chid ) {
  /* must operate in exclusive mode */
  assert( switch_board->lock == 1 );
  pw_channel_t *sch = 
    (pw_channel_t *) map_search_key( switch_board->channels, chid );

  if ( sch == NULL ) {
    sch = (pw_channel_t *) mspace_calloc( pw_space, 1, sizeof(pw_channel_t) );
    sch->chid = chid;
    sch->collection = NULL;
    map_add_key_obj( switch_board->channels, chid, sch );
  }
  return sch;
}

void pw_channel_inspect( int verbose, memaddr_t chid ) {
  pw_channel_t *ch = get_channel_record( chid );

  bop_msg( verbose, "Channel record for %lld", ch->chid );
  if ( ch->is_posted )
      bop_msg( verbose, "\tPosted by task %u, earliest receiver %u", ch->sender, ch->earliest_receiver );
  else
      bop_msg( verbose, "\tNot yet posted");
  map_inspect( verbose, ch->collection, "data in channel" );

}

char sb_post_channel( memaddr_t chid, map_t *ch_data, int sender ) {
  bop_msg( 3, "Switch board: a post for channel %lld by task %u (%llx)", chid, sender, switch_board->past_posts );
  map_inspect( 5, switch_board->past_posts, "past_posts" );
  mem_range_t *prev = map_contains( switch_board->past_posts, chid );
  if ( prev != NULL ) {
    bop_msg( 1, "Channel %lld was posted by %u before.  The current attempt by task %u is ignored", chid, prev->task_id, sender );
    return 0;
  }

  char post_succeeded = 0;

  lock_switch_board( );

  pw_channel_t *chrec = get_channel_record( chid );
  if ( chrec->is_posted ) {
    bop_msg( 1, "Attempting to post to channel %lld again (by task %u), ignored", chid, sender );
    pw_channel_inspect( 2, chid );
    post_succeeded = 0;
  }
  else { 
    chrec->sender = sender;
    chrec->collection = 
      depot_add_collection_with_data( switch_board->data_depot,
				      chrec->collection, ch_data );
    chrec->is_posted = 1;
    bop_msg( 3, "sb_post_channel: channel %lld posted", chid);
    post_succeeded = 1;
  }

  unlock_switch_board( );
  return post_succeeded; 
}

pw_channel_t *sb_wait_channel( memaddr_t chid, int requester ) {
  lock_switch_board( );

  mem_range_t *prev = map_contains( switch_board->past_posts, chid );
  if ( prev != NULL ) {
    bop_lock_release( &switch_board->lock );
    return (pw_channel_t *) prev->rec;
  }

  pw_channel_t *chrec = get_channel_record( chid );

  unlock_switch_board( );

  int b = 1;
  while ( ! chrec->is_posted ) 
    backoff( &b );

  return chrec;
}

void sender_conflict_check( memaddr_t write_addr ) {
  mem_range_t * prec = map_contains( pw_sent, write_addr );
  if ( prec != NULL ) {
    // simple solution for now (for an elaborate one see revision 1450)
    assert( myStatus == MAIN || myStatus == SPEC );
    BOP_abort_next_spec( "of a sender conflict" );
  }
  return;
}

