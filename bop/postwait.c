#include <inttypes.h> /* for PRIdPTR */
#include <stdlib.h> /* for NULL */
#include <string.h> /* for memcpy */
#include "bop_api.h"
#include "external/malloc.h"
#include "atomic.h"  // for locking
#include "bop_ports.h"
#include "bop_merge.h"
#include "bop_map.h"

extern mspace metacow_space;

map_t received;          /* for checking ordering conflicts */
map_t received_n_used;   /* for checking write-after-post conflicts */

mspace pw_space = NULL;  /* shared memory space */

typedef struct _channel_t {
  addr_t id;        /* valid if not zero */
  map_t data;
  unsigned sender;  /* non-zero means posted */
  char is_posted;

  /* for channel chaining */
  struct _channel_t *cc_first, *cc_next;
} channel_t;

struct switch_board_t {
  channel_t *channels;   /* first come, first serve */
  unsigned size;         /* no re-sizing during ppr */
  bop_lock_t lock;       /* global lock, needed for channel chaining */
} switch_board;

addr_t local_ch_id = 0;
map_t local_ch_data;

void channel_local_reset( void ) {
  local_ch_id = 0;
  map_clear( & local_ch_data );
}

static void ppr_reset( void ) {
  channel_local_reset( );
  map_clear( & received );
  map_clear( & received_n_used );
}

static void clear_channel_shm_data( void ) {
  int i;
  for ( i = 0; i < switch_board.size; i ++ )
    if ( switch_board.channels[ i ].id != 0 )
      clear_patch( & switch_board.channels[ i ].data );
}

static void postwait_init( void ) {
  if (pw_space == NULL) {
    char use_lock = 1;
    char is_shared_mem = 1;  /* true */
    size_t init_size = 8000; /* 8KB init size */
    pw_space = create_mspace( init_size, use_lock, is_shared_mem );
    bop_msg( 3, "created postwait (shared memory) mspace at %p", pw_space );
    switch_board.size = 10 * BOP_get_group_size();
    switch_board.channels = 
      (channel_t *) mspace_calloc( pw_space, 
				   switch_board.size, 
				   sizeof( channel_t ));
    assert( metacow_space != NULL );  /* bop_merge_port.ppr_group_init needed */
    init_empty_map( & local_ch_data, metacow_space, "local_ch_data" );
    init_empty_map( & received, metacow_space, "received" );
    init_empty_map( & received_n_used, metacow_space, "received_n_used" );
  }
  else {
    clear_channel_shm_data( );
    unsigned newsize = 10 * BOP_get_group_size( );
    if ( switch_board.size < newsize ) {
      switch_board.size = newsize;
      switch_board.channels = 
	(channel_t *) mspace_realloc( pw_space, 
				    switch_board.channels,
				    switch_board.size * sizeof( channel_t ));
    }
  }

  memset( switch_board.channels, 0,
	  switch_board.size * sizeof( channel_t ) );
  bop_lock_clear( & switch_board.lock );

  bop_msg(4, "postwait_init: (%d) channels (re-)initialized", 
	  switch_board.size );
}

static int has_local_channel( addr_t id ) {
  assert( id > 0 );
  if ( local_ch_id != 0 && local_ch_id != id ) {
    bop_msg( 1, "channel fill/post for "PRIdPTR" is ignored (currently "PRIdPTR"). use channel_local_reset to clear the previous record or wait for next BOP version to allow working on multiple channels at the same time.", id, local_ch_id );
    return 0;
  }
  return 1;
}

void channel_fill( addr_t id, addr_t base, unsigned size ) {
  if ( task_status == SEQ || task_status == UNDY ) return;
  if ( ! has_local_channel( id ) ) return;

  if ( local_ch_id == 0 ) local_ch_id = id;

  map_add_range( & local_ch_data, base, size, NULL );
  bop_msg( 4, "channel fill : map_add_range %p, %p, %.0f, %d", &local_ch_data, base, *((double *)base), size);
}

