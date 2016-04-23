#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include "ruby.h"
#include "bop_api.h"
#include "bop_ports.h"
#include "object_monitor.h"
#include "gc.h"

static bop_record_t * records = NULL;
static bop_record_copy_t * copy_records = NULL;
static update_node_t * write_list = NULL;
static update_node_t * read_list = NULL;
static update_node_t * ordered_writes = NULL;
static volatile size_t * next_copy_record;

static inline void update_wrt_list(bop_record_t*);
static inline void update_rd_list(bop_record_t*);
static inline void update_ordered_list(bop_record_t*);
static void free_all_lists(void);

/**
 * Record access -- set
 *
 * @method record_access
 *
 * @param  object        The Ruby object (VALUE) that is accessed
 * @param  key           Instance variable key
 * @param  is_valid      is the ID valid? true for instance variable false for objects
 * @param  op            READ_BIT or WRITE_BIT
 */
static inline void record_bop_access(VALUE object, ID key, bool id_valid, int op){
  if(is_sequential() && BOP_task_status() != MAIN) return;
  assert(rb_type(object) != T_FIXNUM);
  assert(op == READ_BIT || op == WRITE_BIT);
  bop_record_t * record = get_record(object, key);
  if(record == NULL){
    assert(BOP_task_status() == MAIN);
    return;
  }
  //update the access vector
  uint64_t update_bit = 1 << (getbasebit() + op);
  uint64_t old_vector = __sync_fetch_and_or(&record->vector, update_bit); //returns the old value
  if( op == WRITE_BIT && (old_vector & update_bit) == 0){
    //if the bit was not set in the old vector, then this is the first write to it
    // add it to the promise list
    if(!in_ordered_region)
      update_wrt_list(record);
    else
      update_ordered_list(record);
  }else if(op == READ_BIT && (old_vector & update_bit) == 0){
    //add to read list
    update_rd_list(record);
  }
  //set the ID & id_valid
  if(id_valid){
#ifdef HAVE_USE_PROMISE
    record->id_valid = id_valid;
#endif
  }
  __sync_synchronize();
}

//utility functions that fill in the parameters for @record_bop_access
void record_bop_rd_id(VALUE obj, ID id){
  record_bop_access(obj, id, true, READ_BIT);
}
void record_bop_wrt_id(VALUE obj, ID id){
  record_bop_access(obj, id, true, WRITE_BIT);
}

static inline void record_bop_gc_pr(VALUE obj, ID inst_id){
  record_id_t record_id;
  bop_record_t * record = get_record(obj, inst_id);
  if(record == NULL) return;
  record_id = record->record_id;
  record->vector = 0;
  __sync_synchronize(); //full memory barrier
  assert(__sync_and_and_fetch(&record->record_id, 0) == 0);
  remove_list(&read_list, record_id);
  remove_list(&write_list, record_id);
}
void record_bop_gc(VALUE obj){
  record_bop_gc_pr(obj, -1); //todo iterate over all of obj's instance variables
#ifdef HAVE_USE_PROMISE
  record_bop_gc_pr(obj, DUMMY_ID); //
#endif
}
#ifdef HAVE_USE_PROMISE
void record_bop_rd_obj(VALUE obj){
  record_bop_access(obj, DUMMY_ID, false, READ_BIT);
}
void record_bop_wrt_obj(VALUE obj){
  record_bop_access(obj, DUMMY_ID, false, WRITE_BIT);
}
#endif


//from: http://stackoverflow.com/questions/6943493/hash-table-with-64-bit-values-as-key
static inline uint64_t hash(uint64_t key){
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}

static inline uint64_t hash2(uint64_t obj, uint64_t key){
  return hash(obj) * 3 + hash(key);
}

static inline record_id_t make_record_id(VALUE obj, ID id){
  bop_record_t record = {0};
  record.obj = obj;
  record.id = id;
  return record.record_id;
}

