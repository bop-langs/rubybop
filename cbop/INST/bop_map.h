#ifndef BOP_MAP_H
#define BOP_MAP_H

#include "bop_api.h"
#include "external/malloc.h"

/* The type for the access map.  It stores a set of non-overlapping
memory ranges.  Each is represented by a [base, size] pair. */

typedef struct _range_node_t {
  struct _range_node_t *lc, *rc;
  mem_range_t r;
} range_node_t;

typedef struct _map_t {
  char uses_merge;
  char is_hash;
  mspace residence;   /* where the map is stored */
  range_node_t *root;
  size_t sz;
} map_t;

typedef map_t shm_map_t;

/* The initializers */

map_t *new_merge_map( void );
map_t *new_no_merge_map( void );
map_t *new_hash( void );

shm_map_t *new_shm_merge_map( mspace residence );
shm_map_t *new_shm_no_merge_map( mspace residence );
map_t *new_shm_hash( mspace residence );

/* A special assumption is that the host access map should only
contain ranges with the same task id.  The new range may overlap with
existing ranges.  After the insertion, there should be one range in
the tree containing the inserted range. */

mem_range_t *map_add_range( map_t *map, memaddr_t base, size_t size, int task, void *obj );

/* Using the map as a key-obj table sorted by the key. There is an
   assertion error if trying to insert a key twice. */
mem_range_t *map_add_key_obj( map_t *map, memaddr_t key, void *obj );
void *map_search_key( map_t *map, memaddr_t key );

/* Remove non-overlapping ranges from base_map.  The operation is
destructive for base_map (no change to the ref_map). */

void map_intersect( map_t *base_map, map_t *ref_map );

/* Return the range that contains the input address; NULL if it is not
in the map.  */

mem_range_t *map_contains( map_t *map, memaddr_t addr );

/* Check if the map overlaps with the input range.  Return a boolean.  The last parameter, c_range, is set to the range of the overlap.  Programming note: if one wants the range in the tree, call map_contains with the base. */

char map_overlaps( map_t *map, mem_range_t *range, mem_range_t *c_range ); 

/* Return all overlapping ranges in the map.  It creates an array of
pointers and returns the array and the size.  The ranges are ordered
by the increasing starting address.  Deallocation of the array is the
responsibility of the caller.  */

void map_range_to_array( map_t *map, memaddr_t base, size_t size, mem_range_t **ranges,  unsigned *ranges_size );

/* Return an array representation of the map. Deallocation of the
array is the responsibility of the caller. */

void map_to_array( map_t *map,  mem_range_t **ranges, unsigned *size );

/* Merge ranges in the ref_map into the base_map.  The operation is
destructive for the base map. */

void map_union( map_t *base_map, map_t *ref_map );

/* Remove a complete node (not sub-ranges) */

void map_remove_node( map_t *map, memaddr_t node_base );

/* A visitor function */

void map_foreach( map_t *map, void (*func)( mem_range_t * ) );

/* Auxiliary functions */

void map_free( map_t *map );

/* Only free the tree nodes not the map record */

inline void map_clear( map_t *map );

/* Return the total number of bytes included in the map */

memaddr_t map_size_in_bytes( map_t *map );


/* a print out to stderr of the number of ranges, and for each range,
the starting address and size in bytes */

unsigned map_inspect( int verbose, map_t *map, char *map_nm ); 

#endif
