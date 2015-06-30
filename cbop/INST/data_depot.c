#include <assert.h>
#include "data_depot.h"

extern map_t *write_map, *data_map;

static mspace create_depot_space( size_t init_size ) {
  char is_shared_mem = 1;  /* true */
  char use_lock = 1; 
  mspace new_space = create_mspace( init_size, use_lock, is_shared_mem );
  bop_msg( 3, "created data depot (shared memory) mspace at %llx", new_space );
  return new_space;
}

data_depot_t * create_data_depot( char *name ) {
  assert( name != NULL );
  mspace new_space = create_depot_space( 80000000 ); /* 800M */
  data_depot_t *depot = (data_depot_t *) 
    mspace_calloc( new_space, 1, sizeof( data_depot_t ) );
  bop_lock_clear( & depot->lock );
  depot->residence = new_space;
  depot->name = name;
  depot->collections = mspace_calloc( new_space, 200, sizeof(shm_map_t *) );
  depot->num_cols = 0;
  depot->max_cols = 200;
  bop_msg( 3, "Created data depot %s, %llx in mspace %llx", name, depot, new_space);
  return depot;
}

mspace to_free = NULL;

static void free_shm_copy( mem_range_t *r ) {
  mspace_free( to_free, r->rec );
}

inline void empty_depot_collection( shm_map_t *col ) {
  assert( to_free == NULL );
  to_free = col->residence;
  map_foreach( col, free_shm_copy );
  map_clear( col );
  to_free = NULL;
}

inline void empty_data_depot( data_depot_t *depot ) {
  bop_lock_clear( & depot->lock );
  unsigned i;
  for ( i = 0; i < depot->num_cols; i++ ) {
    shm_map_t *col = depot->collections[ i ];
    empty_depot_collection( col );
  }
  depot->num_cols = 0;
}

/* If col is NULL, create a new collection.  Otherwise, augment. */ 
static inline 
shm_map_t *depot_add_collection( data_depot_t *depot,
				 shm_map_t *col,
				 map_t *vm_data,
				 char copy_data ) {
  int total = 0;

  mem_range_t *ranges; unsigned i, num;
  map_to_array( vm_data, &ranges, &num);

  bop_lock_acquire( & depot->lock );

  if ( col == NULL ) {
    col = new_shm_no_merge_map( depot->residence );
    depot->collections[ depot->num_cols ] = col;
    depot->num_cols ++;
    if ( depot->num_cols == depot->max_cols ) {
      depot->max_cols += 200;
      depot->collections = (shm_map_t **)
	mspace_realloc( depot->residence, depot->collections, 
			depot->max_cols*sizeof(shm_map_t *) );
    }
  }
  assert( !col->uses_merge );  /* can use a merge_map. leave this for another day */
  
  for ( i = 0 ; i < num ; i ++ ) {
    char *data_rec;
    /* allocate space here to permit simultaneous data copying for
       multiple collections in parallel */
    data_rec = (char*) mspace_calloc( depot->residence, 1, ranges[i].size );

    if ( copy_data ) 
      memcpy( data_rec, (void*) ranges[i].base, ranges[i].size );
    
    /* Do not add same data to a depot implicitly, so an overlapping
       range will cause the next call to fail.  */
    map_add_range( col, ranges[i].base, ranges[i].size, ppr_index, data_rec );
    bop_msg(4,"depot_add_collection, col %llx, base %llx, size %lld, ppr_index %d, rec %llx", col, ranges[i].base, ranges[i].size, ppr_index, data_rec );
      
    total += ranges[i].size;
  } /* end all ranges */
  bop_lock_release( & depot->lock );

  free(ranges);
  bop_msg(4, "Copy-out %d range(s) for a total of %d bytes.", col->sz, total); 
  
  return col;
}

shm_map_t *depot_add_collection_no_data( data_depot_t *depot,
					 shm_map_t *col,
					 map_t *vm_data ) {
  return depot_add_collection( depot, col, vm_data, 0 );
}

shm_map_t *depot_add_collection_with_data( data_depot_t *depot,
					   shm_map_t *col,
					   map_t *vm_data ) {
  return depot_add_collection( depot, col, vm_data, 1 );
}

/* no locking here */
void copy_collection_from_depot( shm_map_t *col ) {
  mem_range_t *ranges; unsigned i, num;
  map_to_array( col, &ranges, &num);
  for (i = 0; i < num; i ++ ) {
    if ( ranges[i].size == 0) {
      bop_msg(4, "Data from %llx is not copied (already transferred by post-wait)", ranges[i].base );
      continue;
    }

    BOP_protect_range( (void*) PAGESTART(ranges[i].base), ranges[i].size, PROT_WRITE );
    map_add_range( write_map, ranges[i].base, ranges[i].size, 
		   ppr_index, NULL );

    bop_msg(4, "To copy-in starting %llx (page %lld) for %lld bytes from %llx.", ranges[i].base, ranges[i].base >> PAGESIZEX, ranges[i].size, ranges[i].rec );

    /* from the data depot to the task */
    memcpy( (void *) ranges[i].base, ranges[i].rec, ranges[i].size );

    bop_msg(4, "Copy-in starting %llx (page %lld) for %lld bytes.", ranges[i].base, ranges[i].base >> PAGESIZEX, ranges[i].size );
    bop_stats.pages_pushed += ranges[i].size >> PAGESIZEX;
  }
  free(ranges);

}

void copy_collection_into_depot( shm_map_t *col ) {
  mem_range_t *ranges; unsigned i, num;
  map_to_array( col, &ranges, &num);
  for (i = 0; i < num; i ++ ) {    
    /* from the task to the data depot */
    memcpy( ranges[i].rec, (void *) ranges[i].base, ranges[i].size );

    bop_msg(4, "Copy-out starting %llx (page %lld) for %lld bytes.", ranges[i].base, ranges[i].base >> PAGESIZEX, ranges[i].size );
    bop_stats.pages_pushed += ranges[i].size >> PAGESIZEX;
  }
  free(ranges);
}

void data_depot_inspect( int verbose, data_depot_t *depot ) {
  assert( depot != NULL );

  bop_msg( verbose, "Data depot %s in mspace %llx", depot->name, depot->residence );
}
