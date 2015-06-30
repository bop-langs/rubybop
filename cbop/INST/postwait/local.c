#include <sys/mman.h>    // for mprotect
#include <errno.h>     /* for errno */
#include <stdlib.h>    // for exit
// #include "../hash/hash.h"
#include "../bop_api.h"  // for mem_range_t, BOP_Abort
#include "../bop_map.h"
#include "../data_store.h"  // collection_t
#include "postwait.h"

/* Each channel ID is mapped to a map_t root that contains the channel
   data ranges. */
map_t *local_chans = NULL;
map_t *pw_received = NULL, *pw_sent = NULL;

extern map_t *write_map, *read_map, *data_map;

void alloc_pw_meta_data( void ) {
  local_chans = new_hash( );
  pw_received = new_no_merge_map( );
  pw_sent = new_merge_map( );
}

void clear_local_pw_meta_data( void ) {
  mem_range_t *ranges; int num;
  map_to_array( local_chans, &ranges, &num );
  int i;
  for ( i = 0; i < num; i ++ ) {
    map_t *chan = (map_t *) ranges[i].rec;
    bop_msg( 4, "Clear channel %llu meta data, %d ranges, %llu bytes", ranges[i].base, chan->sz, map_size_in_bytes( chan ));
    map_free( chan );
  }
  free( ranges );

  map_clear( local_chans );
  map_clear( pw_received );
  map_clear( pw_sent );
}

static inline map_t *get_local_chan( chid ) {
  map_t *chan = (map_t *) map_search_key( local_chans, chid );
  if ( chan == NULL ) {
    chan = new_merge_map( );
    map_add_key_obj( local_chans, chid, (void*) chan );
  }
  return chan;
}

void BOP_fill(memaddr_t chid, void *addr, size_t size) {
  if ( myStatus == SEQ || myStatus == UNDY ) {
    bop_msg( 2, "BOP_Fill called with SEQ OR UNDY status is ignored", myStatus );
    return;
  }

  map_t *chan = get_local_chan( chid );

  memaddr_t base = (memaddr_t) addr;
  map_add_range( chan, base, size, ppr_index, (void*) chid );

  bop_msg(3, "bop fill channel %d from addr %llx (page %llu) for %lld bytes", 
	  chid, base, base>>PAGESIZEX, size);
}

void BOP_fill_page( memaddr_t chid, void *start_addr, size_t size ) {
  memaddr_t pstart = PAGESTART( (memaddr_t ) start_addr );
  size_t psize = PAGEFILLEDSIZE2( (memaddr_t) start_addr, size );
  BOP_fill( chid, (void*) pstart, psize );
}

void BOP_post( memaddr_t chid ) {
  map_t *chan = get_local_chan( chid );

  /* keeping only the modified pages */
  unsigned orig = chan->sz;
  map_intersect( chan, write_map );
  unsigned after = chan->sz;
  if ( orig > after ) {
    bop_msg( 2, "BOP_post: %d out of %d range(s) are not in the write_map and removed from sent data", orig - after, orig );
    map_inspect( 2, write_map, "write_map" );
  }

  char succeeded = sb_post_channel( chid, chan, ppr_index ); 

  /* union the posted set with the chan set */
  if ( succeeded )
    map_union( pw_sent, chan );

  map_clear( chan );
}

void process_wait( pw_channel_t *chin ) {
  if ( chin->sender > ppr_index ) {
    bop_msg( 2, "Received data from channel %lld posted by a later task %u is ignored (my ppr id is %d)", chin->chid, chin->sender, ppr_index );
    return;
  }

  shm_map_t *w_data = chin->collection;
  bop_msg( 2, "processing data received from %d", chin->sender );
  map_inspect( 3, w_data, "received data" );

  mem_range_t *ranges; int i, num;
  map_to_array( w_data, &ranges, &num );
  for ( i = 0; i < num; i ++ ) {
    mem_range_t *seg = & ranges[i];
    mem_range_t c;
    if ( map_overlaps( write_map, seg, &c ) || 
	 map_overlaps( read_map, seg, &c ) ) {
      report_conflict( 1, seg, "received data", &c, "read or write map" );
      BOP_abort_spec( "of a receiver conflict");
    }

    /* See if the new arrival replaces an old arrival.  Only do this
       for exact matches. */
    if ( map_overlaps( pw_received, seg, NULL ) ) {
      mem_range_t * prev = map_contains( pw_received, seg->base );
      bop_msg( 1, "Warning: %u bytes from %llx (page %llu) received from ppr %i overlaps with %u bytes from %llx from ppr %u earlier.", seg->size, seg->base, seg->base >> PAGESIZEX, chin->sender, prev->size, prev->base, prev->task_id );
      if ( seg->base == prev->base && seg->size == prev->size ) {
	/* Update the page if needed. ">=" (as opposed to >) allows
	   the same precedessor to send the same page for the 2nd
	   time.  */
	if ( chin->sender >= prev->task_id ) {
	  prev->task_id = chin->sender;
	  prev->rec = seg->rec;
	  prev->rec2 = chin;
	}
	bop_msg( 1, "Warning: the latest version is kept");
      }
      else 
	bop_msg( 1, "Warning: dropping data in this overlapped range");
    }
    else {
      mem_range_t *n = map_add_range( pw_received, seg->base, seg->size, chin->sender, seg->rec);
      n->rec2 = chin;
    }  /* end checking overlap */
  } /* end iterating ranges */
}

void BOP_wait( memaddr_t chid ) {
  if ( myStatus == SEQ || myStatus == MAIN || myStatus == UNDY ) return;

  bop_msg( 2, "waiting for channel %d", chid );
  /* call channel in "chin" */
  pw_channel_t *chin = sb_wait_channel( chid, ppr_index );

  process_wait( chin );

  map_inspect(3, pw_received, "pw_received");
}

void check_postwait( memaddr_t addr, size_t size ) {
  if ( ordered_writes != NULL )
    register_ordered_write( addr, size );

  mem_range_t *range = map_contains( pw_received, addr );

  if ( range != NULL ) {
    /* Not necessarily a bad thing, but disallow for now */
    assert( range->base <= addr && range->base+range->size >= addr+size );

    BOP_protect_range( (void*) addr, size, PROT_WRITE );
    memcpy( (char*) addr, 
	    (char *) range->rec + (addr - range->base), 
	    size );
    BOP_protect_range( (void*) addr, size, PROT_READ );
    map_add_range( read_map, addr, size, ppr_index, NULL );      

    pw_channel_t *crec = range->rec2;
    if ( crec != NULL ) {
      assert( crec->sender < ppr_index );
      if ( crec->earliest_receiver == 0 )
	crec->earliest_receiver = ppr_index;
      else
	crec->earliest_receiver = min( crec->earliest_receiver, ppr_index );
    }
  }
}

/* NOTE: pw_sent changes meaning to postwait pages after this call */
map_t *combine_pw_sent_recv( void ) {
  map_union( pw_sent, pw_received );
  return pw_sent;
}

char check_pw_receive_set( memaddr_t base, int *sender ) {
  mem_range_t *range = map_contains( pw_received, base );
  if ( range != NULL )
    *sender = range->task_id;
  return range!=NULL;
}

