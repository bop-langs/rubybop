#ifndef __OBJ_MARKING_H
#define __OBJ_MARKING_H

#include "external/malloc.h"

typedef enum{
  READ,
  WRITE,
  READ_AND_WRITE
} mem_op;

typedef struct{
  char * start;
  size_t size;
} data_range;

typedef struct{
  data_range key;
  data_range value;
  void * next; // used in BOP lib
} __raw_kv_entry_t, * kv_entry_t;

typedef struct {
  struct __raw_kv_object_t * next;
  kv_entry_t * reads; //anther pointer because of arrays
  kv_entry_t * writes; // ^ length == GROUPSIZE
} __raw_kv_object_t, *kv_object_t;

void record_str_pr(kv_object_t, mem_op, char * key, char * value);
void record_ind_pr(kv_object_t,mem_op, int ind, void* value, size_t v_size);
void record_str_array(kv_object_t, mem_op, int ind, char * value);
kv_object_t new_obj_info(void);

#endif
