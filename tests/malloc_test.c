#include "malloc_wrapper.h"
#include <stdio.h>

int main(){
  int count = 0;
  int mod;
  void* ptr;
  for(count = 1; count < 1000; count++){
    mod = count % 3;
    switch (mod) {
      case 0:
        ptr = malloc(count * 3);
        break;
      case 1:
        ptr = calloc(count, 4);
      case 2:
        ptr = malloc(count * 7);
        ptr = realloc(ptr, count * 8);
        ptr = realloc(ptr, count * 3);
        break;
    }
    free(ptr);
  }
  printf("malloc test complete\n");
  return 0;
}
