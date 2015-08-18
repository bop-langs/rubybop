#include "bop_api.h"
#include "obj_marking.h"
#include "external/malloc.h"
#include "bop_ports.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

static object_info * objects = NULL; /** head of linked - list*/
static pthread_mutex_t lock; /** Lock for the list operations */
void get_lock();
void release_lock();

#define CAST_SET(var, new_val) var = (typeof(var)) new_val
static inline void record_internal(object_info * obj, void*, void*, size_t, size_t);

/** Utility methods for user programs*/
void record_str_pr(object_info * obj, mem_op op, char * key, char * value){
  size_t ks = strlen(key);
  size_t vs = strlen(value);
  record_internal(obj, op, key, value, ks, vs);
}
void record_ind_pr(object_info * obj, mem_op op, int ind, void* value, size_t v_size){
  size_t ks = sizeof(int);
  record_internal(obj, op, &ind, value, ks, v_size);
}
void record_str_array(object_info * obj, mem_op op, int ind, char * value){
  record_ind_pr(obj,  op, ind, value, strlen(value));
}

object_info * new_obj_info(){
  int gs = BOP_get_group_size();
  object_info * info = malloc(sizeof(object_info));

  CAST_SET(info->next, objects);

  objects = (object_info *) info->next;
  info->mspace = create_mspace(0, 1, 1); /* Default size, locked, shared */
  info->reads = mspace_calloc(info->mspace, sizeof(obj_entry), gs); // new object_info start with no entries
  info->writes = mspace_calloc(info->mspace, sizeof(obj_entry), gs);
  return info;
}
obj_entry * alloc_new_entry(object_info * obj){
  return mspace_calloc(obj->mspace, 1, sizeof(obj_entry));
}

extern int spec_order; //used to index into the array
static inline void record_internal(object_info * obj, mem_op op, void* key, void* value, size_t key_size, size_t value_size){
#define MIRROR(field) new_entry->field = field
  bop_assert(obj != NULL);
  bop_assert(obj->mspace != NULL);
  //fill in all the entry book keeping information
  obj_entry * new_entry = alloc_new_entry(obj);
  MIRROR(key);
  MIRROR(value);
  MIRROR(key_size);
  MIRROR(value_size);
  get_lock();
  //add to the write/read region
  bop_assert(spec_order > 0 && spec_order < BOP_get_group_size()); //sanity checks
  obj_entry * cache = NULL;
  if(mem_op == READ || mem_op == READ_AND_WRITE){
    CAST_SET(new_entry->next, object_info->reads[spec_order]); //I think
    object_info->reads[spec_order] = new_entry;
  }else if(mem_op == WRITE || mem_op == READ_AND_WRITE){
    CAST_SET(new_entry->next, object_info->writes[spec_order]); //I think
    object_info->writes[spec_order] = new_entry;
  }else{
    bop_msg(1, "Unkown mem_op for object-level monitoring");
  }

  release_lock();
#undef MIRROR
}

/**
 Failures:
  1) R/W both not-null and key-value pairs are the same

  Plan (there's lots of for loops)
  If any 2 spec tasks R/W the same KEY pair with different VALUE writes, its a failure.
  The check for this is a little messy because theres lots of loops to check.
*/
inline int mem_range_equal(char * a, char* b, size_t a_size, size_t b_size){
  int i;
  if(a_size != b_size)
    return 0;
  for(i = 0; i < a_size; i++){
    if(a[i] != b[i])
      return 0;
  }
  return 1;
}
inline int entry_conflicts(obj_entry one, obj_entry two){
  if(mem_range_equal(one->key, two->value, one->key_size, two->value_size))
    return 1;
  if(mem_range_equal(two->key, one->value, two->key_size, one->value_size))
    return 1
  return 0;
}
int obj_correct(){
  object_info * object;
  const int gs = BOP_get_group_size();
  int read_index, write_index;
  obj_entry * read_data, * write_data;
  for(object = objects; object != NULL; CAST_SET(object, object->next)){
    for(read_index = 0; read_index < gs; read_index++){
      for(read_data = object->reads[read_index]; read_data != NULL; read_data = CAST_SET(read_data, read_data->next)){
        for(write_index = read_index + 1; write_index < gs; write_index++){
          for(write_data = object->writes[read_index]; write_data != NULL; write_data = CAST_SET(write_data, write_data->next)){
            if(entry_conflicts(read_data, write_data){
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
