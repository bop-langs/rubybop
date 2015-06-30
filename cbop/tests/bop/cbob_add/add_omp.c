#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* The program makes an array of randomly initialized integers and adds them together. */

double *data;
int datasize;

double sum;

int main(int argc, char ** argv)
{
  int i;
  int _blocksize, _j, _n;
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

  #pragma omp parallel for private(i,_j,_n)
  for (i=0; i<datasize; i+=_blocksize) {
    _n = i+_blocksize > datasize? datasize:i+_blocksize;
    double sump = 0.0;
    for (_j=i; _j<_n; _j++) {
      sump += sin(data[_j])*sin(data[_j])+cos(data[_j])*cos(data[_j]);
      
    }
    sum += sump;
  }

  printf("%d: %d million numbers added.  The sum is %.0f million (%.0f) \n", getpid(), datasize/1000000, sum/1000000, sum);
}



