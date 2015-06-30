#include <assert.h>
#include <stdio.h>
#include <stdlib.h>  // for malloc
#include <string.h>  // for memset and memcpy

#include "../data_store.h"
#include "../bop_map.h"

data_store_t store;
char *vm_data, *vm_data2;
char test_value = 42, test_value2 = 52;
vm_segment_t **segs;

memaddr_t chunksize = 4096;
int num_segments = 100;
int ppr_id = 42;

extern map_t *write_map;

#define TRUE 1

void test_segments( void );
void test_collections( void );

int main( ) {
  BOP_set_verbose( 0 );
  write_map = new_merge_map( );

  init_data_store( & store );
  
  vm_data = (char *) malloc( num_segments * chunksize );
  memset( vm_data, test_value, num_segments * chunksize);

  test_segments( );

  vm_data2 = (char *) malloc( num_segments * chunksize );
  memset( vm_data2, test_value2, num_segments * chunksize);


  printf("emptying the store\n");
  empty_data_store( & store );

  test_collections( );
}

void test_collections( void ) {
  char *vm_data2 = (char *) malloc( num_segments * chunksize );
  memset( vm_data2, test_value2, num_segments * chunksize);

  map_t *vms = new_merge_map( );
  int task = 0;
  /* ppr 22 */
  map_add_range( vms, (memaddr_t) vm_data, num_segments * chunksize, 
		 22, NULL );
  map_add_range( vms, (memaddr_t) vm_data2, num_segments * chunksize, 
		 22, NULL );

  collection_t *col1 = add_collection_no_data( & store, vms, NULL );

  assert( store.num_bytes == num_segments * chunksize * 2 );
  assert( store.num_collections == 1 );
  assert( store.num_segments = 2 );

  collection_t *col2 = add_collection_no_data( & store, vms, NULL );

  assert( store.num_bytes == num_segments * chunksize * 4 );
  assert( store.num_collections == 2 );
  assert( store.num_segments = 4 );
  
  copyout_collection_data( col1 );
  copyout_collection_data( col2 );

  assert( store.data[ num_segments * chunksize - 1] == test_value );
  assert( store.data[ num_segments * chunksize ] == test_value2 );

  store.data[ 0 ] = test_value - 1;
  copyin_collection_data( col1 );
  assert( vm_data[ 0 ] == test_value - 1 );
  assert( vm_data2[ 0 ] == test_value2 );

  store.data[ 3*num_segments*chunksize ] = test_value2 - 1;
  copyin_collection_data( col2 );
  assert( vm_data[ 0 ] == test_value );
  assert( vm_data2[ 0 ] == test_value2 - 1 );
}

void test_segments( void ) {
  int i;

  segs = (vm_segment_t **) malloc( num_segments * sizeof( vm_segment_t ) );

  printf("allocating and copying %d 4KB segments\n", num_segments);
  memaddr_t diff = (memaddr_t) store.data - (memaddr_t) vm_data;
  mem_range_t range;
  for (i=0; i < num_segments; i++) {
    char *vm_addr = & ( vm_data[ i * chunksize ] );
    range.base = (memaddr_t) vm_addr;
    range.size = chunksize;
    range.task_id = ppr_id;
    unsigned seg = add_segment( & store, &range );
    segs[ i ] = & store.segments[ seg ];
    assert( segs[i]->vm_seg.size == chunksize );
    assert( ((memaddr_t) segs[i]->store_addr) - segs[i]->vm_seg.base == diff );
    if ( i > 0 )
      assert( segs[i]->store_addr - segs[i-1]->store_addr == chunksize );
    memcpy( segs[i]->store_addr, (void *) segs[i]->vm_seg.base, chunksize);
    assert( segs[i]->store_addr[test_value] == test_value );
  }

  printf("emptying the store\n");
  empty_data_store( & store );

  range.base = (memaddr_t) & vm_data[ 0 ];
  range.size = chunksize;
  range.task_id = ppr_id;
  unsigned seg =  add_segment( & store, &range );
  assert( seg == 0 );

  free(segs);
}
