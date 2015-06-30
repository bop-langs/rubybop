#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <string.h>      // for memcpy
#include <sys/mman.h>    // for mprotect
#include "external/malloc.h"
extern mspace meta_space;
#define local_malloc(sz) mspace_malloc(meta_space, sz)

#include "atomic.h"      // for bop_lock_t
#include "bop_api.h"     // for memaddr_t
#include "bop_map.h"  // for map_t
#include "utils.h"       // min and max


/* Design.
   
   A data store is a continuous chunk of (shared) memory that holds
   pieces of data in a number of address spaces (e.g. written data by
   ppr tasks or communicated data by a number of posts) in.  A store
   is an array of collections.  A collection has an array of
   virtual-memory segments.  A segment is a continuous series of
   bytes.  A store is expanded one collection at a time.  All store
   data is freed (cleared) together.  */

/* Implementation.

   Collections are returned as object pointers.  It holds a series of
   segments (in the segments array) by recording the start index and
   the number of segments.  Similarly a segment holds a series of
   bytes (in the data array). */

#define MAX_STORE_SIZE (MAX_MOD_PAGES_PER_TASK*MAX_GROUP_CAP) << PAGESIZEX
#define MAX_SEGMENTS_PER_TASK MAX_MOD_PAGES_PER_TASK
#define MAX_SEGMENTS (MAX_SEGMENTS_PER_TASK * MAX_GROUP_CAP)
#define MAX_POSTWAITS_PER_TASK 10
#define MAX_COLLECTIONS (MAX_POSTWAITS_PER_TASK * MAX_GROUP_CAP)

typedef struct _vm_segment_t {
  mem_range_t vm_seg;
  char *store_addr;
} vm_segment_t;

typedef struct _collection_t {
  vm_segment_t *first_seg;
  unsigned num_segs;
} collection_t;

typedef struct _data_store_t {
  bop_lock_t lock;
  collection_t collections[ MAX_COLLECTIONS ];
  unsigned num_collections;
  vm_segment_t segments[ MAX_SEGMENTS ];
  unsigned num_segments;
  char data[ MAX_STORE_SIZE ];
  memaddr_t num_bytes;
} data_store_t;

inline void init_data_store(data_store_t *store);

/* It sets up the collection and its segments. The VM data copying is
   not done here. */
collection_t *add_collection_no_data( data_store_t *store, 
				      map_t *vm_data, map_t *pw_pages );

/* From VM to shared memory.  This method is thread safe and allows
   data from multiple collections be copied into the store in
   parallel. */
void copyin_collection_data( collection_t *col );

/* From shared memory to VM. */
void copyout_collection_data( collection_t *col );

/* This shouldn't happen concurrently with allocation, so no
   locking */
inline void empty_data_store( data_store_t *store );

void data_store_inspect( int verbose, data_store_t *store );
void collection_inspect( int verbose, collection_t *col );
void vm_segment_inspect( int verbose, vm_segment_t *vms );

#endif
