#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* The program makes an array of randomly initialized integers and adds them together. */

#include "../../src/bop_api.h"

int *dest;
int sum;

void initialize( int );
void lots_of_allocation_on_block( int start, int end );

int main(int argc, char ** argv)
{
  int i;
  int data_size;
  int num_blocks; 
  int block_size;
 

  /* processing the input */
  if (argc>3 || argc<2) {
    printf("Usage: %s array_size num-blocks\n", argv[0]);
    exit(1);
  }
  data_size = (int) (atof(argv[1]));
  assert(data_size>0);
  num_blocks = atoi(argv[2]);
  assert(num_blocks>0);
  dest = (int*) malloc(sizeof(int)*data_size); 
  int size = data_size; 

  //initialize(data_size );

  printf("%d: adding %d numbers\n", getpid(), data_size);

  block_size = ceil( (float) data_size / num_blocks );
 
  while ( data_size > 0 ) {
    int block_end = data_size;
    int block_begin = data_size >= 0 ? data_size : 0 ;
	data_size -= block_size;
    
	//BOP_ppr_begin(1);  /* Begin PPR */

		lots_of_allocation_on_block(block_begin, block_end);

    //BOP_ppr_end(1);  /* End PPR */
  }
	
	for (i=0; i < size; i++) {
		int diff = 1; 
		//printf("dest: %d   i: %d \n", dest[i], i);	
		//fflush(stdout); fflush(stdout);
		
		//diff = ((dest[i] - i) < .0001); 
		if ( diff ) {
			printf("diff: %d; dest[i]: %d; i: %d \n", diff, dest[i], i);
		}
	
		//assert (dest[i] == i);
	}
  //printf("%d: The sum is %.0f \n", getpid(),  );
  printf("end\n");
 return; 
}

/*
void initialize( int data_size ) {
  int i, _s;
  
  // initialization 
  printf("%d: initializing %d million numbers\n", getpid(), data_size/1000000);

  data = (double *) malloc(data_size*sizeof(double));
  assert(data != NULL);
  
  for(i = 0; i < data_size; i++)
     data[i] = i; 

  sum = 0;
}
*/

void lots_of_allocation_on_block( int start, int end ) {
  int j;
  int *addr; 
  /*
  double total = 0;
  for ( j = start ; j < end ; j ++ ) 
    total += sin(data[ j ]) * sin(data[ j ]) + cos(data[ j ]) * cos(data[ j ]);

  BOP_use( &data[j], sizeof( double )*(end - start) );
  */
  for (j = start; j < end; j++) {
			addr = (int*)malloc(sizeof(int)); 
			*addr = j; 
			dest[j] = *addr; 
			
			BOP_promise( &dest[j], sizeof(int) ); 
  } 		
 
}

