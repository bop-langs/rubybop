#ifndef __OBJ_MARKING_H
#define __OBJ_MARKING_H
#include <stdlib.h>
#include <stdbool.h>

typedef enum{
  READ,
  WRITE,
  READ_AND_WRITE
} mem_op;

typedef struct{
  bool is_simple : 1;
  union {
    struct{
      char * start;
      size_t size;
    } complex;
    int simple;
  } data;
} data_range;

typedef struct{
  data_range key;
  data_range value;
  struct __raw_kv_entry_t * next; // used in BOP lib
} __raw_kv_entry_t, * kv_entry_t;

typedef struct {
  struct __raw_kv_object_t * next;
  kv_entry_t * reads; //anther pointer because of arrays
  kv_entry_t * writes; // ^ length == GROUPSIZE
} __raw_kv_object_t, *kv_object_t;

void record_str_pr(kv_object_t, mem_op, char * key, char * value);
void record_array(kv_object_t,mem_op, void* array, int index, size_t array_data_size);
void record_str_array(kv_object_t, mem_op, int ind, char * value);
void record_int_array(kv_object_t, mem_op, int ind, int * array); //no data pointers
void record_int_pair(kv_object_t obj, mem_op op, int ind, int value);

kv_object_t register_new_object(void);

#endif
