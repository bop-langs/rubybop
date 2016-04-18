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

static bop_record_t * records = NULL;
static update_node_t * updated_list = NULL;

static inline void update_list(bop_record_t*);

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
  if(is_sequential()) return;
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
    update_list(record);
  }
  //set the ID & id_valid
  if(id_valid){
    record->id = key;
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
//TODO clear out all records matching the object (eg. for all inst vars)
void record_bop_gc(VALUE obj){
  return;
}
#ifdef HAVE_USE_PROMISE
void record_bop_rd_obj(VALUE obj){
  record_bop_access(obj, (ID) 0, false, READ_BIT);
}
void record_bop_wrt_obj(VALUE obj){
  record_bop_access(obj, (ID) 0, false, WRITE_BIT);
}
#endif


//from: http://stackoverflow.com/questions/6943493/hash-table-with-64-bit-values-as-key
uint64_t hash(uint64_t key){
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}

uint64_t hash2(uint64_t obj, uint64_t key){
  return hash(obj) * 3 + hash(key);
}

static inline record_id_t make_record_id(VALUE obj, ID id){
  bop_record_t record = {0};
  record.obj = obj;
  record.id = id;
  return record.record_id;
}

bop_record_t * get_record(VALUE obj, ID id){
  uint probes;
  uint64_t index;
  record_id_t old_record_id;
  record_id_t record_id = make_record_id(obj, id);
  uint64_t base_index = hash2((uint64_t) obj, (uint64_t) id);
  for(probes = 0; probes <= MAX_PROBES; probes++){
    index = (base_index + probes) % MAX_RECORDS;
    if(records[index].record_id == record_id) //already set to this object
      return &records[index];
    else if(records[index].record_id == 0){
      //found un-allocated. Allocate it atomically
      old_record_id = __sync_val_compare_and_swap(&records[index].record_id, NULL, record_id);
      if(old_record_id == 0 || old_record_id == record_id){
        //valid if either this task set it to the corresponding object or if another did
        return &records[index];
      }
    }
  }
  BOP_abort_spec("Couldn't create set up a new access vector for object %lu", obj);
  return NULL;
}

// BOP-ports
void init_obj_monitor(){
  bop_msg(3, "Initializng object monitor");
  records = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  // mmap MAP_ANONYMOUS will already clear all bits
  if(records == MAP_FAILED){
    bop_msg(3, "Failed to initialize object monitor because %s", strerror(errno));
    exit(-1);
  }
  updated_list = NULL;
  bop_msg(3, "Object Monitor initialized");
}

static inline void update_list(bop_record_t * record){
  update_node_t * node = malloc(sizeof(update_node_t));
  node->next = (struct update_node_t *) updated_list;
  node->record = record;
  updated_list = node;
}

int rb_object_correct(){
  update_node_t * node;
  int index = getbasebit();
  for(node = updated_list; node; node = (update_node_t *) node->next){
    int vector = (node->record->vector >> index) & 0xf;
    if((vector & 0x2) && (vector & 0x4)){
      //p0 wrote & p1 read it
      bop_msg(3, "Conflict in rb_object_correct");
      return 0;
    }
    BOP_promise((void*) node->record->obj, sizeof(VALUE));
  }
  bop_msg(3, "Object monitor (rb_object_correct) valid");
  return 1;
}

static void free_list(update_node_t * node){
  if(node->next != NULL)
    free_list((update_node_t *) node->next);
  free(node);
}
void free_all(){
  if(updated_list != NULL)
    free_list(updated_list);
}
void restore_seq(){
  free_all();
  if(records != NULL)
    if(munmap((void*)records, SHM_SIZE) == -1){
      perror("Couldn't unmap the shared mem region");
    }
}
void obj_commit(){
  // update_node_t * node;
  // for(node = updated_list; node != NULL; node = (update_node_t *) node->next){
  // }
  bop_msg(3, "rb obj commit called");
}
void copy_obj(VALUE obj){
  switch(TYPE(obj)){
  case T_ARRAY:
    bop_msg(1, "Copying Array");
    break;
  case T_STRING:
    bop_msg(1, "Copying String");
    break;


  }

}
bop_port_t rb_object_port = {
	.ppr_group_init		= init_obj_monitor,
  .ppr_check_correctness = rb_object_correct,
  .task_group_succ_fini = restore_seq,
  .undy_init = restore_seq,
  .data_commit = obj_commit,
};