extern map_t read_map;
extern map_t write_map;
extern void map_subtract(map_t *, map_t *);

void channel_post( addr_t id ) {
  if ( task_status == SEQ || task_status == UNDY ) return;
  if ( ! has_local_channel( id ) ) return;

  /* write-only data */
  map_intersect( & local_ch_data, & write_map );

  if ( local_ch_data.size == 0 ) {
    bop_msg( 1, "zero-content channel "PRIdPTR" post", id );
  }

  map_inspect( 6, & local_ch_data );

  unsigned index = id % switch_board.size;
  channel_t *ch = & switch_board.channels[ index ];
  bop_lock_acquire( & switch_board.lock );
  if ( ch->id != 0 && ch->id != id ) {
    bop_msg( 1, "channel "PRIdPTR" conflict with "PRIdPTR" (task %u), fail to allocate", id, ch->id, ch->sender );
    bop_lock_release( & switch_board.lock );
    return; 
  }
  ch->id = id;
  if ( ch->is_posted ) {
    bop_msg( 1, "Posting to posted channel "PRIdPTR" (by task %u), ignored", ch->id, ch->sender );
    bop_lock_release( & switch_board.lock );
    return;
  }
  ch->sender = BOP_ppr_index( );

  create_patch( & ch->data, & local_ch_data, pw_space );

  if ( ch->cc_first == NULL )
    ch->is_posted = 1;
  else {
    ch = ch->cc_first;
    while ( ch != NULL ) {
      ch->is_posted = 1;
      ch = ch->cc_next;
    }
  }
  bop_lock_release( & switch_board.lock );

  map_subtract( & write_map, & local_ch_data );

  channel_local_reset( );
}

/* Allocate a channel record (and initialize the map) if it is the
   first request.  NULL if no channel record is available */
static channel_t *get_channel( addr_t id ) {
  channel_t *ch =  & switch_board.channels[ id % switch_board.size ];
  if ( ch->id != 0 && ch->id != id ) return NULL;  /* no resource */
  if ( ch->id == 0 ) { /* init */
    ch->id = id;
    init_empty_map( & ch->data, pw_space, "channel data" );
  }
  return ch;
}

static void set_chain( channel_t *ch, channel_t *fst, channel_t *nxt, char posted ) {
  ch->cc_first = fst;
  ch->cc_next = nxt;
  ch->is_posted = posted;
}

/* new_ch may or may not be in a chain */
static void connect_chain( channel_t *head, channel_t *new_ch, char posted ) {
  if ( new_ch->cc_first != NULL )
    new_ch = new_ch->cc_first;

  channel_t *p = head;
  while ( p->cc_next != NULL )
    p = p->cc_next;
  set_chain( p, head, new_ch, posted );
  while ( new_ch != NULL ) {
    set_chain( new_ch, head, new_ch->cc_next, posted );
    new_ch = new_ch->cc_next;
  }
}

void channel_chain( addr_t id1, addr_t id2 ) {
  if ( task_status == SEQ || task_status == UNDY ) return;
  bop_lock_acquire( & switch_board.lock );
  channel_t *ch1 = get_channel( id1 );
  channel_t *ch2 = get_channel( id2 );
  if ( ch1 == NULL || ch2 == NULL ) {
    bop_msg( 1, "chaining of "PRIdPTR" and "PRIdPTR" failed for lack of resource", id1, id2 );
    bop_lock_release( & switch_board.lock );
    return;
  }

  char posted = ch1->is_posted || ch2->is_posted;

  if ( ch1->cc_first == NULL && ch2->cc_first == NULL ) {
    set_chain( ch1, ch1, ch2, posted );
    set_chain( ch2, ch1, NULL, posted );
  }
  else {
    if ( ch1->cc_first != NULL ) 
      connect_chain( ch1->cc_first, ch2, posted );
    else
      connect_chain( ch2->cc_first, ch1, posted );
  }
  bop_lock_release( & switch_board.lock );
}

