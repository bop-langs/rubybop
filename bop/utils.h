/** @file utils.h
 *	@brief Header file for bop utility functions such as
 *	blocking/unblocking, reporting conflicts, and bop messages
 *	
 *	@author Rubybop
 */

#ifndef _BOP_UTILS_H_
#define _BOP_UTILS_H_

#include <signal.h>
#include "bop_api.h"

/**	@return int
 */
int bop_terminal_to_workers();
/**	@return int
 */
int bop_terminal_to_monitor();
/** @param int signo
 *	@return int
 */
int block_signal(int signo);
/** @param int signo
 *	@return int
 */
int unblock_signal(int signo);

/** @brief bop message function that prints if there's a conflict
 *	@param int level
 *	@param const char* msg
 *	@return void
 */
void bop_msg(int level, const char * msg, ...);
/** @param const char* env
 *	@param int min
 *	@param int max
 *	@param int def
 *	@return int
 */
int get_int_from_env(const char* env, int min, int max, int def);

/** @param void
 *	@return void
 */
void msg_init(void);
/**	@param void
 *	@return void
 */
void msg_destroy(void);

/** @param int verbose
 *	@param mem_range_t *c1
 *	@param char *n1
 *	@param mem_range_t *c2
 *	@param char *n2
 *	@return void
 */
void report_conflict(int verbose, mem_range_t *c1, char *n1,
		      mem_range_t *c2, char *n2);

#define nop() asm volatile("nop") /**< so it's 'inlined' */

/** @param mem_range_t *r1
 *	@param mem_range_t *r2
 *	@return char
 */
char mem_range_eq(mem_range_t *r1, mem_range_t *r2);

#include "external/malloc.h"

/**	@param void
 *	@return mspace
 */
mspace mspace_small_new(void);
/** @param void
 *	@return void
 */
mspace mspace_medium_new(void);
/** @param void
 *	@return void
 */
mspace mspace_large_new(void);

#endif
