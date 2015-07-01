#ifndef _BOP_UTILS_H_
#define _BOP_UTILS_H_

#include <signal.h>
#include "bop_api.h"

void bop_msg(int level, char * msg, ...);
int get_int_from_env(const char* env, int min, int max, int def);

void report_conflict( int verbose, mem_range_t *c1, char *n1,
		      mem_range_t *c2, char *n2 );

void nop(void);
char mem_range_eq( mem_range_t *r1, mem_range_t *r2 );

#include "external/malloc.h"

mspace mspace_small_new( void );
mspace mspace_medium_new( void );
mspace mspace_large_new( void );

#endif
