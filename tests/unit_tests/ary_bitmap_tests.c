#include <stdlib.h>
#include <bop_api.h>
#include <bop_ports.h>
#include <bop_map.h>
#include <external/malloc.h>

#include <ary_bitmap.h>

extern map_t read_map, write_map;
extern bop_port_t bop_merge_port;

void test1( void ) {
  BOP_set_group_size( 2 );
  bop_merge_port.ppr_group_init( );
  spec_order = 0;
  task_status = MAIN;
  ppr_pos = PPR;
  bop_merge_port.ppr_task_init( );

  unsigned len = 100, i;
  void *ptrs = ary_malloc_with_map( len, sizeof( void * ) );
  for ( i=0; i<len; i+=2 ) {
    ary_use_elem( ptrs, len, sizeof(void*), i );
    ary_promise_elem( ptrs, len, sizeof(void*), i+1 );
  }

  scan_ary_maps( ptrs, len, sizeof(void*) );

  assert( read_map.size == len / 2 );
  assert( write_map.size == len / 2 );

  for ( i=0; i<len; i+=2 ) {
    mem_range_t x;
    x.base = (addr_t) ((char*)ptrs);
    x.size = sizeof(void*);
    assert( map_overlaps( &read_map, &x, NULL ) );
    assert( !map_overlaps( &write_map, &x, NULL ) );
    x.base += sizeof(void*);
    assert( !map_overlaps( &read_map, &x, NULL ) );
    assert( map_overlaps( &write_map, &x, NULL ) );
  }
}

int main( ) {

  test1( );

  task_status = UNDY;
  printf("The tests end.\n");
  return 0;  /* must return 0 for rake test to succeed */
}
