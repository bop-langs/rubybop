#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* The program makes an array of randomly initialized integers and adds them together. */

#include "bop_api.h"

#define write(x , y...) printf(y)
#ifdef BOP
extern unsigned int max_ppr;
#else
unsigned int max_ppr = 1 << 20;
#endif

int main(int argc, char ** argv)
{
  unsigned int alloc_size = max_ppr + 100;
  write(1, "dm max size is %u\n", max_ppr);
  write(1, "Allocation size for test %u\n", alloc_size);
  int num_arrays = 5;
  int * some_arrays[num_arrays];
  void * raw;
  int ind = 0;
  for(ind = 0; ind < num_arrays; ind++){
    BOP_ppr_begin(1);
      sleep(2);
      raw = malloc(alloc_size); //something larger
      some_arrays[ind] = raw;
      some_arrays[ind][0] = ind;
      write(1, "allocation %d at : %p\n", ind, raw);
      BOP_promise(&(some_arrays[ind][0]), sizeof(int));
      BOP_promise(&(some_arrays[ind]), sizeof(some_arrays[ind]));
    BOP_ppr_end(1);
  }

  for(ind = 0; ind < num_arrays; ind++){
    write(1, "%d: index %d begins at address %p\n", getpid(), ind, &some_arrays[ind][0]);
    if( some_arrays[ind][0] != ind){
      write(1, "array %d has invalid values! mem not copied.\n", ind);
      return -1;
    }
  }
  return 0;
}
