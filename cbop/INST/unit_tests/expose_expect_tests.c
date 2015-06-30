#include <stdlib.h>  /* for NULL */
#include "../bop_api.h"  /* for PAGESIZEX */
#include "../bop_map.h"  /* always include after bop_api.h */
#include "../data_depot.h"  /* data store functions */
#include "../postwait/postwait.h"

extern map_t *local_exp_to_send, *read_map, *write_map, *pw_received;

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

int test2( char );

int main( ) {
  BOP_init( );  /* calls create_exp_board */
  BOP_set_verbose( 6 );
  
  group_meta_data_init( 8 ); /* calls init_exp_board_pre_spec_group */

  test1( );
  test2( 0 );
  
  /* ready to exit */
  myStatus = SEQ;

}

int test1( ) {
  /* local tests */
  myStatus = MAIN;
  ppr_index = 0;
  int tmp;
  int *a = (int *) malloc( sizeof( int ) );
  tmp = 3;
  BOP_record_write( &tmp, sizeof( tmp ) );
  *a = 4;
  BOP_record_write( a, sizeof( int ) );
  BOP_expose_later( &tmp, sizeof( tmp ) );
  BOP_expose_later( &tmp, sizeof( tmp ) );
  BOP_expose_later( a, sizeof( int ) );
  check( local_exp_to_send, 2, sizeof( int ) * 2 );

  /* one expose-expect pair */
  BOP_expose_now( );
  myStatus = SPEC;
  ppr_index = 1;
  tmp = 333;
  *a = 444;
  BOP_expect( &tmp, sizeof( tmp ) );
  BOP_record_read( &tmp, sizeof( tmp ) );
  assert( tmp == 3 );
  BOP_expect( a, sizeof( int ) );
  BOP_record_read( a, sizeof( int ) );
  assert( *a == 4 );

  *a = 5;
  BOP_expect( a, sizeof( int ) );
  assert( *a == 5 );
}

int test2( char use_page_prot ) {
  BOP_set_verbose( 1 );

  myStatus = SPEC;
  ppr_index = 1;
  /* 10000 expose-expect */
  int n = 10000;
  int *aa;
  if ( use_page_prot ) {
    aa = (int *) BOP_malloc( n * sizeof( int ) );
  }
  else {
    aa = (int *) malloc( n*sizeof( int ) );
    BOP_record_write( aa, n*sizeof( int ) );
  }

  int i;
  for ( i = 0; i < n; i ++ ) {
    aa[ i ] = i;
    BOP_expose( &aa[i], sizeof( int ) );    
  }
 
  ppr_task_init( );

  ppr_index = 2;
  BOP_expect( aa, sizeof( int ) );

  for ( i = 0; i < n; i += PAGESIZE/sizeof(int) ) {
    aa[i] = -i;
    BOP_record_read( &aa[i], sizeof( int ) );
    assert( aa[i] == i );
  }

}
