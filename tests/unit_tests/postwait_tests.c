#include <stdlib.h>  /* for NULL */
#include <bop_api.h>  /* for PAGESIZEX */
#include <bop_map.h>
#include <bop_ports.h>

static void interface_tests(void);

extern bop_port_t postwait_port;
extern bop_port_t bop_merge_port;

extern unsigned ppr_index;

void empty_post( void ) {
  task_status = SEQ;
  BOP_set_group_size( 2 );
  ppr_group_init( );

  spec_order = 0;
  ppr_index = 10;
  task_status = MAIN;
  ppr_task_init( );
  channel_post( 2 );

  spec_order += 1;
  ppr_index += 1;
  task_status = SPEC;
  ppr_task_init( );
  channel_wait( 2 );
  task_group_succ_fini( );
}

void silent_drop( void ) {
  int x = -1;
  task_status = SEQ;
  BOP_set_group_size( 3 );
  ppr_group_init( );

  spec_order = 1;
  ppr_index = 1;
  task_status = SPEC;
  ppr_task_init( );
  channel_fill( 2, &x, sizeof( int ) );
  x = 1;
  BOP_promise( & x, sizeof( int ) );
  channel_post( 2 );

  x = 0;
  spec_order = 0;
  ppr_index = 0;
  task_status = MAIN;
  ppr_task_init( );
  channel_fill( 1, &x, sizeof( int ) );
  x = 0;
  BOP_promise( & x, sizeof( int ) );
  channel_post( 1 );

  bop_msg( 2, "same datum from two tasks received in reverse sequential order" );
  x = 0;
  spec_order = 2;
  ppr_index = 2;
  task_status = SPEC;
  ppr_task_init( );
  channel_wait( 2 );
  assert( x == 1 );

  channel_wait( 1 );
  assert( x == 1 );

  task_group_succ_fini( );
  bop_msg( 1, "silent drop test finished" );
}

void correctness_check( void ) {
  task_status = SEQ;
  BOP_set_group_size( 2 );
  ppr_pos = PPR;
  int z = 0;

  bop_msg( 2, "correctness checking" );
  ppr_group_init( );
  spec_order = 0;
  ppr_index = 42;
  task_status = MAIN;
  ppr_task_init( );
  channel_fill( &z, &z, sizeof( int ) );
  z = 1;
  BOP_promise( & z, sizeof( int ) );
  channel_post( & z );
  // z = 2;  // uncomment to test modify-after-post conflict handling, next ppr will abort
  // bop_mode = PARALLEL; // uncomment to test modify-after-post conflict handling
  assert( ppr_check_correctness( ) );
  data_commit( );

  z = 0;
  ppr_task_init( );
  spec_order = 1;
  ppr_index = 43;
  task_status = SPEC;
  assert( z != 1 );
  channel_wait( &z );
  assert( z == 1 );
  BOP_use( & z, sizeof( int ) );
  assert( ppr_check_correctness( ) );
  data_commit( );
  task_group_succ_fini( );
}

void ordering_conflict( void ) {
  task_status = SEQ;
  BOP_set_group_size( 3 );
  ppr_pos = PPR;
  int k = 0;

  bop_msg( 2, "ordering conflict" );
  ppr_group_init( );
  spec_order = 0;
  ppr_index = 142;
  task_status = MAIN;
  ppr_task_init( );
  channel_fill( &k, &k, sizeof( int ) );
  k = 1;
  BOP_promise( & k, sizeof( int ) );
  channel_post( & k );
  assert( ppr_check_correctness( ) );
  data_commit( );

  spec_order += 1;
  ppr_index += 1;
  task_status = SPEC;
  ppr_task_init( );
  k = 2;
  BOP_promise( & k, sizeof( int ) );
  assert( ppr_check_correctness( ) );
  data_commit( );

  k = 0;
  spec_order += 1;
  ppr_index += 1;
  task_status = SPEC;
  ppr_task_init( );
  assert( k != 1 );
  channel_wait( &k );
  assert( k == 1 );
  BOP_use( & k, sizeof( int ) );
  assert( ! ppr_check_correctness( ) );
  task_group_succ_fini( );
}

int main( ) {

  interface_tests( );

  silent_drop( );

  correctness_check( );

  ordering_conflict( );

  empty_post( );

  printf("The tests end.\n");
  return 0;  /* must return 0 for rake test to succeed */
}

extern bop_port_t postwait_port;

#include <atomic.h>
typedef struct _channel_t {
  addr_t id;        /* valid if not zero */
  map_t data;
  unsigned sender;  /* non-zero means posted */
  char is_posted;

  /* for channel chaining */
  struct _channel_t *cc_first, *cc_next;
} channel_t;

struct switch_board_t {
  channel_t *channels;   /* first come, first serve */
  unsigned size;         /* no re-sizing during ppr */
  bop_lock_t lock;       /* global lock, needed for channel chaining */
} switch_board;

static void interface_tests( void ) {
  task_status = SEQ;
  BOP_set_group_size( 2 );
  ppr_pos = PPR;
  int x = 0, y = 0;

  bop_msg( 2, "test 1: two pprs, pass one number, one successful and two failed posts" );
  task_status = SEQ;
  ppr_group_init( );
  spec_order = 0;
  task_status = MAIN;
  ppr_task_init( );
  channel_fill( 1, &x, sizeof( int ) );
  x = 1;
  BOP_promise( & x, sizeof( int ) );
  channel_post( 1 );

  x = 2;
  channel_post( 21 );    /* empty post */
  channel_fill( 21, &x, sizeof( int ) );
  channel_post( 21 );    /* resource conflict (depending on the number of channels allocated */
  assert( ! channel_is_posted( 21 ) );
  
  
  x = 0;
  ppr_task_init( );
  spec_order = 1;
  task_status = SPEC;
  assert( x != 1 );
  channel_wait( 1 );
  assert( x == 1 );
  task_group_succ_fini( );

  bop_msg( 2, "test 2: channel chaining, 3 pprs, 4 channels" );
  task_status = SEQ;
  ppr_group_init( );
  spec_order = 0;
  task_status = MAIN;
  ppr_task_init( );
  channel_fill( 1, &x, sizeof( int ) );
  x = 2;
  y = 3;
  channel_fill( 1, &y, sizeof( int ) );
  BOP_promise( & x, sizeof( int ) );
  BOP_promise( & y, sizeof( int ) );
  channel_post( 1 );
  channel_chain( 4, 5 );

  x = 0;
  y = 0;
  ppr_task_init( );
  spec_order = 1;
  task_status = SPEC;
  channel_chain( 1, 2 );
  channel_chain( 2, 4 );
  assert( x != 2 );
  channel_wait( 4 );
  assert( x == 2 );
  assert( y == 3 );

  x = 0;
  y = 0;
  ppr_task_init( );
  spec_order = 1;
  task_status = SPEC;
  assert( x != 2 );
  channel_wait( 2 );
  assert( x == 2 );
  assert( y == 3 );
  task_group_succ_fini( );
}

