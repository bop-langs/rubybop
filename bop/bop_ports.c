#include <stdlib.h> // for NULL
#include <assert.h>
#include "bop_api.h"
#include "bop_ports.h"
#include "bop_ppr_sync.h"

static bop_port_t **bop_ports = NULL;
static unsigned num_ports, alloc_size;

void register_port( bop_port_t *port, char * desc ) {
  assert( BOP_task_status() == SEQ );
  if (bop_ports == NULL) {
    bop_ports = (bop_port_t **) calloc( 10, sizeof(bop_port_t*) );
    alloc_size = 10;
    num_ports = 0;
  }
  else if (num_ports == alloc_size) {
    bop_ports = (bop_port_t **)
      realloc( bop_ports, alloc_size*2*sizeof(bop_port_t*));
    alloc_size *= 2;
  }

  bop_ports[ num_ports ] = port;
  num_ports ++;

  bop_msg( 3, "New BOP port %s (num. %d, %p) is added.",
	   desc, num_ports, port );
}

static int port_call_int_func( int func(void) ) {
  if (func != NULL )
    return ( * func )( );
  else
    return TRUE;
}

static void port_call( void func(void) ) {
  if (func != NULL )
    ( * func )( );
}

void ppr_group_init( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->ppr_group_init );
}
void ppr_task_init( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->ppr_task_init );
}
extern addr_t conflict_addr;
void report_conflict( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
    if ( bop_ports[i]->report_conflict != NULL )
      ( * bop_ports[i]->report_conflict )( conflict_addr );
}
int ppr_check_correctness( void ) {
  int i, passed = TRUE;
  for (i = 0; i < num_ports && passed; i ++)
     passed = passed &&
       port_call_int_func( bop_ports[i]->ppr_check_correctness );
  if ( !passed ) report_conflict( );
  return passed;
}
void data_commit( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->data_commit );
}
void task_group_commit( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->task_group_commit );
}
void task_group_succ_fini( void ) {
  int i;

  bop_msg(3, "Task Group Succ Fini");
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->task_group_succ_fini );
}

void undy_init( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->undy_init );
}

void undy_succ_fini( void ) {
  int i;
  for (i = 0; i < num_ports; i ++) {
    if (bop_ports[i]->undy_succ_fini!=NULL) bop_msg( 1, "bop_ports[%d]->undy_succ_fini", i );
     port_call( bop_ports[i]->undy_succ_fini );
  }
}

/* Port code gen.  See [compiler repos]/tools/scripts/codegen.rb

methods = ["ppr_group_init", "ppr_task_init", "spec_check", "data_commit", "task_group_commit", "task_group_succ_fini", "undy_init", "undy_succ_fini"]

def gen_decl( methods )
  a = "typedef struct {\n"
  methods.each { |m|
    a += "  void (*#{m})( void );\n"
  }
  a += "} bop_port_t;\n"
  puts a
end

def gen_one_func( nm )
  return <<Q
void #{nm}( void ) {
  int i;
  for (i = 0; i < num_ports; i ++)
     port_call( bop_ports[i]->#{nm} );
}
Q
end

def gen_funcs( methods )
  methods.each { |m|
    puts gen_one_func( m )
  }
end

gen_decl(methods)
gen_funcs(methods)

*/
