#ifndef _BOPOMP_H_
#define _BOPOMP_H_

#include "bop.h"
extern int curr_group_cap;
extern struct _map_t *data_map, *read_map, *write_map;

#define BOP_PARALLEL_BEGIN(bop_ppr_id)		\
  { int bop_pi;					\
  int bop_np = curr_group_cap;			\
  int base_i, bop_tile;				\
  for (bop_pi = 0; bop_pi < bop_np; bop_pi++) {	\
  BOP_ppr_begin(bop_ppr_id);			

#define BOP_PARALLEL_END(bop_ppr_id)		\
  BOP_ppr_end(bop_ppr_id);			\
  }						\
  }

#define BOP_FOR(iv, base, bound, step)		\
  bop_tile = (bound - base) / bop_np;		\
  base_i = base + bop_pi * bop_tile;	       	\
  for (iv = base_i; (bop_pi == bop_np - 1) ? (iv < bound) : (iv < base_i + bop_tile); i += step)

#endif
