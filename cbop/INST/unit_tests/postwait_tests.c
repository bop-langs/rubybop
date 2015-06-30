#include <stdlib.h>  /* for NULL */
#include "../bop_api.h"  /* for PAGESIZEX */
#include "../bop_map.h"  /* always include after bop_api.h */
#include "../data_depot.h"  /* data store functions */
#include "../postwait/postwait.h"

extern map_t *local_chans, *pw_received, *pw_sent;
extern map_t *read_map, *write_map;
extern int ppr_index;

static void check( map_t *map, unsigned nodes, memaddr_t bytes ) {
  map_inspect( 1, map, "map" );
  assert( map->sz == nodes );
  assert( map_size_in_bytes( map ) == bytes );
}

static void check_range( mem_range_t *x, memaddr_t base, size_t size ) {
  assert( x->base == base );
  assert( x->size == size );
}

int main( ) {
  BOP_init( );
  BOP_set_verbose( 6 );
  
  group_meta_data_init( 8 );

  /* local tests */
  alloc_pw_meta_data( );

  myStatus = SPEC;

  fill_test( );

  local_receive( );

  /* now adding the switch board */
  create_switch_board( );

  two_task( );
}

int two_task( void ) {
  myStatus = SPEC;
  ppr_index = 1;
  mySpecOrder = 0;
  map_clear( write_map );
  int i, n = 1000;

  char *datap = (char *) malloc( n * PAGESIZE );
  for ( i = 0; i < n; i ++ ) {
    /* task 1 */
    BOP_fill_page( (memaddr_t) i, &datap[i*PAGESIZE], sizeof(char) );
  }

  memset( datap, 'a', n*PAGESIZE );
  for ( i = 0; i < n; i ++ ) {
    BOP_record_write_page( &datap[i*PAGESIZE] );
    BOP_post( (memaddr_t) i );
  }

  ppr_index = 2;
  mySpecOrder = 1;
  map_clear( write_map );
  memset( datap, 0, n*PAGESIZE );
  for ( i = 0; i < n; i ++ ) {
    BOP_wait( (memaddr_t) i );
  }

  for ( i = 0; i < n; i ++ ) {
    BOP_record_read_page( &datap[ i*PAGESIZE ] );
    assert( datap[ i*PAGESIZE ] == 'a' );
  }

  myStatus = SEQ;
  ppr_index = mySpecOrder = 0;
}

int local_receive( void ) {
  map_t * to_send = new_merge_map( );
  int data = 5, *datap;
  datap = (int *) malloc( 4*PAGESIZE );
  datap[0] = 6; datap[PAGESIZE/sizeof(int)] = 7; datap[2*PAGESIZE/sizeof(int)] = 8;

  map_add_range( to_send, (memaddr_t) PAGESTART(datap), 3*PAGESIZE, 10, NULL );
  map_add_range( to_send, (memaddr_t) PAGESTART(&data), PAGESIZE, 10, NULL);

  data_depot_t *depot = (data_depot_t *) create_data_depot( "test local receive" );

  shm_map_t *task0_send = depot_add_collection_with_data( depot, NULL, to_send);
  
  pw_channel_t *chin = (pw_channel_t *) malloc( sizeof(pw_channel_t) );

  chin->chid = 200;
  chin->sender = 10;
  chin->earliest_receiver = 0;
  chin->collection = task0_send;

  ppr_index = 12;  /* I'm task 12 */
  process_wait( chin );
  check( pw_received, 2, 4*PAGESIZE );

  datap[ 0 ] = 60;
  BOP_record_read_page( &datap[0] );
  assert( datap[ 0 ] == 6 );
  check( read_map, 1, PAGESIZE );

  map_clear( to_send );
  map_add_range( to_send, (memaddr_t) PAGESTART(&datap[PAGESIZE/sizeof(int)]), PAGESIZE, 11, NULL );
  shm_map_t *task1_send = depot_add_collection_with_data( depot, NULL, to_send );

  BOP_record_write_page( chin );
  chin->sender = 11;
  chin->collection = task1_send;
  process_wait( chin );
  check( pw_received, 2, 4*PAGESIZE );

  datap[ PAGESIZE/sizeof(int) ] = 70;
  BOP_record_read_page( &datap[PAGESIZE/sizeof(int)] );
  assert( datap[ PAGESIZE/sizeof(int) ] == 7 );
  check( read_map, 1, 2*PAGESIZE );
}

int fill_test( void ) {
  int data = 5, *datap;
  datap = (int *) malloc( sizeof( int ) );
  *datap = 6;

  /* coded Oct 9, 2010 */
  BOP_fill_page( 1092010, &data, sizeof( int ) );
  BOP_fill_page( 1092010, datap, sizeof( int ) );
  check( local_chans, 1, 1 );
  map_t *ch_ranges = (map_t *) map_search_key( local_chans, 1092010 );
  check( ch_ranges, 2, PAGESIZE*2 ); 
}
