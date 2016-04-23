#ifndef __INST_MONITOR_H
#define __INST_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "bop_api.h"
#include "ruby.h"

#define member_size(type, member) (sizeof(((type *)0)->member))

//config options
#define SHM_SIZE (1<<21) //16 4KB pages
#define MAX_RECORDS (((SHM_SIZE) / sizeof(bop_record_t)))
#define MAX_PROBES (MAX_RECORDS)
#define MAX_COPYS (((SHM_SIZE) / sizeof(bop_record_copy_t)))
#define READ_BIT (0)
#define WRITE_BIT (1)
#define MAX_PPR (member_size(bop_record_t, vector) * 4) // vector size in bits over 2 (x 8/2 == 4)

typedef uint128_t record_id_t;
//x86-64 size: 32B with or without use/promise support
typedef struct{
  union{
    struct{
      volatile VALUE obj; //64b
      volatile ID id; //64b
    };
    volatile record_id_t record_id;
  };
  volatile uint64_t vector;
#ifdef HAVE_USE_PROMISE
  bool id_valid;
#endif
} bop_record_t;

typedef union{
    volatile size_t next; //the first item in the list is just used for a next value.
    struct{ //an actual copy struct
      VALUE obj; //host object
      ID id; //the key
      VALUE val; //the value tho update the mapping of
#ifdef WRITER_CHECK
      int last_writer;
#endif
    };
} bop_record_copy_t;

typedef struct{
  void * next; //set to a void pointer to not need excess casts while using the list
  bop_record_t * record;
  record_id_t record_id; //saved here for removale during the GC
} update_node_t;

extern char in_ordered_region;


static inline int base_bit_for(int ppr_ind){
  return ppr_ind * 2;// 2 comes from 2 bits per task
}

static inline int getbasebit(){
  return base_bit_for(BOP_spec_order());
}

bop_record_t * get_record(VALUE, ID);
extern int is_sequential();


void record_bop_rd_id(VALUE, ID);
void record_bop_wrt_id(VALUE, ID);
void record_bop_gc(VALUE);
#ifdef HAVE_USE_PROMISE
void record_bop_rd_obj(VALUE);
void record_bop_wrt_obj(VALUE);
#define DUMMY_ID -1
#endif
//return the spec index of the first writer of the record greater than min_ppr
// i.e. min_ppr + 1 is the first valid return value
// return - 1 on failure / none found
static inline int next_writer(bop_record_t * record, unsigned min_ppr){
  unsigned test_bit;
  unsigned ppr;
  uint64_t vector = record->vector;
  for(ppr = min_ppr + 1; ppr < MAX_PPR; ppr++){
    test_bit = 1 << (base_bit_for(ppr) + WRITE_BIT);
    if(vector & test_bit) return ppr;
  }
  return -1;
}

//return the PPR index of the first writer of the given or -1 if none is found
static inline int first_writer(bop_record_t * record){
  return next_writer(record, -1);
}

static inline int next_reader(bop_record_t * record, unsigned min_ppr){
  unsigned test_bit;
  unsigned ppr;
  uint64_t vector = record->vector;
  for(ppr = min_ppr + 1; ppr < MAX_PPR; ppr++){
    test_bit = 1 << (base_bit_for(ppr) + READ_BIT);
    if(vector & test_bit) return ppr;
  }
  return -1;
}

//return the PPR index of the first writer of the given or -1 if none is found
static inline int first_reader(bop_record_t * record){
  return next_reader(record, -1);
}

static inline int next_accessor(bop_record_t * record, unsigned min_ppr){
  unsigned rd, wrt, ppr;
  uint64_t vector = record->vector;
  for(ppr = min_ppr + 1; ppr < MAX_PPR; ppr++){
    rd = 1 << (base_bit_for(ppr) + READ_BIT);
    wrt = 1 << (base_bit_for(ppr) + WRITE_BIT);
    if(vector & rd || vector & wrt) return ppr;
  }
  return -1;
}

static inline int last_writer(bop_record_t * record){
  unsigned test_bit;
  int ppr;
  uint64_t vector = record->vector;
  for(ppr = MAX_PPR; ppr >= 0; ppr--){
    test_bit = 1 << (base_bit_for(ppr) + WRITE_BIT);
    if(vector & test_bit != 0) return ppr;
  }
  return -1;
}

static inline bool is_commiting_writer(bop_record_t * record){
  return true; //last_writer(record) == BOP_spec_order() && BOP_spec_order() != -1;
}

static inline bool record_id_valid(bop_record_t * record){
#ifdef HAVE_USE_PROMISE
  return record == NULL ? false : record->id_valid;
#else
  return record == NULL ? false : true;
#endif
}

//return the PPR index of the first accessor of the given or -1 if none is found
static inline int first_accessor(bop_record_t * record){
  return next_accessor(record, -1);
}

//update_list utilities
static inline void add_list(update_node_t ** root, bop_record_t * record){
  update_node_t * node = malloc(sizeof(update_node_t));
  node->next = *root;
  node->record = record;
  node->record_id = record->record_id;
  *root = node;
}

static inline void remove_list(update_node_t ** root, record_id_t record_id){
  update_node_t* prev, *current; //for linked-list removal
  for(prev = NULL, current = *root; current != NULL;
      prev = current, current = current->next){
        if(current->record_id == record_id){
          if(prev == NULL)
            *root = current->next;
          else
            prev->next = current->next;
          free(current);
          return;
        }
  }
}

#endif
