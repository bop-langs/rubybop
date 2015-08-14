#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "bop_api.h"

int ITERS = 4;
int *t;

int main(int argc, char ** argv) {

  int i, left, total=0;

  if (argc>1) {
    ITERS = atoi(argv[1]);
    assert(ITERS > 0);
  }

  t = (int *) malloc(ITERS * sizeof(int));

  printf("%d naps:", ITERS);
  for(i=0;i<ITERS;i++) {
    t[i] = rand()%2;
    total += t[i];
    printf(" %d", t[i]);
  }
  printf("\n");

  time_t start, end;
  //srand(getpid());
  start = time(NULL);

  for(i=0;i<ITERS;i++) {
    BOP_ppr_begin( 1 );
    int pid = getpid();
    printf("%d: try to sleep for %d sec\n", pid, t[i]);
    left = sleep(t[i]);
    while (left!=0) {
      printf("%d: try finishing incomplete sleep (%d sec left)\n", pid, left);
      left = sleep(left);
    }
    BOP_ppr_end( 1 );
  }

  end = time(NULL);
  printf("process time: %d seconds\n", total);
  printf("clock time: %d seconds\n", (int) end-start);
  return 0;
}
