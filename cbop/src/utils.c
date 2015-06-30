#include <assert.h>
#include <stdarg.h>  /* for bop_msg */
#include <stdlib.h>  /* getenv */
#include <sys/time.h>
#include <unistd.h>

#include "bop_api.h"

extern task_status_t task_status;
extern ppr_pos_t ppr_pos;
extern int ppr_index;
extern int spec_order;
extern bop_mode_t bop_mode;

task_status_t BOP_task_status(void) {
  return task_status;
}

ppr_pos_t BOP_ppr_pos( void ) {
  return ppr_pos;
}

int BOP_ppr_index( void ) {
  if ( ppr_pos == GAP ) return ppr_index + 1;
  else return ppr_index;
}

int BOP_spec_order( void ) {
  return spec_order;
}

bop_mode_t BOP_mode( void ) {
  return bop_mode;
}

unsigned long long read_tsc(void) {
  unsigned long long tsc;
  asm ("rdtsc":"=A" (tsc):);
  return tsc;
}

int bop_verbose = 0;

void BOP_set_verbose(int x) {
  assert( x < 6 && x > 0 );
  bop_verbose = x;
}

int BOP_get_verbose( void ) {
  return bop_verbose;
}

extern char in_ordered_region;  // bop_ordered.c

void bop_msg(int level, char * msg, ...) {
  if(bop_verbose >= level)
  {
    va_list v;
    va_start(v,msg);
    fprintf(stderr, "%d-", getpid());

    char *pos;
    switch (ppr_pos) {
    case PPR:
      pos = "";
      break;
    case GAP:
      pos = "g";
      break;
    default:
      assert(0);
    }
    if (in_ordered_region) {
      assert( ppr_pos == PPR );
      pos = "od";
    }

    unsigned pidx = BOP_ppr_index( );
    switch(task_status) {
    case UNDY: fprintf(stderr, "Undy-(idx %d%s): ", pidx, pos); break;
    case MAIN: fprintf(stderr, "Main-%d(idx %d%s): ", spec_order, pidx, pos); break;
    case SEQ: fprintf(stderr, "Seq-(idx %d%s): ", pidx, pos); break;
    case SPEC: fprintf(stderr, "Spec-%d(idx %d%s): ", spec_order, pidx, pos); break;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    double curr_time = tv.tv_sec + (tv.tv_usec/1000000.0);
    if (bop_stats.start_time != 0)
      fprintf(stderr, " (%.6lfs) ", curr_time - bop_stats.start_time);

    vfprintf(stderr,msg,v);
    fprintf(stderr,"\n");
    fflush(stderr);
  }
}

/* read the environment variable env, and returns it's integer value.
   if the value is undefined, the default value def is returned.
	 the value is restricted to the range [min,max] */
int get_int_from_env(const char* env, int min, int max, int def)
{
  char* cval;

  assert( min < max );

  cval = getenv( env );
  if (cval == NULL) {
    bop_msg( 2, "Variable %s is set to default (%d).", env, def);
    return def;
  }

  int ival = atoi( cval );
  if ( ival < min ) ival = min;
  if ( ival > max ) ival = max;
  bop_msg( 2, "Variable %s is set as %d based on env ([%d, %d]).", env, ival, min, max);
  
  return ival;
}
