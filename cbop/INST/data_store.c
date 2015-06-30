#include "data_store.h"

extern map_t *write_map;

inline void init_data_store(data_store_t *store) { 
  bop_lock_clear( & store->lock );
  store->num_collections = 0;
  store->num_segments = 0;
  store->num_bytes = 0;
}

/* Return the index in the segments array.  Allocation is not
   synchronized.  The assumption is that the synchronization is done
   at the collection level. */
unsigned add_segment(data_store_t *store, mem_range_t *seg) {
  unsigned seg_index;

  seg_index = store->num_segments;
  vm_segment_t *new_seg = & (store->segments[ seg_index ]);   
  /* check for overflow */
  if ( seg_index >= MAX_SEGMENTS ) {
    bop_lock_release( & store->lock );
    assert( 0 );
  }
  store->num_segments ++;
    
  new_seg->vm_seg = * seg;
  new_seg->store_addr = & (store->data[ store->num_bytes ]);

  /* check for overflow */
  if (store->num_bytes >= MAX_STORE_SIZE) {
    bop_lock_release( & store->lock );
    assert( 0 );
  }
  store->num_bytes += seg->size;

  return seg_index;
}

collection_t *add_collection_no_data( data_store_t *store, 
				      map_t *vm_data, map_t *pw_pages ) {
  mem_range_t *ranges; unsigned i, num;
  int total = 0;

  bop_lock_acquire( & store->lock );

  /* allocate the collection */
  collection_t * new_col = & store->collections[ store->num_collections ];
  /* check for overflow */
  if (store->num_collections >= MAX_COLLECTIONS) {
    bop_lock_release( & store->lock );
    assert( 0 );
  }
  store->num_collections ++;
  
  map_to_array( vm_data, &ranges, &num);

  new_col->first_seg = &store->segments[ store->num_segments ];
  new_col->num_segs = num;
  for ( i = 0 ; i < num ; i ++ ) {
    mem_range_t target = ranges[i];
    while ( ranges[i].base < target.base + target.size ) {
      mem_range_t orange;
      char overlap;
      if ( pw_pages != NULL)
	overlap = map_overlaps( pw_pages, &ranges[i], &orange );
      else
	overlap = 0;  /* false */

      if ( overlap ) {
	/* use page-size ranges. ensure task id be this task */
	if ( ranges[i].base < orange.base ) {
	  ranges[i].size = orange.base - ranges[i].base;
	  add_segment( store, & ranges[i] );

	  bop_msg(6, "Copy-out range %d starting at 0x%llx (%lld) for %d pages.", i, ranges[i].base, (ranges[i].base >> PAGESIZEX), ranges[i].size >> PAGESIZEX);

	  ranges[i].base = orange.base;
	}
	ranges[i].size = orange.size;

	add_segment( store, & ranges[i] );
	bop_msg(6, "Copy-out range %d starting at 0x%llx (%lld) for %d pages.", i, ranges[i].base, (ranges[i].base >> PAGESIZEX), ranges[i].size >> PAGESIZEX);

	ranges[i].base = orange.base + orange.size;
	ranges[i].size = target.base + target.size - ranges[i].base;
      }
      else {
	add_segment( store, & ranges[i] );
	bop_msg(6, "Copy-out range %d starting at 0x%llx (%lld) for %d pages.", i, ranges[i].base, (ranges[i].base >> PAGESIZEX), ranges[i].size >> PAGESIZEX);
	break;
      }
    } /* end page range loop */
     
    total += target.size;
  } /* end all ranges */
  bop_lock_release( & store->lock );

  free(ranges);
  bop_msg(4, "Copy-out %d range(s) for a total of %d bytes.", num, total); 
  
  return new_col;
}

void copyin_collection_data( collection_t *col ) {
  int i;
  for (i = 0; i < col->num_segs; i ++ ) {
    vm_segment_t *seg = &col->first_seg[ i ];
    if (seg->vm_seg.size == 0) {
      bop_msg(4, "Page %lld is not copied (already transferred by post-wait)", seg->vm_seg.base >> PAGESIZEX );
      continue;
    }

    BOP_protect_range( (void*) seg->vm_seg.base, seg->vm_seg.size, PROT_WRITE );
    map_add_range( write_map, seg->vm_seg.base, seg->vm_seg.size, 
		   ppr_index, NULL );

    /* from the data store to the task */
    memcpy( (void *) seg->vm_seg.base, seg->store_addr, seg->vm_seg.size );

    bop_msg(4, "Copy-in starting page %lld for %lld pages.", seg->vm_seg.base >> PAGESIZEX, seg->vm_seg.size >> PAGESIZEX);
    bop_stats.pages_pushed += seg->vm_seg.size >> PAGESIZEX;
  }
}

void copyout_collection_data( collection_t *col ) {
  int i;
  for (i = 0; i < col->num_segs; i ++ ) {
    vm_segment_t *seg = &col->first_seg[ i ];
    /* from the task to the data store */
    memcpy( seg->store_addr, (void *) seg->vm_seg.base, seg->vm_seg.size );

    bop_msg(6, "Copy-out memory data starting %llx (page %lld) for %u bytes.", seg->vm_seg.base, seg->vm_seg.base >> PAGESIZEX, seg->vm_seg.size );
  }
}

void vm_segment_inspect( int verbose, vm_segment_t *seg ) {

  bop_msg( verbose, "\t\tvm_seg: %llx, page %llu, size %u, store offset %llu", seg->vm_seg.base, seg->vm_seg.base >> PAGESIZEX, seg->vm_seg.size, (memaddr_t) seg->store_addr );

}

void collection_inspect( int verbose, collection_t *col ) {
  bop_msg( verbose, "\tCollection has %u segment(s) from %llx", col->num_segs, col->first_seg );
  vm_segment_t *segs = col->first_seg;
  unsigned j, n = col->num_segs;
  for ( j = 0; j < n; j ++ )
    vm_segment_inspect( verbose, & segs[ j ] );
}

void data_store_inspect( int verbose, data_store_t *store ) {
  assert( store != NULL );

  bop_msg( verbose, "Data store has %u segment(s) in %u collection(s)", store->num_collections, store->num_segments );
  int i;
  for ( i = 0; i < store->num_collections; i++ ) 
    collection_inspect( verbose, & store->collections[ i ] );
}


/* This shouldn't happen concurrently with allocation, so no
   locking */
inline void empty_data_store( data_store_t *store ) {
  init_data_store( store );
} 


