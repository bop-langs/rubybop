#include <assert.h> /* for assert */
#include <err.h>
#include <stdlib.h> /* for NULL */
#include <errno.h>   /* for pipe draining */

#include "bop_api.h"  /* for memaddr_t and constants */
#include "atomic.h"  // for locking
#include "commit.h"
#include "data_depot.h" /* for data store */
#include "utils.h"  /* for min */

#ifdef BOP_RUBY
#include <ruby/st.h>
map_t *rb_objs;
shm_map_t *rb_mod_union;
#endif

extern map_t *read_map, *write_map;
extern map_t *pw_received;

typedef struct __task_status {
  /* steps of commit */
  bop_lock_t write_map_posted;
  bop_lock_t data_posted;
  bop_lock_t checked;

  /* whether known to be the last task in group */
  char is_last_member;

  /* the write_map pointer, the actual map is in the data depot */
  shm_map_t *collection;

  char space[64];  /* avoid cache block sharing */
} task_status_t;

typedef struct __group_meta_t {
  bop_lock_t lock;

  int group_size;  /* set at the start of the group */

  task_status_t *status;

  bop_lock_t undy_created;
  pid_t undy_pid;
  bop_lock_t undy_conceded;

  /* a flag set by speculation for understudy to read and reset */
  int hard_abort_flag;
} group_meta_t;
  
mspace commit_space;
data_depot_t *commit_data;   /* data store for write maps and written pages */

group_meta_t *group_meta;
shm_map_t *write_union;

static inline void *local_calloc(size_t cnt, size_t sz ) {
  return mspace_calloc( commit_space, cnt, sz );
}
static inline void local_free( void *p ) {
  return mspace_free( commit_space, p );
}

void create_group_meta( void ) {
  char is_shared_mem = 1;  /* true */
  char use_lock = 1;
  size_t init_size = 200000; /* 2KB init size */
  commit_space = create_mspace( init_size, use_lock, is_shared_mem );
  bop_msg( 3, "created commit (shared memory) mspace at %llx", commit_space );
  commit_data = create_data_depot( "commit data depot" );
  write_union = new_shm_no_merge_map( commit_space );
  group_meta = NULL;

#ifdef BOP_RUBY
  rb_objs = new_hash( );
  rb_mod_union = new_shm_hash( commit_space );
#endif
}

static group_meta_ruby_init( void ) {
#ifdef BOP_RUBY
  map_clear( rb_objs );
  map_clear( rb_mod_union );
#endif
}

void group_meta_data_init(int group_size) {
  assert(group_size >= 2);

  if (group_meta != NULL) {
    bop_msg( 1, "group_meta_data_init warning: group_meta should be null but not");
    group_meta_data_free( );
  }

  group_meta = (group_meta_t *) local_calloc( 1, sizeof( group_meta_t ) );
  group_meta->status = (task_status_t *) local_calloc( group_size, 
						    sizeof( task_status_t ) );
  group_meta->group_size = group_size;

  empty_data_depot( commit_data );
  map_clear( write_union );

  group_meta_ruby_init( );

  bop_msg(2, "Meta data 0x%llx allocated supporting %d tasks in a speculation group", group_meta, group_size);
}

void group_meta_data_free(void) {
  local_free( group_meta->status );
  local_free( group_meta );
  group_meta = NULL;

  group_meta_ruby_init( );
}

/* Check my access map with write_union while updating write_union. Return the
   base of a conflict range.  0 if there is no conflict.  */