bop_record_t * get_record(VALUE obj, ID id){
  static const record_id_t UNALLOCATED = 0;
  bool has_gced = false;
  uint64_t probes, index;
  record_id_t old_record_id;
  record_id_t record_id = make_record_id(obj, id);
  uint64_t base_index = hash2((uint64_t) obj, (uint64_t) id);
  search: for(probes = 0; probes <= MAX_PROBES; probes++){
    index = (base_index + probes) % MAX_RECORDS;
    if(records[index].record_id == record_id) //already set to this object
      return &records[index];
    else if(records[index].record_id == UNALLOCATED){
      //found un-allocated. Allocate it atomically
      old_record_id = __sync_val_compare_and_swap(&records[index].record_id,
        UNALLOCATED, record_id);
      if(old_record_id == UNALLOCATED || old_record_id == record_id){
        //valid if either this task set it to the corresponding object or if another did
        return &records[index];
      }
    }
  }
  if(!has_gced){
    has_gced = true;
    rb_gc_start();
    goto search;
  }else{
    BOP_abort_spec("Couldn't create set up a new access vector for object %lu", obj);
    return NULL;
  }
}
// list utilities
static inline void update_wrt_list(bop_record_t * record){
  add_list(&write_list, record);
}
static inline void update_rd_list(bop_record_t * record){
  add_list(&read_list, record);
}
static inline void update_ordered_list(bop_record_t * record){
  add_list(&ordered_writes, record);
}
// BOP-ports
void init_obj_monitor(){
  bop_msg(3, "Initializng object monitor");
  records = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  copy_records = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  // mmap MAP_ANONYMOUS will already clear all bits
  if(records == MAP_FAILED || copy_records == MAP_FAILED){
    bop_msg(3, "Failed to initialize object monitor because %s", strerror(errno));
    exit(-1);
  }
  next_copy_record = &copy_records[0].next;
  *next_copy_record = 1;
  write_list = NULL;
  read_list = NULL;
  bop_msg(3, "Object Monitor initialized");
}
int rb_object_correct(){
  if(BOP_task_status() == MAIN) return true;
  update_node_t * node;
  int write_mask, read_mask, spec_order, index;
  read_mask = 1 << (getbasebit() + READ_BIT);
  spec_order = BOP_spec_order();
  for(node = read_list; node != NULL; node = node->next){
    //if any previous task has written st that I've read, its an error
    for(index = 0; index < spec_order; index++){
      write_mask = 1 << (base_bit_for(index) + WRITE_BIT);
      if( (node->record->vector & write_mask) && (node->record->vector & read_mask) ){
        return false;
      }
    }
  }
  return true;
}

static void free_list(update_node_t * node){
  if(node->next != NULL)
    free_list(node->next);
  free(node);
}
void free_all_lists(){
  if(write_list != NULL)
    free_list(write_list);
  if(read_list != NULL)
    free_list(read_list);
  if(ordered_writes != NULL)
    free_list(ordered_writes);
  read_list = write_list = ordered_writes = NULL;
}
void restore_seq(){
  // if(BOP_spec_order() == BOP_get_group_size() - 1) return;
  if(records != NULL)
    if(munmap(records, SHM_SIZE) == -1){
      perror("Couldn't unmap the shared mem region (records)");
    }
  if(copy_records != NULL)
    if(munmap(copy_records, SHM_SIZE) == -1){
      perror("Couldn't unmap the shared mem region (copy_records)");
    }
  next_copy_record = NULL;
  free_all_lists();
}
void commit(bop_record_t * record){
  //only need to commit the last one
  if(in_ordered_region || is_commiting_writer(record)){
    size_t index = __sync_fetch_and_add(next_copy_record, 1);
    if(index >= MAX_COPYS){
      BOP_abort_spec("Ran out of copy records!");
      return;
    }
    bop_record_copy_t * copy = &copy_records[index];
    copy->obj = record->obj;
    copy->id = record->id;
    copy->val = rb_ivar_get(record->obj, record->id);
  }
}
void parent_merge(){
  if(BOP_task_status() == UNDY) return;
  size_t index;
  bop_record_copy_t * copy;
  bop_msg(3, "Checking %d copy records", *next_copy_record - 1);
  for(index = 1; index < MAX_COPYS && index < *next_copy_record ; index++){
    copy = &copy_records[index];
    bop_msg(3, "Setting 0x%x (type 0x%x) id 0x%x to 0x%x", copy->obj, TYPE(copy->obj), copy->id, copy->val);
    rb_ivar_set(copy->obj, copy->id, copy->val);
  }
  free_all_lists();
}
void obj_commit(){
  if(BOP_task_status() == UNDY) return;
  update_node_t * node;
  bop_msg(3, "rb obj commit");
  uint64_t my_clear_mask = ~((1<<getbasebit() + READ_BIT) | (1<<getbasebit() + WRITE_BIT));
  for(node = write_list; node != NULL; node = node->next){
    commit(node->record);
    __sync_fetch_and_and(&node->record->vector, my_clear_mask);
  }
  for(node = ordered_writes; node != NULL; node = node->next){
    commit(node->record);
    __sync_fetch_and_and(&node->record->vector, my_clear_mask);
  }
  free_all_lists();
}
bop_port_t rb_object_port = {
	.ppr_group_init		= init_obj_monitor,
  .ppr_check_correctness = rb_object_correct,
  .task_group_succ_fini = restore_seq,
  .undy_init = restore_seq,
  .data_commit = obj_commit, //called by all but last puts data in shared mem
  .on_exit_ordered = obj_commit,
  .task_group_commit = parent_merge, //called by last to commit the changes
  .on_enter_ordered = parent_merge
};
