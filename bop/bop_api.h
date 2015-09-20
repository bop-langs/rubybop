/** @file bop_api.h
 *  @brief Behavior-oriented parallelization. See Ding et al. PLDI 2007 and Ke
      et al. OOPSLA 2011. A complete re-write on June 29, 2012.
 *  @author Rubybop
 */

#ifndef _BOP_API_H_
#define _BOP_API_H_

/** DEBUG flags on during BOP */
#ifndef NDEBUG
#define bop_debug(x, ...) bop_msg(1, "%s:%d " x "\n" , __FILE__, __LINE__, ##__VA_ARGS__); /**< We want the task status while debugging->bop_msg */
#else
#define bop_debug(...)
#endif
/** @param void
 *  @return void
 */
void error_alert_monitor(void);

#if defined(BOP)

#include <stdarg.h>  /**< for bop_msg */
#include <assert.h>  /**< for assert */
#include <stdio.h>   /**< for IO */
#include <stdint.h>
typedef uintptr_t addr_t;

#define TRUE 1
#define FALSE 0

/** Task status typedef
 *  Either sequential, understudy, main ppr, or speculative
 */
typedef enum {
  SEQ = -2, /**< sequential mode, ready to change at next PPR */
  UNDY,  /**< the understudy */
  MAIN,  /**< first PPR task after SEQ mode */
  SPEC   /**< a speculation process */
} task_status_t;

/** PPR positive? */
typedef enum {
  PPR = 0,
  GAP,
  PSEQ
} ppr_pos_t;

/** Bop mode */
typedef enum {
  SERIAL = 0,
  PARALLEL
} bop_mode_t;

/** @param void
 *  @return task_status_t
 */
task_status_t BOP_task_status(void);
/** @param void
 *  @return ppr_pos_t
 */
ppr_pos_t BOP_ppr_pos(void);
/** @param void
 *  @return bop_mode_t
 */
bop_mode_t BOP_mode(void);
/** @param void
 *  @return int
 */
int BOP_spec_order(void);
/** @param void
 *  @return int
 */
int BOP_ppr_index(void);

/** system-wide data structure for a memory range */
typedef struct _mem_range_t {
  addr_t base;
  size_t size;
  void *rec;         /**< optional data record */
  unsigned task;     /**< optional ppr_index used by post-wait */
} mem_range_t;

// %------------------ Function Prototype -------------- %
/** @name TOLABEL
 *  @brief the TOLABEL macro by Grant Farmer
 */
///@{
#define TOL(x) L##x
#define TOLABEL(x) TOL(x)
#define TOS(x) #x
#define TOSTRING(x) TOS(x)
#define BOP_ppr_begin(id) if (_BOP_ppr_begin(id)==1) goto TOLABEL(id)
#define BOP_ppr_end(id) _BOP_ppr_end(id); TOLABEL(id):
///@}
#define BOP_group_over(id) _BOP_group_over(id) /**< Only forbids SPEC processes from continuing */

/** @name BOP_Ordered
 *  @brief BOP ordered functions
 */
///@{
/** @param addr_t
 *  @return void
 */
void BOP_ordered_begin( addr_t );
/** @param addr_t
 *  @return void
 */
void BOP_ordered_end( addr_t );
/** @param addr_t
 *  @return void
 */
void BOP_ordered_skip( addr_t );
///@}

/** @name BOP_Groupsize
 *  @brief BOP groupsize functions
 */
///@{
/** @param void
 *  @return int
 */
int BOP_get_group_size(void);
/** @param void
 *  @return int
 */
int BOP_get_verbose(void);
/** @param int sz
 *  @return void
 */
void BOP_set_group_size(int sz);
/** @param int x
 *  @return void
 */
void BOP_set_verbose(int x);
///@}

/** @name Granularity
 *  @brief byte granularity interface
 */
///@{
/** @brief Read access to shared data
 *  @param void* addr
 *  @param size size_t
 *  @return void
 */
void BOP_record_read(void* addr, size_t size);
/** @brief Write access to shared data
 *  @param void* addr
 *  @param size size_t
 *  @return void
 */
void BOP_record_write(void* addr, size_t size);
#define BOP_use( x, y ) BOP_record_read( x, y )
#define BOP_promise( x, y ) BOP_record_write( x, y )
///@}

