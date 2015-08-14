#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* The program makes an array of randomly initialized integers and adds them together. */

#include "bop_api.h"

double * data;
double sum;

void initialize( int );
double lots_of_computation_on_block( int start, int end );

int main(int argc, char ** argv)
{
  int i, s;
  int data_size, num_blocks;
  int block_size;

  /* processing the input */
  if (argc>3 || argc<2) {
    printf("Usage: %s array_size-in-millions num-blocks\n", argv[0]);
    exit(1);
  }
  data_size = (int) (atof(argv[1])*1000000);
  assert(data_size>0);
  num_blocks = atoi(argv[2]);
  assert(num_blocks>0);

  initialize(data_size );

  printf("%d: adding %d million numbers\n", getpid(), data_size/1000000);

  block_size = ceil( (float) data_size / num_blocks );

  while ( data_size > 0 ) {
    int block_end = data_size;
    data_size -= block_size;
    int block_begin = data_size >= 0 ? data_size : 0 ;

    BOP_ppr_begin(1);  /* Begin PPR */

       double block_sum = lots_of_computation_on_block( block_begin, block_end );

      BOP_ordered_begin( 1 );

          BOP_use( &sum, sizeof( double) );

          sum += block_sum;

          BOP_promise( &sum, sizeof( double ) );

      BOP_ordered_end( 1 );

    BOP_ppr_end(1);  /* End PPR */
  }

  printf("%d: The sum is %.0f million (%.0f) \n", getpid(), sum/1000000, sum);
  return 0;
}


void initialize( int data_size ) {
  int i, _s;

  /* initialization */
  printf("%d: initializing %d million numbers\n", getpid(), data_size/1000000);

  data = (double *) malloc(data_size*sizeof(double));
  assert(data != NULL);

  for(i = 0; i < data_size; i++)
     data[i] = i;

  sum = 0;
}

double lots_of_computation_on_block( int start, int end ) {
  int j;
  double total = 0;
  for ( j = start ; j < end ; j ++ )
    total += sin(data[ j ]) * sin(data[ j ]) + cos(data[ j ]) * cos(data[ j ]);

  BOP_record_read( &data[j], sizeof( double )*(end - start) );

  return total;
}
