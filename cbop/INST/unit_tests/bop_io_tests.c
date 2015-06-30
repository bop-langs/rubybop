#include "../bop_api.h"
#include "../bop_io.h"
#include <stdio.h>

void do_print( int status, int spec_order ) {
  static count = 0;

  myStatus = status;
  mySpecOrder = spec_order;

  BOP_printf( "stdout: status %d, spec order %d, count %d.\n", 
	      status, spec_order, count);

  BOP_fprintf( stderr, "stderr: status %d, spec order %d, count %d.\n", 
	       status, spec_order, count + 3);

  count ++;
}

void run_test( void ) {

  bop_io_group_init( 3 );

  do_print( MAIN, 0 );

  myStatus = MAIN;
  mySpecOrder = 0;
  /*  BOP_DumpStdout( );
      BOP_DumpStderr( ); */

  do_print( SPEC, 1 );
  do_print( SPEC, 2 );

  do_print( UNDY, -1 );

  myStatus = SPEC;
  mySpecOrder = 2;
  BOP_DumpGroupOutput( );

  myStatus = UNDY;
  BOP_DumpGroupOutput( );
} 

int main( ) {
  bop_io_init( );

  run_test( );

  run_test( );

  abort( );
}
