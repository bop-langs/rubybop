#include <inttypes.h> /* for PRIdPTR */
#include <stdlib.h> /* for NULL */
#include <string.h> /* for memcpy */
#include "bop_api.h"
#include "external/malloc.h"
#include "bop_ports.h"
#include "bop_merge.h"
#include "bop_map.h"

map_t read_map = {.residence = NULL}, write_map;

mspace commit_space = NULL;     /* shared memory space for merge meta data*/
mspace metacow_space = NULL;        /* copy-on-write memory */
map_t *patches = NULL;          /* an array of map_t */
unsigned num_patches;
map_t *write_union = NULL;

static void ppr_reset( void ) {
  map_clear( & read_map );
  map_clear( & write_map );
}

void BOP_record_read(void* addr, size_t size) {
  if ( task_status == UNDY || task_status == SEQ ) return;
  if ( ppr_pos == GAP ) return;
  if ( read_map.residence == NULL ) return;  /* not initialized */

  map_add_range( & read_map, (addr_t) addr, size, NULL );
}

extern char in_ordered_region;
extern map_t ordered_writes;

void BOP_record_write(void* addr, size_t size) {
  if ( task_status == UNDY || task_status == SEQ ) return;
  if ( ppr_pos == GAP ) return;
  if ( read_map.residence == NULL ) return;  /* not initialized */

  map_add_range( & write_map, (addr_t) addr, size, NULL );

  if ( in_ordered_region )
    map_add_range( & ordered_writes, (addr_t) addr, size, NULL );
}

static void free_shm_copy( mem_range_t *r ) {
  mspace_free( commit_space, r->rec );
}

/* Patch initialization is done by individual tasks, and the cleaning
   up is done by the last spec task. */
static void bop_merge_init( void ) {
  if (commit_space == NULL) {
    char use_lock = 1;
    char is_shared_mem = 1;  /* true */
    size_t init_size = 100000000; /* 100MB init size */
    commit_space = create_mspace( init_size, use_lock, is_shared_mem );
    use_lock = 0;
    is_shared_mem = 0;
    write_union = new_range_set( commit_space, "write_union" );
    num_patches = BOP_get_group_size();
    patches =  (map_t *) mspace_calloc( commit_space,
				        num_patches,
					sizeof(map_t) );
    metacow_space = create_mspace( init_size, use_lock, is_shared_mem );
    bop_msg( 3, "created commit (shared memory) mspace at %p", commit_space );
    init_empty_map( & read_map, metacow_space, "read_map" );
    init_empty_map( & write_map, metacow_space, "write_map" );
  }
  else {
    unsigned gs = BOP_get_group_size();
    if ( num_patches < gs )
      patches =  (map_t *) mspace_realloc( commit_space, patches,
					   gs * sizeof(map_t) );
    num_patches = gs;
    map_clear( write_union );
  }

  bop_msg(4, "bop_merge_init: (%d) patches and write union map (re-)initialized", num_patches );
}

static inline char mem_range_eq( mem_range_t *r1, mem_range_t *r2 ) {
  return r1->base == r2->base && r1->size == r2->size;
}

extern map_t received, received_n_used;  /* postwait.c */

addr_t conflict_addr;

/* Check my access map with write_union while updating write_union. Return the
   base of a conflict range.  0 if there is no conflict.  */
static addr_t check_for_overlap( map_t *my_access, char is_write_map ) {
  int i;
  bop_msg( 2, "%d ranges in %s", my_access->size, my_access->name );
  map_inspect(5, my_access);

  if ( write_union->size == 0 ) return 0;

  mem_range_t *my_ranges; unsigned num;
  map_to_array( my_access, &my_ranges, &num );

  for (i = 0; i < num; i ++) {
    mem_range_t *mseg = &my_ranges[ i ];
    mem_range_t access_overlap, recv_overlap;
    char same_access, same_recv;

    bop_msg(4, "testing range %d, from %p ["PRIdPTR"] for %zd bytes", i, mseg->base, mseg->base >> PAGESIZEX, mseg->size);

    mem_range_t tseg = *mseg;
    char still_conf_free = 1;

    // do {
    same_access = map_overlaps(write_union, &tseg, &access_overlap);

    if (same_access) {
      mem_range_t *mod_range = map_contains(write_union, access_overlap.base);
      bop_msg(2, "Overlap with access map from %p ["PRIdPTR"] for %zd bytes, %s %p-%p for %zd bytes, write union %p-%p for %zd bytes", access_overlap.base, access_overlap.base >> PAGESIZEX, access_overlap.size, my_access->name, mseg->base, mseg->base+mseg->size, mseg->size, mod_range->base, mod_range->base + mod_range->size, mod_range->size);

      still_conf_free = 0;
      if ( mem_range_eq( &access_overlap, mseg ) ) {

	mem_range_t *acc_range = map_contains(write_union, access_overlap.base);
	conflict_addr = access_overlap.base;
	if ( mem_range_eq( acc_range, &access_overlap) ) {
	  same_recv = map_overlaps( & received, & tseg, & recv_overlap);
	  if (same_recv) {
	    bop_msg(3, "Overlap with receive set from %p ["PRIdPTR"] for %zd bytes", recv_overlap.base, recv_overlap.base >> PAGESIZEX, recv_overlap.size);

	    if ( mem_range_eq( &access_overlap, &recv_overlap) ) {
	      mem_range_t *recv_range = map_contains( & received, recv_overlap.base );
	      if (recv_range->task == acc_range->task ) {
		bop_msg(2, "The overlap is not a conflict as the data was received from the last writer %d", recv_range->task);
		/* save it to check for modify-after-post conflicts */
		map_add_range_from_task( & received_n_used,
					 acc_range->base, acc_range->size,
					 recv_range->rec, acc_range->task );
		still_conf_free = 1;
	      }
	      else
		bop_msg(2, "The overlap is a post-wait ordering conflict since the page was received from %d but last written by %d", recv_range->task, acc_range->task );
	     }
	  }
	  else
	    bop_msg( 1, "Read or write conflict with a prior task" );
	}
	else {
	  bop_msg( 2, "The range in write_union, %p for %zu bytes, does not equal to the overlap.  Treat it as a conflict", acc_range->base, acc_range->size);
	}
      }
      else
	bop_msg( 2, "The range in %s, %p for %zu bytes, does not equal to the overlap.  Treat it as a conflict", my_access->name, mseg->base, mseg->size );

      if ( !still_conf_free)
	return access_overlap.base;

    }
    //} while ( same_access && still_conf_free );
  }
  return 0;
}

