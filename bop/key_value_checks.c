#include "bop_api.h"
#include "key_value_checks.h"
#include "external/malloc.h"
#include "bop_ports.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

static key_val_object * objects = NULL; /** head of linked - list*/
static pthread_mutex_t lock; /** Lock for the list operations */
void get_lock();
void release_lock();

#define CAST_SET(var, new_val) var = (typeof((var))) (new_val) //who needs type safety?
static inline void record_internal(key_val_object * obj, mem_op, void*, void*, size_t, size_t);

/** Utility methods for user programs*/
void record_str_pr(key_val_object * obj, mem_op op, char * key, char * value){
  size_t ks = strlen(key);
  size_t vs = strlen(value);
  record_internal(obj, op, key, value, ks, vs);
}
void record_ind_pr(key_val_object * obj, mem_op op, int ind, void* value, size_t v_size){
  size_t ks = sizeof(int);
  record_internal(obj, op, &ind, value, ks, v_size);
}
void record_str_array(key_val_object * obj, mem_op op, int ind, char * value){
  record_ind_pr(obj,  op, ind, value, strlen(value));
}


key_val_entry * make_new_entry(key_val_object * obj, void* key, void* value, size_t key_size, size_t value_size){
#define MIRROR(field) new_entry->field = field
  key_val_entry * new_entry = mspace_calloc(obj->mspace, 1, sizeof(key_val_entry));
  MIRROR(key_size);
  MIRROR(value_size);
  MIRROR(key);
  MIRROR(value);
  return new_entry;
#undef MIRROR
}


extern int spec_order; //used to index into the array
static inline void record_internal(key_val_object * obj, mem_op op, void* key, void* value, size_t key_size, size_t value_size){
  bop_assert(obj->mspace != NULL);
  //fill in all the entry book keeping information
  key_val_entry * new_entry = make_new_entry(obj, key, value, key_size, value_size);
  get_lock();
  //add to the write/read region
  bop_assert(spec_order > 0 && spec_order < BOP_get_group_size()); //sanity checks
  if(op == READ || op == READ_AND_WRITE){
    CAST_SET(new_entry->next, obj->reads[spec_order]); //I think
    obj->reads[spec_order] = new_entry;
  }else if(op == WRITE || op == READ_AND_WRITE){
    if(op == READ_AND_WRITE){
      //entries need to be in seperate list or have next_read and next_write fields
      new_entry = make_new_entry(obj, key, value, key_size, value_size);
    }
    CAST_SET(new_entry->next, obj->writes[spec_order]); //I think
    obj->writes[spec_order] = new_entry;
  }else{
    bop_msg(1, "Unkown mem_op for object-level monitoring");
  }

  release_lock();
}

/**
Failures:
1) R/W both not-null and key-value pairs are the same

Plan (there's lots of for loops)
If any 2 spec tasks R/W the same KEY pair with different VALUE writes, its a failure.
The check for this is a little messy because theres lots of loops to check.
*/
static inline int mem_range_equal(char * a, char* b, size_t a_size, size_t b_size){
  int i;
  if(a_size != b_size)
    return 0;
  for(i = 0; i < a_size; i++){
    if(a[i] != b[i])
    return 0;
  }
  return 1;
}

static inline int entry_conflicts(key_val_entry * one, key_val_entry * two){
  return mem_range_equal(one->key, two->value, one->key_size, two->value_size) ||
  mem_range_equal(two->key, one->value, two->key_size, one->value_size);
}

int obj_correct(){
  key_val_object * object;
  const int gs = BOP_get_group_size();
  int read_index, write_index;
  key_val_entry * read_data, * write_data;
  for(object = objects; object != NULL; CAST_SET(object, object->next)){
    for(read_index = 0; read_index < gs; read_index++){
      for(CAST_SET(read_data, object->reads[read_index]); read_data != NULL; CAST_SET(read_data, read_data->next)){
        for(write_index = read_index + 1; write_index < gs; write_index++){
          for(CAST_SET(write_data, object->writes[write_index]); write_data != NULL; CAST_SET(write_data, write_data->next)){
            if( entry_conflicts(read_data, write_data) ){
              return 0;
            }
          }
        }
      }
    }
  }
  //loop through everything and check for conflicts.
  return 1;
}
//inter-process locking set-up
static inline typeof(lock) * initialize_obj_lock(){
  static pthread_mutexattr_t attr; //don't want this too die after the process is over
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&lock, &attr);
  return &lock;
}
static inline void destroy_lock(typeof(lock) * lck){
  pthread_mutex_destroy(lck);
}
void obj_marking_init(){
  initialize_obj_lock();
}

void obj_marking_teardown(){
  //kill the lock
  destroy_lock(&lock);
}
void get_lock(){
  pthread_mutex_lock(&lock);
}
void release_lock(){
  pthread_mutex_unlock(&lock);
}
bop_port_t object_rw_port = {
  .ppr_check_correctness = obj_correct,
  .ppr_group_init = obj_marking_init
};
