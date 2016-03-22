#ifndef __HAVE_OBJ_MONITORING
#define __HAVE_OBJ_MONITORING
#include "ruby.h"
typedef struct {
  struct RBasic * basic; //is always the start of the rb struct

} bop_record;

#define CONVERTER(RTYPE, ptr) (struct RTYPE *) ((char*(ptr)) - offset(struct RTYPE), record);


#endif
