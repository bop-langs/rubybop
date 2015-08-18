#ifndef __OBJ_MARKING_H
#define __OBJ_MARKING_H

#include "external/malloc.h"

typedef enum{
  READ,
  WRITE,
  READ_AND_WRITE
} mem_op;

typedef struct{
  char * key;
  char * value;
  size_t key_size;
  size_t value_size;
  struct obj_entry * next; // used in BOP lib
} obj_entry;

typedef struct {
  volatile struct object_info * next;
  obj_entry ** reads; // length == GROUPSIZE
  obj_entry ** writes; // length == GROUPSIZE
  mspace mspace;
} object_info;

void record_str_pr(object_info *, mem_op, char * key, char * value);
void record_ind_pr(object_info *,mem_op, int ind, void* value, size_t v_size);
void record_str_array(object_info *, mem_op, int ind, char * value);
object_info * new_obj_info(void);

#endif
