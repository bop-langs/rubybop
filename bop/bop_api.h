/* Behavior-oriented parallelization. See Ding et al. PLDI 2007 and Ke
   et al. OOPSLA 2011. A complete re-write on June 29, 2012.
*/

#ifndef _BOP_API_H_
#define _BOP_API_H_

#if defined(BOP)

#include <stdarg.h>  /* for bop_msg */
#include <assert.h>  /* for assert */
#include <stdio.h>   /* for IO */

#include <stdint.h>
typedef uintptr_t addr_t;

#define TRUE 1
#define FALSE 0

typedef enum {
  SEQ = -2, // sequential mode, ready to change at next PPR
  UNDY,  // the understudy
  MAIN,  // first PPR task after SEQ mode
  SPEC   // a speculation process
} task_status_t;

typedef enum {
  PPR = 0,
  GAP,
  PSEQ
} ppr_pos_t;

typedef enum {
  SERIAL = 0,
  PARALLEL
} bop_mode_t;

task_status_t BOP_task_status(void);
ppr_pos_t BOP_ppr_pos(void);
bop_mode_t BOP_mode(void);
int BOP_spec_order(void);
int BOP_ppr_index(void);

/* system wide data structure for a memory range */
typedef struct _mem_range_t {
  addr_t base;
  size_t size;
  void *rec;         /* optional data record */
  unsigned task;     /* optional ppr_index used by post-wait */
} mem_range_t;

// %------------------ Function Prototype -------------- %
/* the TOLABEL macro by Grant Farmer */
#define TOL(x) L##x
#define TOLABEL(x) TOL(x)
#define TOS(x) #x
#define TOSTRING(x) TOS(x)
#define BOP_ppr_begin(id) if (_BOP_ppr_begin(id)==1) goto TOLABEL(id)
#define BOP_ppr_end(id) _BOP_ppr_end(id); TOLABEL(id):

void BOP_ordered_begin( addr_t );
void BOP_ordered_end( addr_t );
void BOP_ordered_skip( addr_t );

int BOP_get_group_size(void);
int BOP_get_verbose(void);

void BOP_set_group_size(int sz);
void BOP_set_verbose(int x);

void exec_cleanup(char*);

// int sys_execv(const char *filename, char *const argv[]);
// int sys_execve(const char *filename, char *const argv[], char *const envp[]);

void cleanup_execv(const char *filename, char *const argv[]);
void cleanup_execve(const char *filename, char *const argv[], char *const envp[]);
int sys_execv(const char *filename, char *const argv[]);
int sys_execve(const char *filename, char *const argv[], char *const envp[]);
int execv(const char *filename, char *const argv[]);
int execve(const char *filename, char *const argv[], char *const envp[]);


/* Byte granularity interface */
void BOP_record_read(void* addr, size_t size);
void BOP_record_write(void* addr, size_t size);
#define BOP_use( x, y ) BOP_record_read( x, y )
#define BOP_promise( x, y ) BOP_record_write( x, y )

typedef void monitor_t (void *, size_t);

/* Called by a speculation process in case of error. */
void BOP_abort_spec( const char* msg );
void BOP_abort_next_spec( char* msg );

/* FILE I/O */
int BOP_printf(const char *format, ...);
int BOP_fprintf(FILE *stream, const char *format, ...);

/* Check if addr has been read/written. Return the memory range;
   otherwise NULL*/
mem_range_t *BOP_check_read(void* addr);
mem_range_t *BOP_check_write(void* addr);
mem_range_t *BOP_check_access(void* addr);

// %--------------- For Debug ------------- %
void bop_set_verbose( int );
int bop_get_verbose( void );
void bop_msg(int level, const char * msg, ...);

/* For collecting statistics */
typedef struct {
  double start_time;
  int num_by_spec;
  int num_by_main;
  int num_by_undy;
  int data_copied;
  int data_posted;
} stats_t;

extern stats_t bop_stats;

/* For BOP_malloc */
void *_BOP_malloc(size_t sz, char *file, unsigned line);
void *_BOP_calloc(size_t n_elements, size_t elem_size, char *file, unsigned line);
void _BOP_free(void* mem, char *file, unsigned line);
void *_BOP_realloc(void* mem, size_t newsize, char *file, unsigned line);

#include "dmmalloc.h"
#include "malloc_wrapper.h"
#define BOP_malloc( sz )	malloc( sz )
#define BOP_alloc( n, s )	calloc( n, s )
#define BOP_free( m )		free ( m )
#define BOP_realloc( m, nsz )	realloc( m, nsz )


// Original Code
/*
#define BOP_malloc( sz )  _BOP_malloc( sz, __FILE__, __LINE__ )
#define BOP_calloc( n, s ) _BOP_calloc( n, s, __FILE__, __LINE__ )
#define BOP_free( m ) _BOP_free( m, __FILE__, __LINE__ )
#define BOP_realloc( m, nsz ) _BOP_realloc( m, nsz, __FILE__, __LINE__ )
*/

/* should be disabled if defining BOP_printf printf
#define printf BOP_printf
#define fprintf BOP_fprintf
#define scanf BOP_abort_spec_group(); scanf
#define fscanf BOP_abort_spec_group(); fscanf
*/

//#define BOP_malloc(s) malloc(s)
//#define BOP_free(s) free(s)
//#define BOP_realloc(p, s) realloc(p, s)
//#define BOP_calloc(c, s) calloc(c, s)

#define PAGESIZEX 12

#else

/* original code */
#define BOP_set_verbose( x )
#define BOP_set_group_size( x )
#define BOP_spec_order( ) 0
#define BOP_ppr_index( ) 0

#define BOP_ppr_begin(id)
#define BOP_ppr_end(id)
#define BOP_ordered_begin( )
#define BOP_ordered_end( )

#define BOP_record_read( addr, size )
#define BOP_record_write( addr, size )
#define BOP_use( addr, size )
#define BOP_promise( addr, size )

#define bop_msg(level, string, arg) printf( string, arg )

#define BOP_abort_spec( msg )
#define BOP_abort_next_spec( msg )
#define BOP_abort_spec_group( msg )

#include <stdlib.h>

#endif

#endif
