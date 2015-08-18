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
  struct key_val_entry * next; // used in BOP lib
} key_val_entry;

typedef struct {
  volatile struct key_val_object * next;
  key_val_entry ** reads; // length == GROUPSIZE
  key_val_entry ** writes; // length == GROUPSIZE
  mspace mspace;
} key_val_object;

void record_str_pr(key_val_object *, mem_op, char * key, char * value);
void record_ind_pr(key_val_object *,mem_op, int ind, void* value, size_t v_size);
void record_str_array(key_val_object *, mem_op, int ind, char * value);
key_val_object * new_obj_info(void);

#endif
