#if defined(BOP) || defined(BOP_SEQ) || defined(BOP_NOPROT)
#include <INST/bop_api.h>
#include <INST/bop_malloc.h>


/* the TOLABEL macro by Grant Farmer */
#define TOL(x) L##x
#define TOLABEL(x) TOL(x)

#define TOS(x) #x
#define TOSTRING(x) TOS(x)

#define BOP_ppr_begin(id) if (BOP_PrePPR(id)==1) goto TOLABEL(id)
#define BOP_ppr_end(id) BOP_PostPPR(id); TOLABEL(id):
// #define BOP_ordered_begin( )  BOP_wait( (memaddr_t) ppr_index - 1 )
// #define BOP_ordered_end( addr, size )  BOP_fill_page( (memaddr_t) ppr_index, addr, size ); BOP_post( (memaddr_t) ppr_index )

#define BOP_use( addr, size )  BOP_record_read( addr, size )
#define BOP_promise( addr, size )  BOP_record_write( addr, size )

#define BOPVARDECL(t,x) t __attribute__((aligned (PAGESIZE))) x;
#define BOPARRAYDECL(t,x,n) t __attribute__((aligned (PAGESIZE))) x[n];
#define BOPVARCNAME(x) _bop_init_##x
#define BOPVARPRE(x) static void BOPVARCNAME(x) () __attribute__((constructor (220)));
#define BOPVARBODYST(x) static void BOPVARCNAME(x) () {
#define BOPVARBODYEND(x) }
#define BOPVARREGISTER(x) BOP_global_var(&x, sizeof(x), __FILE__, TOSTRING(__LINE__));

#define BOPVAR(t,x) BOPVARDECL(t,x) BOPVARPRE(x) BOPVARBODYST(x) BOPVARREGISTER(x) BOPVARBODYEND(x)
#define BOPVARINIT(t,x,c) BOPVARDECL(t,x) BOPVARPRE(x) BOPVARBODYST(x) x = c; BOPVARREGISTER(x) BOPVARBODYEND(x)
#define BOPARRAY(t,x,n) BOPARRAYDECL(t,x,n) BOPVARPRE(x) BOPVARPRE(x) BOPVARBODYST(x) BOPVARREGISTER(x) BOPVARBODYEND(x)
#define BOPPRIV(t,x) BOP_global_var_priv(&x, sizeof(x));

// #define malloc(sz) BOP_malloc(sz)
#define BOP_pagefill_malloc( sz ) BOP_valloc( sz )

#define printf BOP_printf
#define fprintf BOP_fprintf
#define scanf BOP_hard_abort(); scanf
#define fscanf BOP_hard_abort(); fscanf
//#define fopen BOP_hard_abort(); fopen

#else

/* original code */
#define BOP_set_verbose( x )
#define BOP_set_group_size( x )
#define BOP_spec_order( ) 0
#define BOP_ppr_index( ) 0

#define BOP_ppr_begin(id)
#define BOP_ppr_end(id)
#define BOP_ordered_begin( )
//#define BOP_ordered_end( addr, size )
#define BOP_ordered_end( )
#define BOP_record_read( addr, size )
#define BOP_record_write( addr, size )

#define BOP_use( addr, size )
#define BOP_promise( addr, size )

#define BOP_wait( id )
#define BOP_fill( id, base, size )
#define BOP_post( id )
#define BOP_fill_page( id, base, size )

/* int ppr_index = 0; */

#define BOPVAR(type, name) type name;
#define BOPPRIV(type, name)
#define BOPARRAY(type, name, number) type name[number];
#define BOPVARINIT(type, name, ival) type name = ival;

//#define BOP_malloc(sz)  malloc(sz)
#define BOP_pagefill_malloc( sz ) valloc( sz )
#define BOP_valloc(sz) valloc(sz)
#define BOP_malloc_priv(sz) malloc(sz)

#define bop_msg(level, string, arg) printf( string, arg )

#define BOP_expose_later(base, size ) 
#define BOP_expose_now( ) 
#define BOP_expose( base, size )
#define BOP_expect( base, size )

#define BOP_protect_range( base, sz, prot ) 
#define BOP_protect_range_set( set, prot )

#define BOP_abort_spec( msg )
#define BOP_AbortSpec( )
#define BOP_abort_next_spec( msg )
#define BOP_hard_abort( msg )

#include <stdlib.h>

#define BOP_malloc(s) malloc(s)
#define BOP_free(s) free(s)

#endif
