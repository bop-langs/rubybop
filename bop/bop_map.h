/** @file bop_map.h
 *	@brief Defines functions for the map struct
 *	@author Rubybop
 */

#ifndef _BOP_MAP_H_
#define _BOP_MAP_H_

#include "bop_api.h"
#include "external/malloc.h"

/** range node struct */
typedef struct _range_node_t {
  struct _range_node_t *lc, *rc;
  mem_range_t r;
} range_node_t;

/** map struct */
typedef struct {
  char is_hash;
  mspace residence;   /**< where the map is stored */
  range_node_t *root;
  size_t size;
  char *name;
} map_t;

/** @param map_t *ret
 *	@param char *nm
 *	@return void
 */
void init_empty_map( map_t *ret, mspace res, char *nm );
/**	@param mspace ms
 *	@param char *nm
 *	@return map_t*
 */
map_t *new_range_set( mspace ms, char *nm );
/**	@param map_t *base
 *	@param mspace space
 *	@return map_t*
 */
map_t *map_clone( map_t *base, mspace space );

/**	@param map_t *map
 *	@param addr_t base
 *	@param size_t size
 *	@param void *rec
 *	@return mem_range_t*
 */
mem_range_t *map_add_range( map_t *map, addr_t base, size_t size, void *rec );
/**	@param map_t *map
 *	@param addr_t base
 *	@param size_t size
 *	@param void *obj
 *	@param unsigned ppr_id
 *	@return mem_range_t
 */
mem_range_t *map_add_range_from_task(map_t *map, addr_t base, size_t size, void *obj, unsigned ppr_id);
/**	@param map_t *map
 *	@param addr_t addr
 *	@return mem_range_t*
 */
mem_range_t *map_remove( map_t *map, addr_t addr );

/**	@param map_t *map
 *	@param addr_t addr
 *	@return mem_range_t*
 */
mem_range_t *map_contains( map_t *map, addr_t addr );
/**	@param map_t *map
 *	@param mem_range_t *inp
 *	@param mem_range_t *c_range
 *	@return char
 */
char map_overlaps( map_t *map, mem_range_t *inp, mem_range_t *c_range);

/**	@param map_t *map
 *	@param void (*func)(mem_range_t *)
 *	@return void
 */
void map_foreach( map_t *map, void (*func)(mem_range_t *) );
/**	@param map_t *map
 *	@param void *sum
 *	@param void (*func)(void*, mem_range_t*)
 *	@return void
 */
void map_inject( map_t *map, void *sum, void (*func)( void *, mem_range_t * ) );

/**	@param map_t *map
 *	@param mem_range_t **ranges
 *	@param unsigned *size
 *	@return void
 */
void map_to_array(map_t *map,  mem_range_t **ranges, unsigned *size);

/**	@name Destructive
 *		the ref_map is freed afterwards
 */
///@{
/**	@param map_t *base_map
 *	@param map_t *ref_map
 *	@return void
 */
void map_intersect( map_t *base_map, map_t *ref_map );
/**	@param map_t *base_map
 *	@param map_t *ref_map
 *	@return void
 */
void map_union( map_t *base_map, map_t *ref_map );
///@}

/**	@param map_t *map
 *	@return void
 */
void map_free( map_t *map );
/**	@param map_t *map
 *	@return void
 */
void map_clear( map_t *map );

/**	@param map_t *map
 *	@return size_t
 */
size_t map_size_in_bytes( map_t *map );
/**	@param int verbose
 *	@param map_t *map
 *	@return void
 */
void map_inspect( int verbose, map_t *map );

#endif
