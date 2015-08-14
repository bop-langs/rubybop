#include <inttypes.h> /* for PRIdPTR */
#include <stdlib.h>  // for malloc

#include "bop_map.h"
#define min(a, b) ((a)<(b)? (a): (b))
#define max(a, b) ((a)<(b)? (b): (a))

static void check( map_t *map, unsigned nodes, addr_t bytes ) {
  // map_inspect( 1, map, "map" );
  assert( map->size == nodes );
  assert( map_size_in_bytes( map ) == bytes );
  bop_msg( 1, "check map passes, %u node(s), "PRIdPTR" byte(s)", nodes, bytes );
}

static void check_range( mem_range_t *x, addr_t base, size_t size ) {
  assert( x->base == base );
  assert( x->size == size );
}

void test_200k( mspace shm ) {
  int i, n = 200000;
  unsigned *ary = (unsigned *) mspace_malloc( shm, 2 * n * 4 );
  map_t *set = new_range_set( shm, "200k set 1" );
  for ( i = 0; i < n; i ++ )
    map_add_range( set, (addr_t) & ary[ i ], 4, NULL );
  map_inspect( 1, set );
  check( set, 1, n * 4 );	 
  map_free( set );
  set = new_range_set( shm, "200k set 2" );
  for ( i = 0; i < n; i ++ )
    map_add_range( set, (addr_t) & ary[ i + n ], 4, NULL );
  map_inspect( 1, set );
  check( set, 1, n * 4 );	 

  mspace_free( shm, ary );
}

int main( ) {
  bop_msg( 1, "test0");

  char is_shared_mem = 1;  /* false */
  char use_lock = 0; 
  size_t init_size = 20000000; /* 20M init size */
  mspace shm = create_mspace( init_size, use_lock, is_shared_mem );
  
  test0( shm );
  test1( shm );
  test_inject( shm );

  test_200k( shm );

  printf("The tests end.\n");
  /* 
  test_key_obj( shm );
  test_set_ops( shm ); 
  test_map_foreach( shm ); */
}

int test_inject( mspace space ) {
  map_t *set = new_range_set( space, "set" );

  int i;
  int n = 1000;
  for ( i = 0; i < 2*n; i += 2 ) {
    int *ptr = mspace_calloc( space, 1, sizeof(int) );
    *ptr = i;
    map_add_range( set, (addr_t) i, 1, ptr );
  }

  check( set, n, n );

  mem_range_t *ranges;
  int num_ranges;
  map_to_array( set, &ranges, &num_ranges );

  assert( num_ranges == n );
  for ( i = 0; i < n; i += 1 ) 
    assert( *((int*) ranges[ i ].rec) == i * 2 );
}

int test0( mspace space ) {
  map_t *map;
  map = new_range_set( space, "map" );
  int task = 0;
  map_add_range( map, 0, 1, NULL );
  check( map, 1, 1 );

  map_add_range( map, 1, 1, NULL );
  check( map, 1, 2 );

  map_add_range( map, 3, 1, NULL );
  check( map, 2, 3 );

  map_inspect( 1, map );

  /* just filling the hole [2,2] */
  map_add_range( map, 2, 1, NULL );
  check( map, 1, 4 );

  map_add_range( map, 5, 2, NULL );
  check( map, 2, 6 );

  /* covering a bit larger than the hold [4,4] */
  map_add_range( map, 4, 2, NULL );
  check( map, 1, 7 );

  map_add_range( map, 10, 1, NULL );
  check( map, 2, 8 );

  map_inspect( 1, map );

  map_add_range( map, 8, 1, NULL );
  check( map, 3, 9 );

  map_inspect( 1, map );

  mem_range_t *r = map_contains( map, 4 );
  check_range( r, 0, 7 );

  r = map_contains( map, 6 );
  check_range( r, 0, 7 );

  assert( map_contains( map, 7 ) == NULL );

  assert( map_contains( map, 11 ) == NULL );

  mem_range_t x, c;
  x.base = 7;
  x.size = 1;
  assert( ! map_overlaps( map, &x, &c ) );

  x.size = 2;
  assert( map_overlaps( map, &x, &c ) );
  check_range( &c, 8, 1 );

  map_t *map2;
  map2 = new_range_set(space, "map2");

  map_subtract(map, map2);
  check(map, 3, 9);

  map_add_range(map2, 0, 1, NULL);
  map_subtract(map, map2);
  check(map, 3, 8);

  map_add_range(map2, 8, 1, NULL);
  map_subtract(map, map2);
  check(map, 2, 7);

  map_add_range(map2, 2, 2, NULL);
  map_add_range(map2, 6, 1, NULL);
  map_subtract(map, map2);
  check(map, 3, 4);
}

int test1( mspace space ) {
  int num_segments = 100;
  int chunksize = 4096;

  char *vm_data = (char *) malloc( num_segments * chunksize );
  char *vm_data2 = (char *) malloc( num_segments * chunksize );

  map_t *vms;
  vms = new_range_set( space, "vms" );

  map_add_range( vms, (addr_t) vm_data, num_segments * chunksize, 
		 NULL );
  map_add_range( vms, (addr_t) vm_data2, num_segments * chunksize, 
		 NULL );

  assert( map_contains( vms, (addr_t) vm_data ));
  assert( map_contains( vms, (addr_t) vm_data2 ));
  
  addr_t larger = max( (addr_t) vm_data, (addr_t) vm_data2 );
  addr_t smaller = min( (addr_t) vm_data, (addr_t) vm_data2 );

  mem_range_t conflict;
  if ( larger - smaller > 2*num_segments * chunksize ) {
    mem_range_t gap, conflict;
    gap.size = num_segments * chunksize;
    gap.base = smaller + gap.size;
    assert( ! map_overlaps( vms, &gap, &conflict ) );
  }
}
