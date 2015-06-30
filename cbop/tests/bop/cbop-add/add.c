#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "../../../include/bop.h"
/* The program makes an array of randomly initialized integers and adds them together. */

double *data;
int datasize;

double sum;

int main(int argc, char ** argv)
{
  int i;
  int _blocksize, n, j;
  int _parallelism;

  /* processing the input */
  if (argc>3 || argc<2) {
    printf("Usage: %s array_size-in-millions num-blocks\n", argv[0]);
    exit(1);
  }
  datasize = (int) (atof(argv[1])*1000000);
  assert(datasize>0);
  _parallelism = atoi(argv[2]);
  assert(_parallelism>0);

  /* Initialization */
  printf("%d: initializing %d million numbers\n", getpid(), datasize/1000000);

  data = (double *) malloc(datasize*sizeof(double));
  for (i = 0; i < datasize; i++)
    data[i] = i;

  printf("%d: adding %d million numbers\n", getpid(), datasize/1000000);
  _blocksize = ceil((float)datasize/_parallelism);

  for (i=0; i<datasize; i+=_blocksize) {
	n = i+_blocksize > datasize? datasize:i+_blocksize;
	
	BOP_ppr_begin(1);  /* Begin PPR */
		double sump = 0.0;
		for (j=i; j<n; j++) {
		  sump += sin(data[j])*sin(data[j])+cos(data[j])*cos(data[j]);
		}

		//BOP_ordered_begin();
			BOP_use( &sum, sizeof( double) );
				sum += sump;
			BOP_promise( &sum, sizeof( double ) );
		//BOP_ordered_end();

	BOP_ppr_end(1); /* End PPR */
  }

  printf("%d: %d million numbers added.  The sum is %.0f million (%.0f) \n", getpid(), datasize/1000000, sum/1000000, sum);
}
