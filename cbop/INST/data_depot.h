#ifndef DATA_DEPOT_H
#define DATA_DEPOT_H

#include <string.h>      // for memcpy
#include <sys/mman.h>    // for mprotect

#include "atomic.h"      // for bop_lock_t
#include "bop_api.h"     // for memaddr_t
#include "bop_map.h"  // for map_t
#include "utils.h"       // min and max

extern mspace meta_space;
#define local_malloc(sz) mspace_malloc(meta_space, sz)

/* Design.
   
   A data depot is a set of collections.  Each collection is a set of
   memory segments and a copy of their contents (in the rec field of
   mem_range_t ).  It replaces the implementation in data_store.[ch],
   which does not support dynamically adding or subtracting from
   multiple collections in the set.
*/

typedef struct _data_depot_t {
  bop_lock_t lock;
  mspace residence;
  char *name;
  shm_map_t **collections;
  unsigned num_cols, max_cols;
} data_depot_t;

data_depot_t *create_data_depot( char *name );

/* It sets up the collection and its segments. The VM data copying is
   specified by the last parameter. */
shm_map_t *depot_add_collection_no_data( data_depot_t *depot, 
					 shm_map_t *col,
					 map_t *new_data);
shm_map_t *depot_add_collection_with_data( data_depot_t *depot, 
					 shm_map_t *col,
					 map_t *new_data);

/* From VM to shared memory.  This method is thread safe and allows
   data from multiple collections be copied into the depot in
   parallel. */
void copy_collection_from_depot( shm_map_t *col );

/* From shared memory to VM. */
void copy_collection_into_depot( shm_map_t *col );

/* This shouldn't happen concurrently with allocation, so no
   locking */
void empty_depot_collection( shm_map_t *col );
void empty_data_depot( data_depot_t *depot );

void data_depot_inspect( int verbose, data_depot_t *depot );

#endif
