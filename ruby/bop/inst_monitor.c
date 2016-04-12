#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include "ruby.h"
#include "bop_api.h"
#include "bop_ports.h"

//config options
#define SHM_SIZE (1<<13) //2 pages (assuming 4kb pages)
#define MAX_RECORDS ((SHM_SIZE) / sizeof(bop_record_t))
#define MAX_PROBES MAX_RECORDS
#define READ_BIT 0
#define WRITE_BIT 1


typedef struct{
  volatile bop_key_t obj; //for checking
  volatile int64_t vector;
} bop_record_t;

typedef struct{
  struct update_node_t * next;
  bop_record_t * record;
} update_node_t;

static bop_record_t * records = NULL;
static update_node_t * updated_list = NULL;


int getbasebit(void);
volatile int64_t * get_access_vector(bop_key_t);
volatile bop_record_t * get_record(bop_key_t);
void updatelist(bop_record_t*);
extern int is_sequential();


void record_bop_rd(bop_key_t obj){
  volatile int64_t * vector = get_access_vector(obj);
  int64_t bit_index = getbasebit() + READ_BIT;
  int64_t update_bit = 1 << bit_index;
  __sync_fetch_and_or(vector, update_bit); //returns the old value
}

void record_bop_wrt(bop_key_t obj){
  if(is_sequential()) return;
    printf("recording write for %p\n", obj);
  volatile bop_record_t * record = get_record(obj);
  volatile int64_t * vector = &record->vector;
  int64_t bit_index = getbasebit() + WRITE_BIT;
  int64_t update_bit = 1 << bit_index;
  int64_t old_vector = __sync_fetch_and_or(vector, update_bit); //returns the old value
  if( (old_vector & update_bit) == 0){
    //if the bit was not set in the old vector, then this is the first write to it
    // add it to the promise list
    updatelist((bop_record_t *) record);
  }
}

//from: http://stackoverflow.com/questions/6943493/hash-table-with-64-bit-values-as-key
int64_t hash(int64_t key){
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}


volatile bop_record_t * get_record(bop_key_t obj){
  uint probes;
  int64_t index;
  bop_key_t cas_value;
  int64_t base_index = hash((int64_t) obj);
  for(probes = 0; probes <= MAX_PROBES; probes++){
    index = (base_index + probes) % MAX_RECORDS;
    if(records[index].obj == obj) //already set to this object
      return &records[index];
    else if(records[index].obj == 0){
      //found un-allocated. Allocate it atomically
      cas_value = (bop_key_t) __sync_val_compare_and_swap(&records[index].obj, NULL, obj);
      if(cas_value == NULL || cas_value == obj)
        //valid if either this task set it to the corresponding object or if another did
        return &records[index];
    }
  }
  BOP_abort_spec("Couldn't create set up a new access vector for object %lu", obj);
  return NULL;
}

inline volatile int64_t * get_access_vector(bop_key_t obj){
  return &get_record(obj)->vector;
}

inline int getbasebit(){
  return BOP_spec_order() * 2; // 2 comes from 2 bits per task
}

void init_obj_monitor(){
  records = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if(records == MAP_FAILED){
    perror("mmap failed");
    exit(-1);
  }
  memset((void*)records, 0, SHM_SIZE);
  updated_list = NULL;
}

void updatelist(bop_record_t * record){
  update_node_t * node = malloc(sizeof(update_node_t));
  node->next = (struct update_node_t *) updated_list;
  node->record = record;
  updated_list = node;
}
//TODO which task calls this function? lower or higher indexed task??
int rb_object_correct(){
  update_node_t * node;
  int index = getbasebit();
  for(node = updated_list; node; node = (update_node_t *) node->next){
    int vector = (node->record->vector >> index) & 0xf;
    if((vector & 0x2) && (vector & 0x4)) //p0 wrote & p1 read it
      return 0;
    BOP_promise((void*) node->record->obj, sizeof(VALUE));
  }
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
  // for(node = updated_list; node; node = (update_node_t *) node->next){
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
