#include <inttypes.h>

#include "bop_api.h"
#include "bop_map.h"
#include "bop_ports.h"
#include "external/malloc.h"
#include "postwait.h"

extern mspace metacow_space;

map_t ordered_writes = {.residence = NULL};
char in_ordered_region = 0;
addr_t ordered_region_id;

static addr_t channel_for_region( addr_t region_id ) {
  int gs = BOP_get_group_size( );
  return region_id * gs + BOP_ppr_index( );
}

void BOP_ordered_begin( addr_t id ) {
  if ( ordered_writes.residence == NULL ) {
    bop_msg( 2, "BOP ordered port not active. BOP_ordered_region( "PRIdPTR" ) ignored.", id );
    return;
  }

  if ( ! in_ordered_region ) {
    in_ordered_region = 1;
    ordered_region_id = id;
    bop_msg( 2, "Entering ordered region "PRIdPTR, id );
    addr_t my_ch = channel_for_region( id );
    channel_wait( my_ch - 1 );
  }
  else
    bop_msg( 2, "Currently in ordered region "PRIdPTR". BOP_ordered_region( "PRIdPTR" ) ignored.", ordered_region_id, id );
}

/* pass over, leave out, disregard */
void BOP_ordered_skip( addr_t id ) {
  addr_t my_ch = channel_for_region( id );
  channel_chain( my_ch - 1, my_ch );
}

static void fill_range( void *sum, mem_range_t *range ) {
  addr_t ch = (addr_t) sum;
  channel_fill( ch, range->base, range->size );
}

void BOP_ordered_end( addr_t id ) {
  if ( in_ordered_region && ordered_region_id == id ) {
    bop_msg( 2, "Leaving ordered region "PRIdPTR, id );
    addr_t my_ch = channel_for_region( id );
    map_inject( & ordered_writes, (void *) my_ch, fill_range );
    channel_post( my_ch );
    map_clear( & ordered_writes );
    in_ordered_region = 0;
  }
  else
    bop_msg( 2, "No matching ordered region (%d "PRIdPTR"). BOP_ordered_end( "PRIdPTR" ) ignored.", in_ordered_region, ordered_region_id, id );
}

static void ppr_reset( void ) {
  if ( ordered_writes.residence == NULL ) 
    init_empty_map( & ordered_writes, metacow_space, "ordered_writes" );
  else
    map_clear( & ordered_writes );
}

bop_port_t bop_ordered_port = {
  .ppr_task_init         = ppr_reset,
};
