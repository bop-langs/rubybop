#include "bop_api.h"
#include "bop_map.h"
#include "utils.h"

#include "external/malloc.h"
extern mspace priv_heap;
extern map_t *data_map;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>

unsigned long long read_tsc(void) {
  unsigned long long tsc;
  asm ("rdtsc":"=A" (tsc):);
  return tsc;
}

int VERBOSE = 1;

void bop_msg(int level, char * msg, ...) {
  if(VERBOSE >= level)
  {
    va_list v;
    va_start(v,msg);
    fprintf(stderr, "%d-", getpid());
    switch(myStatus) {
    case UNDY: fprintf(stderr, "Undy-(ppr %d): ", ppr_index); break;
    case MAIN: fprintf(stderr, "Main-(ppr %d): ", ppr_index); break;
    case GAP: fprintf(stderr, "Sp-gap-(spec %d)-(ppr %d): ", mySpecOrder, ppr_index); break;
    case SEQ: fprintf(stderr, "Seq: "); break;
    case SPEC: fprintf(stderr, "Spec-%d-(ppr %d): ", mySpecOrder, ppr_index); break;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    double curr_time = tv.tv_sec + (tv.tv_usec/1000000.0);
    fprintf(stderr, " (%.6lfs) ", curr_time - bop_stats.bop_start_time);

    vfprintf(stderr,msg,v);
    fprintf(stderr,"\n");
    fflush(stderr);
  }
}

void bop_error(void) {
  /* reverst to sequential execution */
  myStatus = SEQ;
  bop_msg(1,"Revert to sequential execution");
}

void BOP_protect_range(void *addrp, size_t len, int prot) {
#ifndef BOP_NOPROT
  memaddr_t addr = (memaddr_t) addrp;
  mem_range_t *drange = map_contains( data_map, addr );
  if ( drange == NULL ) return;

  if ( addr != PAGESTART( addr ) || len & 0xfff != 0 ) 
    bop_msg( 2, "BOP_protect_range_set warning: requested range %llx, %u is not complete page(s)", addr, len );

  if ( ! (drange->base <= addr && drange->base+drange->size >= addr+len )) 
    bop_msg( 2, "BOP_protect_range warning: requested range %llx, %u is not nested in the allocated range %llx, %u", addr, len, drange->base, drange->size );

  if(mprotect((void*) PAGESTART(addr), len, prot)){
    bop_msg(1,"BOP_protect_range cannot change permission of pages %llu - %u (0x%x)", PAGESTART(addr)>>PAGESIZEX, PAGEEND(addr+len-1)>>PAGESIZEX, PAGESTART(addr));
    perror(" ");
    memaddr_t i = 0;
    for ( i = PAGESTART(addr); i <= PAGESTART(addr+len-1); i += PAGESIZE) {
      if(mprotect( (void*) i, PAGESIZE, prot)) {
	bop_msg(1,"utils.c:protect_addrs cannot change permission for the page at %llx (%lld) with protection %d", i, i>>PAGESIZEX, prot );
	BOP_abort_spec( "BOP_protect_range failed" );
      } else {
	bop_msg(4, "protected page %d",i >> PAGESIZEX);
      }
    }
  } else {
    if ( len > PAGESIZE )  /* don't output at every page fault */
      bop_msg(2, "protected pages %llu - %llu", addr >> PAGESIZEX, (addr+len-1) >> PAGESIZEX);
  }
#endif
}

// protect all address ranges in data_map
void BOP_protect_range_set(map_t *map, int prot) {
  mem_range_t *ranges; unsigned i, num;
  size_t count = 0;
  unsigned long long startTime = read_tsc();

  map_inspect (2, map, "map of ranges to be protected");
  map_to_array( map, &ranges, &num);
  for (i = 0 ; i < num ; i ++ ) {
    BOP_protect_range( (void *) ranges[i].base, ranges[i].size, prot);
    count += ranges[i].size;
  }
  free(ranges);

  bop_msg(4,"%d pages protected",count >> PAGESIZEX);
  unsigned long long endTime = read_tsc();
  bop_msg(4,"it took %llu time units",endTime - startTime);
}

/* read the environment variable env, and returns it's integer value.
   if the value is undefined, the default value def is returned.
	 the value is restricted to the range [min,max] */
int get_int_from_env(const char* env, int min, int max, int def)
{
  char* cval;
  int   ival = def;

  cval = getenv( env );
  if( cval !=NULL ) ival = atoi( cval );
  if ( ival < min ) ival = min;
	if ( ival > max ) ival = max;
  
  return ival;
}

void* bop_shmmap(size_t length) {
  return mmap(NULL, length ,PROT_READ | PROT_WRITE, 
	      MAP_ANONYMOUS | MAP_SHARED, -1 , 0);
}

void report_conflict( int verbose, 
		      mem_range_t *c1, char *n1,
		      mem_range_t *c2, char *n2 ) {

  bop_msg( verbose, "range %llx (page %llu, size %u) in %s overlaps with range %llx (page %llu, size %u) in %s", c1->base, c1->base >> PAGESIZEX, c1->size, n1, c2->base, c2->base >> PAGESIZEX, c2->size, n2 );

}

char mem_range_eq( mem_range_t *r1, mem_range_t *r2 ) {
  return r1->base == r2->base && r1->size == r2->size;
}

static mspace mspace_new( size_t size ) {
  int use_lock = 0; 
  char is_shared_mem = 1;
  mspace nm = create_mspace( size, use_lock, is_shared_mem );
  return nm;
}

mspace mspace_small_new( void ) {
  mspace nnm = mspace_new( 20000 );
  return nnm;
}

mspace mspace_medium_new( void ) {
  return mspace_new( 2000000 );
}

mspace mspace_large_new( void ) {
  return mspace_new( 200000000 );
}
