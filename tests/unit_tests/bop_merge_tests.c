#include <stdlib.h>  /* for NULL */
#include <bop_api.h>  /* for PAGESIZEX */
#include <bop_map.h>
#include <bop_ports.h>

static void two_tasks_test(void);

extern map_t *read_map, *write_map;
extern bop_port_t bop_merge_port;

int main( ) {

  two_tasks_test( );

  task_status = UNDY;
  printf("The tests end.\n");
  return 0;  /* must return 0 for rake test to succeed */
}

static void ppr_access( void* addr, size_t size, int is_read ) {
  if ( is_read )
    BOP_record_read( addr, size );
  else
    BOP_record_write( addr, size );
}

static void two_tasks_test( void ) {
  BOP_set_group_size( 2 );
  ppr_pos = PPR;
  int x = 0, y = 0;

  /* pair 1: write to different data */
  bop_merge_port.ppr_group_init( );
  spec_order = 0;
  task_status = MAIN;
  bop_merge_port.ppr_task_init( );
  x = 1;
  ppr_access( &x, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );
  
  x = 0;
  bop_merge_port.ppr_task_init( );
  spec_order = 1;
  task_status = SPEC;
  y = 1;
  ppr_access( &y, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );

  x = 0; 
  y = 0;
  bop_merge_port.task_group_commit( );
  assert( x == 1 && y == 1 );

  /* pair 2: WAW conflict (false conflict) */
  bop_merge_port.ppr_group_init( );
  spec_order = 0;
  task_status = MAIN;
  bop_merge_port.ppr_task_init( );
  x = 1;
  ppr_access( &x, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );
 
  x = 0;
  spec_order = 1;
  task_status = SPEC;
  bop_merge_port.ppr_task_init( );
  x = 2;
  ppr_access( &x, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );

  x = 0; 
  bop_merge_port.task_group_commit( );
  assert( x == 2 );

  bop_msg( 1, "pair 3: WAR conflict (false conflict)" );
  bop_merge_port.ppr_group_init( );
  x = 1;
  spec_order = 0;
  task_status = MAIN;
  bop_merge_port.ppr_task_init( );
  y = x;
  ppr_access( &x, sizeof( int ), 1 );
  ppr_access( &y, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );
 
  x = 0;
  y = 0;
  spec_order = 1;
  task_status = SPEC;
  bop_merge_port.ppr_task_init( );
  x = 2;
  ppr_access( &x, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );

  x = 0;
  y = 0;
  bop_merge_port.task_group_commit( );
  assert( x == 2 && y == 1 );

  x = 0;
  y = 0;
  bop_msg( 1, "pair 4: RAW conflict" );
  bop_merge_port.ppr_group_init( );
  x = 1;
  spec_order = 0;
  task_status = MAIN;
  bop_merge_port.ppr_task_init( );
  x = 2;
  ppr_access( &x, sizeof( int ), 0 );
  assert( bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );
 
  x = 1;
  spec_order = 1;
  task_status = SPEC;
  bop_merge_port.ppr_task_init( );
  y = x;
  ppr_access( &x, sizeof( int ), 1 );
  ppr_access( &y, sizeof( int ), 0 );
  assert( ! bop_merge_port.ppr_check_correctness( ) );
  bop_merge_port.data_commit( );

  x = 0;
  y = 0;
  bop_merge_port.task_group_commit( );
  assert( x == 2 && y == 1 );

}