static memaddr_t check_for_overlap( map_t *my_access, char is_write_map ) {
  int i;
  map_inspect(3, my_access, "my_access");

  mem_range_t *my_ranges; unsigned num;
  map_to_array( my_access, &my_ranges, &num );
  
  for (i = 0; i < num; i ++) {
    mem_range_t *mseg = &my_ranges[ i ];
    mem_range_t access_overlap, recv_overlap;
    char same_access, same_recv;

    bop_msg(4, "testing range %d, from 0x%llx [%d] for %d bytes", i, mseg->base, mseg->base >> PAGESIZEX, mseg->size);

    mem_range_t tseg = *mseg;
    char still_conf_free = 1;
    
    // do {
    same_access = map_overlaps(write_union, &tseg, &access_overlap);
    if (!same_access && is_write_map) 
      map_add_range( write_union, mseg->base, mseg->size, ppr_index, NULL);

    if (same_access) {
      still_conf_free = 0;
      if ( mem_range_eq( &access_overlap, mseg ) ) {

	bop_msg(2, "Overlap with access map from 0x%llx [%lld] for %d bytes", access_overlap.base, access_overlap.base >> PAGESIZEX, access_overlap.size);

	mem_range_t *acc_range = map_contains(write_union, access_overlap.base);
	if ( mem_range_eq( acc_range, &access_overlap) ) {
	  same_recv = map_overlaps(pw_received, &tseg, &recv_overlap);
	  if (same_recv) {
	    bop_msg(3, "Overlap with receive set from 0x%llx [%d] for %d bytes", recv_overlap.base, recv_overlap.base >> PAGESIZEX, recv_overlap.size);

	    if ( mem_range_eq( &access_overlap, &recv_overlap) ) {
	      mem_range_t *recv_range = map_contains( pw_received, recv_overlap.base );
	      if (recv_range->task_id == acc_range->task_id ) {
		bop_msg(2, "The overlap is not a conflict as the page was received from the last writer %d", recv_range->task_id);
		if ( is_write_map )
		  acc_range->task_id = ppr_index;
		still_conf_free = 1;
	      }
	      else 
		bop_msg(2, "The overlap is a post-wait pairing conflict since the page was received from %d but last written by %d", recv_range->task_id, acc_range->task_id );
	    }
	  }
	  else 
	    bop_msg( 1, "Read or write conflict with task %d", acc_range->task_id );
	}
	else 
	  bop_msg( 2, "The range in write_union, %llx for %u bytes, does not equal to the overlap.  Treat it as a conflict", acc_range->base, acc_range->size);
      }
      else 
	bop_msg( 2, "The range in my_access (is_write_map), %llx for %u bytes, does not equal to the overlap.  Treat it as a conflict", is_write_map, mseg->base, mseg->size );

      if ( !still_conf_free) {
	bop_msg( 2, "%d access map (is_write %d):", ppr_index, is_write_map );
	map_inspect( 2, my_access, "my_access" );
	map_inspect( 2, pw_received, "pw_received" );
	return access_overlap.base;
      }
    }
    //} while ( same_access && still_conf_free );
  }
  return 0;
}

#ifdef BOP_RUBY
static int has_conflict = 0;

static void check_object( mem_range_t *rg ) {
  if (has_conflict) return;
  mem_range_t *pred = map_contains( rb_mod_union, rg->base );
  has_conflict = st_ppr_has_conflict( (st_table*) rg->base, 
				      (map_t *) pred->rec );
  if (has_conflict) return;
  st_ppr_copyout_mods( (st_table*) rg->base );
  map_union( (map_t *) pred->rec, ((st_table*) rg->base)->ppr_mod );
}

static void task1_check( mem_range_t *rg ) {
  if ( ((st_table*)rg->base)->ppr_mod->sz == 0 ) return;
  st_ppr_copyout_mods( (st_table *) rg->base );
  map_add_key_obj( rb_mod_union, rg->base, (void*) ((st_table*)rg->base)->ppr_mod );
}

static void obj_merge( mem_range_t *rg ) {
  st_ppr_merge( (st_table*) rg->base, (map_t *)  ((st_table*)rg->base)->ppr_mod );
}

int obj_check_correctness( void ) {
  map_t * my_edits = new_no_merge_map( );
  map_union( my_edits, rb_objs );

  bop_msg( 1, "my_edits has %d items, rb_mod_union has %d", my_edits->sz, rb_mod_union->sz);

  if ( rb_mod_union->sz == 0 ) {
    map_foreach( my_edits, task1_check );
    return 1;
  }

  map_intersect( my_edits, rb_mod_union );
  has_conflict = 0;
  map_foreach( my_edits, check_object );

  if ( has_conflict ) return 0;

  map_foreach( rb_mod_union, obj_merge );
  
  return 1;
}
#endif