/** @param void*
 *  @param size_t
 *  @return void
 */
typedef void monitor_t (void *, size_t);

/** @brief called by a speculation process in case of error
 *  @param const char* msg
 *  @return void
 */
void BOP_abort_spec( const char* msg );
/** @brief called by a speculation process in case of error
 *  @param char* msg
 *  @return void
 */
void BOP_abort_next_spec( char* msg );

/** @brief File I/O
 *  @param const char *format
 *  @return int
 */
int BOP_printf(const char *format, ...);
/** @brief File I/O
 *  @param FILE *stream
 *  @param const char *format
 *  @return int
 */
int BOP_fprintf(FILE *stream, const char *format, ...);

/** @brief check if addr has been read
      return the memory range; otherwise NULL
 *  @param void* addr
 *  @return mem_range_t*
 */
mem_range_t *BOP_check_read(void* addr);
/** @brief check if addr has been written
      return the memory range; otherwise NULL
 *  @param void* addr
 *  @return mem_range_t*
 */
mem_range_t *BOP_check_write(void* addr);
/** @brief check if addr has been accessed
      return the memory range; otherwise NULL
 *  @param void* addr
 *  @return mem_range_t*
 */
mem_range_t *BOP_check_access(void* addr);

/** @name Debug
 *  @brief for debug
 */
///@{
/** @param int
 *  @return void
 */ 
void bop_set_verbose( int );
/** @param void
 *  @return int
 */
int bop_get_verbose( void );
/** @param int level
 *  @param const char* msg
 *  @return void
 */
///@}
void bop_msg(int level, const char * msg, ...);
extern short malloc_panic;  /**< Malloc panic mode */
#define bop_assert(x) if(!(x)) {malloc_panic = 1; bop_msg(0, ("Assertion: %s failed, %s:%d %s"), #x, __FILE__, __LINE__, __func__); abort();} /**< Malloc panic mode */

/** For collecting statistics */
typedef struct {
  double start_time;
  int num_by_spec;
  int num_by_main;
  int num_by_undy;
  int data_copied;
  int data_posted;
} stats_t;

extern stats_t bop_stats;

/** @name BOP_malloc
 */
///@{
/** @param size_t sz
 *  @param char *file
 *  @param unsigned line
 *  @return void*
 */
void *_BOP_malloc(size_t sz, char *file, unsigned line);
/** @param size_t n_elements
 *  @param size_t elem_size
 *  @param char *file
 *  @param unsigned line
 *  @return void*
 */
void *_BOP_calloc(size_t n_elements, size_t elem_size, char *file, unsigned line);
/** @param void* mem
 *  @param char* file
 *  @param unsigned line
 *  @return void
 */
void _BOP_free(void* mem, char *file, unsigned line);
/** @param void* mem
 *  @param size_t newsize
 *  @param char* file
 *  @param unsigned line
 *  @return void*
 */
void *_BOP_realloc(void* mem, size_t newsize, char *file, unsigned line);

#include "dmmalloc.h"
#include "malloc_wrapper.h"
#define BOP_malloc( sz )	malloc( sz )
#define BOP_alloc( n, s )	calloc( n, s )
#define BOP_free( m )		free ( m )
#define BOP_realloc( m, nsz )	realloc( m, nsz )
///@}


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

size_t max_ppr_request;
#else

/** @name Original_code
 */
///@{
#define BOP_set_verbose( x )
#define BOP_set_group_size( x )
#define BOP_spec_order( ) 0
#define BOP_ppr_index( ) 0

#define BOP_ppr_begin(id)
#define BOP_ppr_end(id)
#define BOP_ordered_begin( )
#define BOP_ordered_end( )
#define BOP_group_over(id)

#define BOP_record_read( addr, size )
#define BOP_record_write( addr, size )
#define BOP_use( addr, size )
#define BOP_promise( addr, size )

#define bop_msg(ignored, ...) printf( __VA_ARGS__ ) /**< Behaves same in both bop & non-bop */

#define BOP_abort_spec( msg )
#define BOP_abort_next_spec( msg )
#define BOP_abort_spec_group( msg )
#define bop_assert(x) assert(x) /**< Non-bop bop_msg macro */

#include <stdlib.h>
///@}

#endif

#endif