range_node_t * new_node( mspace msp, addr_t base, size_t size, void *obj );
range_node_t * splay (range_node_t * t, addr_t key);
char overlap(mem_range_t *b, mem_range_t *r, addr_t *base, size_t *size);

/* Split the range when needed, which will return a non-nil range node
   leftover */
static range_node_t *range_remove_overlap_return_leftover( mem_range_t *range, mspace lospace, addr_t obase, size_t osize ) {
  if ( range->base < obase )
    range->size = obase - range->base;
  if ( range->base + range->size > obase + osize ) {
    range_node_t * n = new_node( lospace, obase + osize,
				 range->base + range->size - (obase + osize),
				 NULL );
    n->r.task = range->task;
    return n;
  }
  else
    return NULL;
}

static char *data_clone( mspace space, char *src, size_t size ) {
  char *newdata = (char *) mspace_malloc( space, size );
  memcpy( newdata, src, size );
  return newdata;
}

mem_range_t *map_add_range_from_task(map_t *map, addr_t base, size_t size, void *obj, unsigned ppr_id) {
  assert( map != NULL );
  assert( size > 0 );

  bop_msg( 6, "map %s (%p) adding from task %u the range %p, size %zd, obj %p", map->name, map, ppr_id, base, size, obj );

  char *data = obj == NULL? NULL :
    data_clone( map->residence, (char *) obj, size );
  range_node_t * n = new_node( map->residence, base, size, (void *) data );
  n->r.task = ppr_id;

  if(map->root == NULL) {
    map->root = n;
    map->size = 1;
    return & map->root->r;
  }

  range_node_t *top = splay( map->root, base);

  /* If there is overlap, split the root node to remove the overlap. A
     future extension is to keep the part from a later ppr_id. It
     would require the full implementation of reordered receives.  */
  addr_t cbase; size_t csize;
  mem_range_t *target = &top->r;
  char has_overlap = overlap( &n->r, target, &cbase, &csize);
  if ( ! has_overlap && top->lc != NULL )  { 
    target = &top->lc->r; /* could be overlapping with left child */
    has_overlap = overlap( &n->r, target, &cbase, &csize);
  }

  if ( ! has_overlap ) {  /* no overlap */
    map->root = n;
    if ( n->r.base < top->r.base ) { /* insert left */
      n->rc = top;
      n->lc = top->lc;
      // if (!(n->lc->r.base + n->lc->r.size <= n->r.base ))
      //     bop_msg( 3, "n->lc->r.base %p, n->lc->r.size %zd, n->r.base %p, n->rc->r.base %p", n->lc->r.base, n->lc->r.size, n->r.base, n->rc->r.base);
      assert( n->lc == NULL || n->lc->r.base + n->lc->r.size <= n->r.base );
      top->lc = NULL;
    }
    else { /* insert right */
      n->lc = top;
      n->rc = top->rc;
      assert( n->rc == NULL || n->r.base + n->r.size <= n->rc->r.base );
      top->rc = NULL;
    }
    map->size ++;
    return & n->r;
  }

  bop_msg( 3, "Overlap, write_union [%p, %p), new range [%p, %p)", target->base, target->base + target->size, base, base + size );

  /* An exact overlap.  Replace the node. */
  if ( n->r.base == top->r.base && n->r.size == top->r.size ) {
    void *newdata = n->r.rec;
    unsigned newtask = n->r.task;
    mspace_free( map->residence, n );
    if ( top->r.task > newtask )
      mspace_free( map->residence, newdata );  /* silent drop */
    else {
      mspace_free( map->residence, top->r.rec );
      top->r.rec = newdata;
      top->r.task = newtask;
    }
    map->root = top;
    return & top->r;
  }

  BOP_abort_spec( "non-perfect range overlap in post-wait is not implemented" );
  return NULL;

  /* overlapping cases not implemented
  range_node_t *leftover = 
    range_remove_overlap_return_leftover( &top->r, map->residence,
					  cbase, csize );
  */
  /* Call this function recursively to add the leftover.  Take care to
     split the patch data accordingly.  */
}

