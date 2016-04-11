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
  volatile VALUE obj; //for checking
  volatile int64_t vector;
} bop_record_t;

typedef struct{
  struct update_node_t * next;
  bop_record_t * record;
} update_node_t;

static volatile bop_record_t  volatile * records = NULL;
static update_node_t * updated_list = NULL;

int getbasebit(void);
volatile int64_t * get_access_vector(VALUE);
void updatelist(bop_record_t*);

void record_bop_rd(VALUE obj){
  volatile int64_t * vector = get_access_vector(obj);
  int64_t bit_index = getbasebit() + READ_BIT;
  int64_t update_bit = 1 << bit_index;
  __sync_fetch_and_or(vector, update_bit); //returns the old value
}
bop_record_t * vector_to_record(int64_t* vector){
  return (bop_record_t *) (((char*) vector) - offsetof(bop_record_t, vector));
}

void record_bop_wrt(VALUE obj){
  volatile int64_t * vector = get_access_vector(obj);
  int64_t bit_index = getbasebit() + WRITE_BIT;
  int64_t update_bit = 1 << bit_index;
  int64_t old_vector = __sync_fetch_and_or(vector, update_bit); //returns the old value
  if( old_vector & ~update_bit){
    //if the above, then this is the first write the this VALUE. this means
    // it must be added to the promise list
    updatelist((bop_record_t*) vector_to_record((int64_t*) vector));
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

volatile int64_t * get_access_vector(VALUE obj){
  uint probes;
  int64_t index;
  VALUE cas_value;
  int64_t base_index = hash((int64_t) obj);
  for(probes = 0; probes <= MAX_PROBES; probes++){
    index = (base_index + probes) % MAX_RECORDS;
    if(records[index].obj == obj) //already set to this object
      return &records[index].vector;
    else if(records[index].obj == 0){
      //found un-allocated. Allocate it atomically
      cas_value = (VALUE) __sync_val_compare_and_swap(&records[index].obj, NULL, obj);
      if(cas_value == 0 || cas_value == obj)
        //valid if either this task set it to the corresponding object or if another did
        return &records[index].vector;
    }
  }
  BOP_abort_spec("Couldn't create set up a new access vector for object!");
  return NULL;
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
  }
  return 1;
}

bop_port_t rb_object_port = {
	.ppr_group_init		= init_obj_monitor,
  .ppr_check_correctness = rb_object_correct,
};
