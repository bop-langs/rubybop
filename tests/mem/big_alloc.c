#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* The program makes an array of randomly initialized integers and adds them together. */

#include "bop_api.h"

//Alignment based on word size
#if __WORDSIZE == 64
#define ALIGNMENT 8
#elif __WORDSIZE == 32
#define ALIGNMENT 4
#else
#error "need 32 or 64 bit word size"
#endif


#define NUM_CLASSES 16
#define CLASS_OFFSET 4 //how much extra to shift the bits for size class, ie class k is 2 ^ (k + CLASS_OFFSET)
#define MAX_SIZE sizes[NUM_CLASSES - 1]
#define SIZE_C(k) ALIGN((1 << (k + CLASS_OFFSET)))	//allows for iterative spliting
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define write(x, y...) bop_msg(0, y)
// #define write(x , y...) printf(y)

int main(int argc, char ** argv)
{
  unsigned int dm_max_size = SIZE_C(NUM_CLASSES);
  write(1, "dm max size is %u\n", dm_max_size );

  int num_arrays = 5;
  int * some_arrays[num_arrays];
  void * raw;
  int ind = 0;
  for(ind = 0; ind < num_arrays; ind++){
    BOP_ppr_begin(1);
      sleep(2);
      raw = malloc(dm_max_size + 50); //something larger
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
