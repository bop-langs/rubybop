#include "bop_api.h"
#include "key_value_checks.h"
#include "external/malloc.h"
#include "bop_ports.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

static mspace kv_mspace;
static kv_object_t kv_list = NULL; /** head of linked - list*/
static pthread_mutex_t lock; /** Lock for the list operations */
void get_lock();
void release_lock();


#define SIZE(dr) (dr.data.complex.size)
#define DATA(dr) (dr.data.complex.start)
#define SET(var, new_val) var = (typeof((var))) (new_val)

static inline void record_internal(kv_object_t, mem_op, kv_entry_t);
extern int spec_order; //used to index into the array


//recording functions
static inline void record_internal(kv_object_t obj, mem_op op, kv_entry_t new_entry){
  get_lock();
  //add to the write/read region
  bop_assert(spec_order > 0 && spec_order < BOP_get_group_size()); //sanity checks
  if(op == READ || op == READ_AND_WRITE){
    SET(new_entry->next, obj->reads[spec_order]);
    obj->reads[spec_order] = new_entry;
  }else if(op == WRITE || op == READ_AND_WRITE){
    if(op == READ_AND_WRITE){
      const kv_entry_t prev_entry = new_entry;
      new_entry = mspace_malloc(kv_mspace, sizeof(__raw_kv_entry_t));
      memcpy(new_entry, prev_entry, sizeof(__raw_kv_entry_t));
    }
    SET(new_entry->next, obj->writes[spec_order]);
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
static inline int data_range_eq(data_range read, data_range write){

  int index;

  if(read.is_simple != write.is_simple)
    return 0;
  if(read.is_simple)
    return read.data.simple == write.data.simple;
  //here: both are complex
  if(SIZE(read) != SIZE(write))
    return 0;
  for(index = 0; index < SIZE(read); index++){
    if(DATA(read)[index] != DATA(write)[index])
    return 0;
  }
  return 1;
}
//compare the same part in each struct
static inline int entry_conflicts(kv_entry_t read, kv_entry_t write){
  //only care if keys are equal
  return data_range_eq(read->key, write->key) &&  //accessed the same part of struct
    ! data_range_eq(read->value, write->value); //didn't see the same things
}
//TODO this needs to be stored in hash tables. Lists are bad.
int obj_correct(){
  kv_object_t kv_obj;
  const int gs = BOP_get_group_size();
  int read_index, write_index;
  kv_entry_t read_data, write_data;
  for(kv_obj = kv_list; kv_obj != NULL; SET(kv_obj, kv_obj->next)){
    for(read_index = 0; read_index < gs; read_index++){
      for(SET(read_data, kv_obj->reads[read_index]); read_data != NULL; SET(read_data, read_data->next)){
        for(write_index = read_index + 1; write_index < gs; write_index++){
          for(SET(write_data, kv_obj->writes[write_index]); write_data != NULL; SET(write_data, write_data->next)){
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
static inline typeof(lock) * init_obj_lock(){
  static pthread_mutexattr_t attr; //don't want this too die after the process is over
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&lock, &attr);
  return &lock;
}
static inline void destroy_lock(typeof(lock) * lck){
  pthread_mutex_destroy(lck);
}
static inline void init_mspace(){
  if(!kv_mspace)
    kv_mspace = create_mspace(0, 1, 1); //same as bop io
}

void obj_marking_init(){
  init_obj_lock();
  init_mspace();
}

void obj_marking_teardown(){
  //kill the lock
  destroy_lock(&lock);
  if(!kv_mspace)
    kv_mspace = create_mspace(0, 1, 1); //def. size, locked, shared
}
void get_lock(){
  pthread_mutex_lock(&lock);
}
void release_lock(){
  pthread_mutex_unlock(&lock);
}
kv_object_t register_new_object(){
  init_mspace();
  kv_object_t kvo= mspace_calloc(kv_mspace, 1, sizeof(__raw_kv_object_t));
  SET(kvo->next, kv_list);
  kv_list = kvo;
  return kvo;
}

static inline void * copy_range(void * data, size_t size){
  void * dest = mspace_malloc(kv_mspace, size);
  return memcpy(dest, data, size);
}
static inline void complex_data(data_range * data_range, void * data, size_t len){
  data_range->is_simple = 0;
  data_range->data.complex.start = copy_range(data, len);
  data_range->data.complex.size = len;
}
static inline void simple_data(data_range * data_range, int num){
  data_range->is_simple = 1;
  data_range->data.simple = num;
}
void record_str_pr(kv_object_t obj, mem_op op, char * key, char * value){
  kv_entry_t entry = mspace_malloc(kv_mspace, sizeof(__raw_kv_entry_t));
  complex_data(&entry->key, key, strlen(key));
  complex_data(&entry->value, value, strlen(value));
  record_internal(obj, op, entry);
}
void record_array(kv_object_t obj, mem_op op, void* array, int index, size_t array_data_size){
  kv_entry_t entry = mspace_malloc(kv_mspace, sizeof(__raw_kv_entry_t));
  //for generic arrays, we store the index (KEY) and the (complex) data, array[index].
  //indexing etc for the array[index] in a generic way
  char * chars = (char*) array;
  char * value = chars + (index * array_data_size);
  complex_data(&entry->value, value, array_data_size);
  simple_data(&entry->key, index); //easy part
  record_internal(obj, op, entry);
}
void record_str_array(kv_object_t obj, mem_op op, int ind, char * value){
  kv_entry_t entry = mspace_malloc(kv_mspace, sizeof(__raw_kv_entry_t));
  simple_data(&entry->key, ind);
  complex_data(&entry->value, value, strlen(value));
  record_internal(obj, op, entry);
}
void record_int_array(kv_object_t obj, mem_op op, int ind, int * array){
  record_int_pair(obj, op, ind, array[ind]);
}
void record_int_pair(kv_object_t obj, mem_op op, int ind, int value){
  kv_entry_t entry = mspace_malloc(kv_mspace, sizeof(__raw_kv_entry_t));
  simple_data(&entry->key, ind);
  simple_data(&entry->value, value);
  record_internal(obj, op, entry);
}

bop_port_t object_rw_port = {
  .ppr_check_correctness = obj_correct,
  .ppr_group_init = obj_marking_init
};
