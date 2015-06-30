#include <stdlib.h>  /* for NULL */
#include "../bop_api.h"  /* for malloc routines */
#include "../bop_map.h"  /* always include after bop_api.h */
#include "../data_depot.h"  /* data store functions */

extern map_t *read_map, *write_map;
extern int ppr_index;

int copyout_copyin( void );
int calloc_realloc( void );

int main( ) {
  BOP_init( );
  BOP_set_verbose( 6 );

  group_meta_data_init( 8 );
  
  bop_malloc_init( 8 );

  copyout_copyin( );

  bop_msg( 1, "tests finished correctly" );
  abort( );
}

int const M = 1000;

int calloc_realloc( void ) {
  int *mem = BOP_calloc( 1000, sizeof(int) );
  int i;
  for ( i=0; i < M; i++ ) assert( mem[i] == 0 );

  for ( i=0; i < M; i++ ) mem[i] == i;
  int *mem1 = BOP_realloc( mem, 10*M*sizeof(int) );
  assert( mem1 != mem );
  for ( i=0; i < M; i++ ) assert( mem1[i] == i );
}

int const N = 10;
data_depot_t *depot;

int copyout_copyin( void ) {
  myStatus = SPEC;
  mySpecOrder = 1; 
  map_clear( write_map );
  int i, j;
  void *data[N];

  for ( i = 0; i < N; i ++ ) {
    bop_msg( 5, "******************* %i", i );
    data[i] = BOP_malloc( i );
    memset( data[i], i, i );
  }

  // assert( write_map->sz == N );

  depot = create_data_depot( "depot" );

  map_inspect( 5, write_map, "write_map" );
  shm_map_t *col = depot_add_collection_with_data( depot, NULL, write_map );

  for ( i = 0; i < N; i ++ ) {
    memset( data[i], -1, i );
  }
  
  copy_collection_from_depot( col );

  for ( i = 0; i < N; i ++ ) 
    for ( j = 0; j < i; j ++ ) {
      assert( ((char *) data[i])[j] == i );
  }

  empty_data_depot( depot );
}

