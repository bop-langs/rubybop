#include <assert.h>

#include "../external/malloc.h"  // for mspace
#include "../bop_api.h"  /* for bop_msg */
#include "../bop_map.h"

static map_t *new_empty_map( mspace res ) {
  map_t *ret = mspace_malloc(res, sizeof(map_t));
  ret->residence = res;
  ret->is_hash = 0;
  ret->root = NULL;
  ret->sz = 0;
  return ret;
}

map_t *new_shm_merge_map( mspace res ) {
  map_t *n = new_empty_map( res );
  n->uses_merge = 1;  /* true */
  return n;
}

map_t *new_shm_no_merge_map( mspace res ) {
  map_t *n = new_empty_map( res );
  n->uses_merge = 0;  /* false */
  return n;
}

map_t *new_shm_hash( mspace res ) {
  map_t *n = new_empty_map( res );
  n->uses_merge = 0;
  n->is_hash = 1;
  return n;
}
