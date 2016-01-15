#ifndef _BOP_UTILS_H_
#define _BOP_UTILS_H_

#include <signal.h>
#include "bop_api.h"

int bop_terminal_to_workers();
int bop_terminal_to_monitor();
int block_signal(int signo);
int unblock_signal(int signo);

void bop_msg(int level, const char * msg, ...);
int get_int_from_env(const char* env, int min, int max, int def);

void msg_init(void);
void msg_destroy(void);

void report_conflict( int verbose, mem_range_t *c1, char *n1,
		      mem_range_t *c2, char *n2 );

#define nop() asm volatile("nop") /** so it's 'inlined' */

char mem_range_eq( mem_range_t *r1, mem_range_t *r2 );

#include "external/malloc.h"

mspace mspace_small_new( void );
mspace mspace_medium_new( void );
mspace mspace_large_new( void );

#endif
