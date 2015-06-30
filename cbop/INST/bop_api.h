/* Head file for behavior parallelization.
NOTE: the maximum shared memory segment size is 32M on Linux.
 */

#ifndef _BEHAVEPAR_H_
#define _BEHAVEPAR_H_

#include <stdint.h>

#ifdef __MACH__
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

#include <stdarg.h>  /* for bop_msg */
#include <assert.h>  /* for assert */
#include <signal.h>
#include <stdio.h>   /* for IO */

typedef uintptr_t memaddr_t;
struct _map_t;

#define TRUE 1
#define FALSE 0

// %--------------- Constants ----------------------%
#define PAGESIZE 4096 // memory page size
#define PAGESIZEX 12  // exponent of the page size

#define MAX_GROUP_CAP 16     // largest number for CURR_GROUP_CAP
extern int curr_group_cap;  

/* Task size constants used by group meta data allocation
   (shm_commit.c) */
#define MAX_MOD_PAGES_PER_TASK 50  
/* largest size for the WRITE map */
#define MAX_MAP_SIZE_PER_TASK MAX_MOD_PAGES_PER_TASK

#define PF_ERR_WRITE 2 // write operation type number

#if defined(__LINUX__)
#define REG_ERR 19 // register number having operation type information
#define SIG_MEMORY_FAULT SIGSEGV
#define WRITEOPT(cntxt) ((cntxt)->uc_mcontext.gregs[REG_ERR] & PF_ERR_WRITE)
#elif defined(__MACH__)
#define REG_ERR 13 // register number having operation type information
#define _XOPEN_SOURCE  // see /usr/include/ucontext.h
#define SIG_MEMORY_FAULT SIGBUS
#define MAP_ANONYMOUS MAP_ANON
#define WRITEOPT(cntxt) ((cntxt)->uc_mcontext->__es.__err & PF_ERR_WRITE)
#else
#define REG_ERR 19 // register number having operation type information
#define SIG_MEMORY_FAULT SIGSEGV
#define WRITEOPT(cntxt) ((cntxt)->uc_mcontext.gregs[REG_ERR] & PF_ERR_WRITE)
#endif

typedef enum {
  SEQ,   // sequential mode, at the start or after SPEC abort or UNDY finish
  GAP,   // between PPRs by a parallel task
  MAIN,  // first PPR task after SEQ mode
  SPEC,  // a speculation process
  UNDY,  // the understudy
} Status;
extern volatile Status myStatus;
extern int mySpecOrder;
extern int ppr_index;
extern int bopgroup;

/* object level commit support */
#define FL_OBJ_READ (1<<0)
#define FL_OBJ_MOD (1<<1)
#define FL_OBJ_DEL (1<<2)

#define BOP_FL_SET_P(x,f) (x)=(void*) ((memaddr_t) x | (f))
#define BOP_FL_TEST(x,f) ((memaddr_t) x)&(f)
#define BOP_FL_UNSET_P(x,f) (x)=(void*) ((memaddr_t) x & ~(f))
#define BOP_FL_REVERSE_P(x,f) (x)=(void*) ((memaddr_t) x ^ (f))

/* system wide data structure for a memory range */
typedef struct _mem_range_t {
  memaddr_t base;
  size_t size;
  unsigned task_id;  /* the writer or reader task */
  void *rec;         /* optional data associated with the mem range */
  void *rec2;        /* an optional 2nd pointer (used in exact kill after a sender conflict */
} mem_range_t;

/* BOP type flags */
enum BOP_TYPE {BatchBOP=1, RotatingBOP};
extern enum BOP_TYPE bopType;

// %------------------ Function Prototype -------------- %
int BOP_pre_ppr(int id);
void BOP_post_ppr(int id);

void BOP_set_group_size(int sz);
void BOP_set_verbose(int x);

/* Dependence hints */
/* Post-wait Interface */
void BOP_fill(memaddr_t chid, void *start_addr, size_t size);
void BOP_post( memaddr_t chid );
void BOP_wait( memaddr_t chid );
/* page granularity version of the interface */
void BOP_fill_page(memaddr_t chid, void *start_addr, size_t size);

/* Expose-expect Interface (postwait/exp_board.c) */
void BOP_expose_later( void *base, size_t size );
void BOP_expose_now( );
void BOP_expose( void *base, size_t size );
void BOP_expect( void *base, size_t size );

/* Byte granularity interface */
void BOP_record_read(void* addr, size_t size);
void BOP_record_write(void* addr, size_t size);

/* page granularity version of the interface */
void BOP_record_read_page(void* addr);
void BOP_record_write_page(void* addr);

void BOP_fill_page(memaddr_t chid, void *start_addr, size_t size);

/* Memory protection */
void *BOP_pagefill_malloc(size_t sz);
void *BOP_valloc(size_t sz);
void *BOP_malloc_priv(size_t sz);
void BOP_global_var(void* var, int sz, char* a, char* b);
void BOP_global_var_priv(void *var, int sz, char *a, char *b);

void BOP_protect_range( void *base, size_t sz, int prot );
void BOP_protect_range_set( struct _map_t *set, int prot );

#define PAGESTART(x) (((memaddr_t)(x)>>PAGESIZEX)<<PAGESIZEX)
#define PAGEEND(x) (PAGESTART(x)+PAGESIZE-1)
#define PAGEFILLEDSIZE(sz) PAGESTART((sz) + PAGESIZE - 1)  /* page multiples size of sz, e.g. 1 byte becomes PAGESIZE bytes */
#define PAGEFILLEDSIZE2(addr, sz) PAGEFILLEDSIZE((addr + sz)-PAGESTART(addr)) /* page multiples size of an object starting at addr with sz bytes */ 

/* Called by a speculation process in case of error. 
   FailMe: the current spec has failed.
   FailNextSpec: the current spec is the last to succeed. */
void BOP_abort_spec( char* msg );
void BOP_abort_next_spec( char* msg );
void BOP_hard_abort( char* msg );

/* FILE I/O */
int BOP_printf(const char *format, ...);
int BOP_fprintf(FILE *stream, const char *format, ...);
void BOP_DumpGroupOutput( void );
void BOP_DumpStdout( void );
void BOP_DumpStderr( void );

/* Returns true/false depending whether the page has been read/written
   as recorded by the access map. */
char BOP_check_read(void* addr);
char BOP_check_write(void* addr);
char BOP_check_access(void* addr);

// %--------------- For Debug ------------- %
extern int VERBOSE;
void bop_msg(int level, char * msg, ...);

/* For collecting statistics */
typedef struct {
  double bop_start_time;
  int num_malloc;
  int num_malloc_priv;
  int num_global_var;
  int num_global_priv_var;
  int pages_pushed;
} Stats;

extern Stats bop_stats;

/* For BOP_malloc */
void BOP_malloc_init( int );
void *BOP_malloc(size_t sz);
void* BOP_calloc(size_t n_elements, size_t elem_size);
void BOP_free(void* mem);
void* BOP_realloc(void* mem, size_t newsize);
void BOP_reset( void );
void* shared_malloc(size_t size);
void* shared_calloc(size_t n_elements, size_t elem_size);

#endif