/* Return 1 if assertion passes. */
static char assert_no_overlap( map_t *map1, map_t *map2 ) {
  map_t *copy = map_clone( map1, metacow_space );
  map_intersect( copy, map2);
  char has_overlap = ( copy->size != 0 );
  if ( has_overlap ) {
    bop_msg( 1, "maps %s and %s overlap at %p for %zd bytes", map1->name, map2->name, copy->root->r.base, copy->root->r.size );
    map_inspect( 5, map1 );
    map_inspect( 5, map2 );
    map_inspect( 6, copy );
    map_free( copy );
    return 0;
  }
  else {
    map_free( copy );
    return 1;
  }
}

static void receive_range( void *sum, mem_range_t *range ) {
  channel_t *ch = (channel_t *) sum;
  mem_range_t *approved = 
    map_add_range_from_task( & received, range->base, range->size, 
			     range->rec, ch->sender );
  memcpy( (void *) approved->base, approved->rec, approved->size );
}

static inline void wait_until_posted( channel_t *ch ) {
  if ( ch->is_posted ) return;
  int b = 1;
  while ( ! ch->is_posted )
    backoff( &b );
}

void channel_wait( addr_t id ) {
  if ( task_status == SEQ || task_status == UNDY || task_status == MAIN ) 
    return;
  bop_msg( 3, "waiting for channel %d", id );
  unsigned index = id % switch_board.size;
  channel_t *ch = & switch_board.channels[ index ];
  wait_until_posted( ch );
  if ( ch->id != id ) 
    BOP_abort_spec( "no forthcoming post" );
  bop_msg( 4, "received channel %d", id );
  
  if ( ch->cc_first != NULL ) { /* there is chaining */
    ch = ch->cc_first;
    while ( ch->data.size == 0 && ch->cc_next != NULL )
      ch = ch->cc_next;
    assert( ch->data.size != 0 ); /* can't find a non-empty post */
  }

  /* a more conservative check
  if ( ! assert_no_overlap( & received, & ch->data ) ) {
    bop_msg( 1, "channel wait abandoned since data is received again" );
    return;
    } */

  if ( ! assert_no_overlap( & ch->data, & read_map ) || 
       ! assert_no_overlap( & ch->data, & write_map ) ) {
    bop_msg( 1, "channel wait abandoned due to a access-before-wait conflict" );
    return;
  }

  map_inject( & ch->data, (void *) ch, receive_range );
}

extern map_t *patches;

static void mdfy_aftr_pst_chk( mem_range_t *mem ) {
  unsigned sender_spec_order = spec_order - (BOP_ppr_index( ) - mem->task);
  map_t *patch = & patches[ sender_spec_order ];
  mem_range_t *match = map_contains( patch, mem->base );
  assert( match != NULL );
  void *committed = match->rec;
  void *received = mem->rec;
  char diff = memcmp( committed + mem->base - match->base,
		      received, mem->size );
  if ( diff ) {
    bop_msg( 1, "modify-after-post by task %u for %p, %zd bytes", mem->task, mem->base, mem->size );
    BOP_abort_spec( "correctness checking failed" );
  }
}

static void ppr_commit( void ) {
  if ( received_n_used.size == 0 ) return;

  /* check for modify-after-post conflicts */
  map_foreach( & received_n_used, mdfy_aftr_pst_chk );

  bop_msg( 4, "finish checking modify-after-post conflicts" ); 
}

char channel_is_posted( addr_t id ) {
  unsigned index = id % switch_board.size;
  channel_t *ch = & switch_board.channels[ index ];
  if ( ch->id != id ) return 0;
  return ch->is_posted;
}

bop_port_t postwait_port = {
  .ppr_group_init        = postwait_init,
  .ppr_task_init         = ppr_reset,
  .data_commit           = ppr_commit,
};

