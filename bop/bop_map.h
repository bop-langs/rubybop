#ifndef _BOP_MAP_H_
#define _BOP_MAP_H_

#include "bop_api.h"
#include "external/malloc.h"

typedef struct _range_node_t {
  struct _range_node_t *lc, *rc;
  mem_range_t r;
} range_node_t;

typedef struct {
  char is_hash;
  mspace residence;   /* where the map is stored */
  range_node_t *root;
  size_t size;
  char *name;
} map_t;

void init_empty_map( map_t *ret, mspace res, char *nm );
map_t *new_range_set( mspace ms, char *nm );
map_t *map_clone( map_t *base, mspace space );

mem_range_t *map_add_range( map_t *map, addr_t base, size_t size, void *rec );
mem_range_t *map_add_range_from_task(map_t *map, addr_t base, size_t size, void *obj, unsigned ppr_id);
mem_range_t *map_remove( map_t *map, addr_t addr );

mem_range_t *map_contains( map_t *map, addr_t addr );
char map_overlaps( map_t *map, mem_range_t *inp, mem_range_t *c_range);

void map_foreach( map_t *map, void (*func)(mem_range_t *) );
void map_inject( map_t *map, void *sum, void (*func)( void *, mem_range_t * ) );

void map_to_array(map_t *map,  mem_range_t **ranges, unsigned *size);

/* Destructive: the ref_map is freed afterwards */
void map_intersect( map_t *base_map, map_t *ref_map );
void map_union( map_t *base_map, map_t *ref_map );

void map_free( map_t *map );
void map_clear( map_t *map );

size_t map_size_in_bytes( map_t *map );
void map_inspect( int verbose, map_t *map );

#endif
