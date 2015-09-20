#include "bop_api.h"

#if defined(__LINUX__)
#include <string.h>
#define BYTE_COUNT_T size_t
#elif defined(__APPLE__)
#define BYTE_COUNT_T int
#endif

void *memmove(void *dest, const void *source, size_t num) {
  char holder[num];
  memcpy(holder, source, num);
  memcpy(dest, holder, num);
  BOP_promise(dest, num);
  return dest;
}