void union_add_range( mem_range_t *range ) {
  map_add_range_from_task( write_union, range->base, range->size, NULL, BOP_ppr_index( ) );
}

/* Return 1 if no conflict found, otherwise 0 */
static int ppr_is_correct( void ) {

  if ( task_status == UNDY || task_status == SEQ ) return 1;

  bop_msg( 3, "%d ranges in %s", write_union->size, write_union->name );
  bop_msg(3,"Check for dependence conflicts");

  addr_t ret = check_for_overlap( & read_map, 0 /* not write map */ );
  if (ret != 0) {
    bop_msg(1, "Read-after-write conflict.  Address %p, page "PRIuPTR".\n", ret, ret >> PAGESIZEX);
    return 0;
  }
  map_foreach( & write_map, union_add_range );

  bop_msg( 3, "%d ranges in %s", write_union->size, write_union->name );
  map_inspect( 5, write_union );

  /* ret = check_for_overlap( write_map, 1 ); union the write map */
  return 1; /* correctness check passed */
}


struct patch_inject_t {
  map_t *patch;
  unsigned total;
};

static void add_range_block( void *_sum, mem_range_t *range ) {
  struct patch_inject_t *sum = (struct patch_inject_t *) _sum;
  char *data_rec;
  data_rec = (char*) mspace_calloc( sum->patch->residence, 1, range->size );
  memcpy( data_rec, (void*) range->base, range->size );
  map_add_range( sum->patch, range->base, range->size, data_rec );
  sum->total += range->size;
}

/* If col is NULL, create a new collection.  Otherwise, augment. */
void create_patch( map_t *patch, map_t *change_set, mspace space ) {
  init_empty_map( patch, space, "patch" );

  struct patch_inject_t sum = {
    .patch = patch,
    .total = 0
  };

  map_inject( change_set, &sum, add_range_block );

  bop_stats.data_copied += sum.total;
  bop_msg(2, "Created a patch with %d range(s) for a total of %d bytes.", patch->size, sum.total);
}

static void apply_range_block( void *_sum, mem_range_t *range ) {
  unsigned *sum = (unsigned *) _sum;
  memcpy( (void *) range->base, range->rec, range->size );
  bop_msg(6, "Copy-in starting %p (page "PRIdPTR") for %zd bytes.", range->base, range->base >> PAGESIZEX, range->size );
  *sum += range->size;
}

void apply_patch( map_t *patch ) {
  unsigned sum = 0;
  map_inject( patch, &sum, apply_range_block );
  bop_stats.data_copied += sum;
  map_clear( patch );                /* cleaning up */
}

static void free_range_data( void *space, mem_range_t *range ) {
  mspace_free( (mspace) space, range->rec );  /* cleaning up */
}

void clear_patch( map_t *patch ) {
  map_inject( patch, (void*) patch->residence, free_range_data );
  map_clear( patch );
}

static void ppr_commit( void ) {
  assert( spec_order >= 0 );
  bop_msg(3, "Data commit start");
  create_patch( & patches[ spec_order ], & write_map, commit_space );
  bop_msg(3, "Data commit done");
}

static void ppr_group_commit( void ) {
  /* copy updated data */
  bop_msg(4, "Copying in modified data.");
  int ppr;
  for (ppr = 0 ; ppr <= spec_order ; ppr ++ ) {
    apply_patch( & patches[ ppr ] );

    bop_msg(4, "Copied modified data from group member %d.", ppr);
  }

  for (ppr = 0 ; ppr <= spec_order ; ppr ++ ) {
    clear_patch( & patches[ ppr ] );
  }

  // map_clear( write_union );   // do it in task group init

}

bop_port_t bop_merge_port = {
  .ppr_group_init        = bop_merge_init,
  .ppr_task_init         = ppr_reset,
  .ppr_check_correctness = ppr_is_correct,
  .data_commit           = ppr_commit,
  .task_group_commit     = ppr_group_commit
};