/* Return 1 if no conflict found, otherwise 0 */
int check_correctness(int myGroupID, map_t *read_map, map_t *write_map) {

  bop_msg(3,"Check for dependence conflicts");

  memaddr_t ret = check_for_overlap( read_map, 0 /* not write map */ );
  if (ret != 0) {
    bop_msg(1, "Read-after-write conflict.  Address %llx, page %llu.\n", ret, ret >> PAGESIZEX);
    return 0;
  }

  ret = check_for_overlap( write_map, 1 /* write map */ );
#ifndef BOP_RUBY
  if (ret != 0) {
    bop_msg(1, "Write-write conflict.  Address %llx, page %llu.\n", ret, ret >> PAGESIZEX);
    return 0;
  }
#endif

#ifdef BOP_RUBY
  if ( !obj_check_correctness( ) )
    return 0;
  else
#endif
    return 1; /* correctness check passed */
}

void copyout_write_map(int member_id, map_t *my_write_map, 
		       map_t *my_pw_pages) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  group_meta->status[ member_id ].collection = 
    depot_add_collection_no_data( commit_data, NULL, my_write_map );
}

void copyout_written_pages(int member_id ) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  copy_collection_into_depot( group_meta->status[ member_id ].collection );
}

void copyin_update_pages(int sourceGroupID) {
  copy_collection_from_depot( group_meta->status[ sourceGroupID ].collection );
}

void post_undy_created( void ) {
  bop_msg(3,"Send UndyCreated");
  group_meta->undy_created = 1;
  group_meta->undy_pid = getpid();
}

int wait_undy_created(void) {
  bop_msg(2,"Waiting UndyCreated");
  bop_wait_flag( &group_meta->undy_created );
  return group_meta->undy_pid; 
}

void post_undy_conceded(int ppr_id) {
  group_meta->undy_conceded = 1;
}

int wait_undy_conceded(void) {
  bop_msg(2,"Waiting UndyConceded");
  bop_wait_flag( &group_meta->undy_conceded );
  return -1;  /* not really needed */
}

void post_write_map_done(int member_id, int ppr_id) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  group_meta->status[ member_id ].write_map_posted = 1;
  bop_msg(2,"Posted write_map_done for task (group id %d)", member_id);
}

int wait_write_map_done(int member_id) {
  bop_msg(3,"Waiting for write_map_done from spec %d", member_id);
  assert( member_id >= 0 && member_id < group_meta->group_size );
  bop_wait_flag( &group_meta->status[ member_id ].write_map_posted );
  bop_msg(2,"Received write_map_done from %d", member_id);
  return -1;
}

void post_data_done(int member_id, int ppr_id) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  group_meta->status[ member_id ].data_posted = 1;
  bop_msg(2,"Posted data_done for task (group id %d)", member_id);
}

int wait_data_done(int member_id) {
  bop_msg(3,"Waiting for data_done from spec %d", member_id);
  assert( member_id >= 0 && member_id < group_meta->group_size );
  bop_wait_flag( &group_meta->status[ member_id ].data_posted );
  bop_msg(2,"Received data_done from %d", member_id);
  return -1;
}

void post_check_done(int member_id, int ppr_id) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  group_meta->status[ member_id ].checked = 1;
  bop_msg(2,"Posted check_done for task (group id %d)", member_id);
}

int wait_check_done(int member_id) {
  bop_msg(3,"Waiting for check_done from spec %d", member_id);
  assert( member_id >= 0 && member_id < group_meta->group_size );
  bop_wait_flag( &group_meta->status[ member_id ].checked );
  bop_msg(2,"Received check_done from %d", member_id);
  return -1;
}

/* for early termination */
int is_last_member(int member_id) {
  assert( member_id >= 0 && member_id < group_meta->group_size );
  return group_meta->status[ member_id ].is_last_member; 
}

void set_last_member(int member_id) {
  bop_msg( 2, "The group will terminate at task %u", member_id );
  assert( member_id >= 0 && member_id < group_meta->group_size );
  group_meta->status[ member_id ].is_last_member = 1; 
}

int get_hard_abort_flag( void ) {
  return group_meta->hard_abort_flag;
}

void set_hard_abort_flag( void ) {
  group_meta->hard_abort_flag = TRUE;
}

void clear_hard_abort_flag( void ) {
  group_meta->hard_abort_flag = FALSE;
}

void inspect_commit_data_store( verbose ) {
  data_depot_inspect( verbose, commit_data );
}
