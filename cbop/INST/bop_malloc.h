#ifndef _BOP_ALLOC_H_
#define _BOP_ALLOC_H_

#include <stddef.h>

/* supporting functions */
void priv_heap_reset(void);
void priv_var_check(void);
void priv_var_reset(void);

/* helpers */
void bop_show_mspace(void *);

#endif
