#include <stdlib.h>  // for malloc

#include "../bop_map.h"
#include "../utils.h"  // for max, min

int main( ) {
  bop_msg( 1, "test0");
  test0( NULL );
  test1( NULL );
  test_key_obj( NULL );
  test_set_ops( NULL );

  char is_shared_mem = 1;  /* false */
  char use_lock = 0; 
  size_t init_size = 20000000; /* 20M init size */
  mspace shm = create_mspace( init_size, use_lock, is_shared_mem );
  
  test0( shm );
  test1( shm );
  test_key_obj( shm );
  test_set_ops( shm );
  test_map_foreach( shm );
}

static void check( map_t *map, unsigned nodes, memaddr_t bytes ) {
  // map_inspect( 1, map, "map" );
  assert( map->sz == nodes );
  assert( map_size_in_bytes( map ) == bytes );
}

static void check_range( mem_range_t *x, memaddr_t base, size_t size ) {
  assert( x->base == base );
  assert( x->size == size );
}

static void nullify( mem_range_t *r ) {
  r->rec = NULL;
}

static void check_null( mem_range_t *r ) {
  assert( r->rec == NULL );
}

int test_map_foreach( mspace space ) {
  map_t *table = new_shm_hash( space );
  int i;
  int n = 1000;
  for ( i = 0; i < n; i ++ ) 
    map_add_key_obj( table, (memaddr_t) i, (void*) &i );

  check( table, n, n );

  map_foreach( table, nullify );

  map_foreach( table, check_null );

  bop_msg( 1, "finished testing map_visit on %d-node hash", n);
}

int test_set_ops( mspace space ) {
  map_t *map;
  if ( space == NULL) 
    map = new_hash( );
  else
    map = new_shm_hash( space );

  int task = 0;

  int i, n = PAGESIZE * 1000;
  for ( i = 0; i < n; i += PAGESIZE ) {
      map_add_key_obj( map, (memaddr_t) i, NULL );
  }
  check( map, 1000, 1000 );

  map_t *ref;
  if ( space == NULL) 
    ref = new_no_merge_map( );
  else
    ref = new_shm_no_merge_map( space );
  map_add_range( ref, 25*PAGESIZE, 50*PAGESIZE, task, NULL );
  map_intersect( map, ref );
  check( map, 50, 50 );

  map_intersect( ref, map );
  check( ref, 50, 50 );

  map_union( map, ref );
  check( map, 50, 50 );

  map_add_range( ref, 100*PAGESIZE, 1, task, NULL );
  map_union( map, ref );
  check( map, 51, 51 );

  map_free( map );
  map_free( ref );
} 

int test_key_obj( mspace space ) {
  map_t *table;
  if ( space == NULL) 
    table = new_hash( );
  else
    table = new_shm_hash( space );
  int k = 5;
  map_add_key_obj( table, 500*PAGESIZE, &k );
  map_add_key_obj( table, 500*PAGESIZE, &k );
  check( table, 1, 1 );

  int i, n = PAGESIZE * 1000;
  for ( i = 0; i < n; i += PAGESIZE ) {
    if (map_contains( table, i ) == NULL)
      map_add_key_obj( table, (memaddr_t) i, NULL );
  }
  check( table, 1000, 1000 );

  map_remove_node( table, 0 );
  map_remove_node( table, n - PAGESIZE );
  map_remove_node( table, 499*PAGESIZE );
  check( table, 997, 997 );

  mem_range_t *range = map_contains( table, 500*PAGESIZE );
  assert( range != NULL );
  assert( range->rec == &k );
  assert( *( (int*) range->rec) == 5 );

  map_free( table );
}

int test0( mspace space ) {
  map_t *map;
  if ( space == NULL)
    map = new_merge_map( );
  else
    map = new_shm_merge_map( space );
  int task = 0;
  map_add_range( map, 0, 1, task, NULL );
  check( map, 1, 1 );

  map_add_range( map, 1, 1, task, NULL );
  check( map, 1, 2 );

  map_add_range( map, 3, 1, task, NULL );
  check( map, 2, 3 );

  /* just filling the hole [2,2] */
  map_add_range( map, 2, 1, task, NULL );
  check( map, 1, 4 );

  map_add_range( map, 5, 2, task, NULL );
  check( map, 2, 6 );

  /* covering a bit larger than the hold [4,4] */
  map_add_range( map, 4, 2, task, NULL );
  check( map, 1, 7 );

  map_add_range( map, 10, 1, task, NULL );
  check( map, 2, 8 );

  map_add_range( map, 8, 1, task, NULL );
  check( map, 3, 9 );

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
  if (space == NULL)
    map2 = new_merge_map();
  else
    map2 = new_shm_merge_map(space);

  map_subtract(map, map2);
  check(map, 3, 9);

  map_add_range(map2, 0, 1, task, NULL);
  map_subtract(map, map2);
  check(map, 3, 8);

  map_add_range(map2, 8, 1, task, NULL);
  map_subtract(map, map2);
  check(map, 2, 7);

  map_add_range(map2, 2, 2, task, NULL);
  map_add_range(map2, 6, 1, task, NULL);
  map_subtract(map, map2);
  check(map, 3, 4);
}

int test1( mspace space ) {
  int num_segments = 100;
  int chunksize = 4096;

  char *vm_data = (char *) malloc( num_segments * chunksize );
  char *vm_data2 = (char *) malloc( num_segments * chunksize );

  map_t *vms;
  if ( space == NULL) 
    vms = new_merge_map( );
  else
    vms = new_shm_merge_map( space );

  map_add_range( vms, (memaddr_t) vm_data, num_segments * chunksize, 
		 0, NULL );
  map_add_range( vms, (memaddr_t) vm_data2, num_segments * chunksize, 
		 0, NULL );

  assert( map_contains( vms, (memaddr_t) vm_data ));
  assert( map_contains( vms, (memaddr_t) vm_data2 ));
  
  memaddr_t larger = max( (memaddr_t) vm_data, (memaddr_t) vm_data2 );
  memaddr_t smaller = min( (memaddr_t) vm_data, (memaddr_t) vm_data2 );

  mem_range_t conflict;
  if ( larger - smaller > 2*num_segments * chunksize ) {
    mem_range_t gap, conflict;
    gap.size = num_segments * chunksize;
    gap.base = smaller + gap.size;
    assert( ! map_overlaps( vms, &gap, &conflict ) );
  }
}
