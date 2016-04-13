#ifndef __INST_MONITOR_H
#define __INST_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "bop_api.h"
#include "ruby.h"

#define member_size(type, member) (sizeof(((type *)0)->member))



//config options
#define SHM_SIZE (1<<13) //2 pages (assuming 4kb pages)
#define MAX_RECORDS (((SHM_SIZE) / sizeof(bop_record_t)))
#define MAX_PROBES (MAX_RECORDS)
#define READ_BIT (0)
#define WRITE_BIT (1)
#define MAX_PPR (member_size(bop_record_t, vector) * 4) // vector size in bits over 2 (x 8/2 == 4)

typedef struct{
  volatile VALUE obj; //for checking
  volatile uint64_t vector;
  volatile ID id;
  bool id_valid;
} bop_record_t;

typedef struct{
  struct update_node_t * next;
  bop_record_t * record;
} update_node_t;


static inline int base_bit_for(int ppr_ind){
  return ppr_ind * 2;// 2 comes from 2 bits per task
}

static inline int getbasebit(){
  return base_bit_for( BOP_spec_order());
}

bop_record_t * get_record(VALUE);
extern int is_sequential();


void record_bop_rd_id(VALUE, ID);
void record_bop_wrt_id(VALUE, ID);

void record_bop_rd_obj(VALUE);
void record_bop_wrt_obj(VALUE);

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

//return the PPR index of the first accessor of the given or -1 if none is found
static inline int first_accessor(bop_record_t * record){
  return next_accessor(record, -1);
}


#endif
