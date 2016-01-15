#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "bop_api.h"
#include "bop_ports.h"

extern bop_port_t bop_io_port;

/* Print a number apparently from a given process */
void do_print( int n, task_status_t status, int order ) {
  static count = 0;

  /* Sanity test status and spec order */
  switch (status) {
  case UNDY: assert(order == -1); break;
  case MAIN: assert(order ==  0); break;
  case SPEC: assert(order  >  0); break;
  default:   assert(0);
  }

  task_status = status;
  spec_order = order;

  BOP_printf(
      "stdout: %d, status %d, spec order %d, count %d.\n",
      n, task_status, spec_order, count++);

  /* Not yet implemented
  BOP_fprintf( stderr,
      "stderr: %d, status %d, spec order %d, count %d.\n",
      n, task_status, spec_order, count++);
     */
}

/* Print one thing in N PPRs, with a particular side succeeding */
void run_test( task_status_t status, int group_size) {
  assert(status != MAIN); /* Main shouldn't win the race. */

  /* Initialization */
  BOP_set_group_size( group_size );
  bop_io_port.ppr_group_init( );

  /* First PPR prints in MAIN */
  do_print( 0, MAIN, 0 );

  /* Other PPRs print in both a SPEC and UNDY */
  int i;
  for (i = 1; i < group_size; i++) {
    do_print( i, SPEC,  i );
    do_print( i, UNDY, -1 );
  }

  /* Trigger a "win" */
  switch (status) {
  case UNDY:
    bop_io_port.undy_succ_fini();
    break;

  case SPEC:
    bop_io_port.task_group_succ_fini();
    break;

  default:
    assert(0); /* Huh? */
  }
}

int main( ) {
  run_test( SPEC, 3 );
  run_test( UNDY, 3 );

  printf("The tests end.\n");
  abort( );
}
