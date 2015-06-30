#include <sys/mman.h>  /* for mprotect */
#include <string.h>  /* for memset */

#include "bop_api.h"  /* for mem_range_t */
#include "bop_malloc.h"
#include "external/malloc.h"
#include "bop_map.h"  /* for access map functions */

/* should they be volatile? */
map_t *data_map = NULL;  /* pages for global and heap variables under monitoring */
map_t *priv_vars = NULL;  /* privatizable global variables */
map_t *priv_objs = NULL; /* privatizable objects from BOP_malloc_priv */

/* mspaces */
mspace meta_space = NULL; /* for allocation map and access maps */
mspace serial_space = NULL; /* for allocation in ctrl and understudy tasks */

/* BOP_global_var_priv and BOP_malloc_priv.  The former is reset to 0
   and the latter is freed at the start of each task */
mspace priv_heap = NULL;  

void private_space_init( void ) {
  char is_shared_mem = 0;  /* false */
  char use_lock = 0; 
  size_t init_size = 20000000; /* 20M init size */
  priv_heap = create_mspace( init_size, use_lock, is_shared_mem );
  serial_space = create_mspace( init_size, use_lock, is_shared_mem );
  bop_msg( 3, "created priv_heap and serial_space mspace at %llx and %llx", priv_heap, serial_space );

  meta_space = priv_heap;  /* map_space uses priv_space */
  assert(priv_heap != NULL);
  assert(serial_space != NULL);

  data_map = new_merge_map( );
  priv_vars = new_merge_map( );
  priv_objs = new_merge_map( );

  // map_inspect(priv_heap);
}

/*
 * BOP_malloc shouldn't be necessary here since it's defined in BOP_malloc.c */
/*
#ifdef USE_DL_PREFIX
void *malloc (size_t sz) {
#else
void *BOP_malloc(size_t sz) {
#endif
  /* abort speculation and force sequential execution if the allocator
     is not SEQ or UNDY *//*
  BOP_hard_abort( "BOP malloc during a ppr" );

  memaddr_t obj = (memaddr_t) mspace_calloc(serial_space, 1, sz);
  bop_msg(4, "BOP_malloc %d bytes at 0x%x (pages [%llu, %llu])", sz, obj, PAGESTART(obj)>>PAGESIZEX, PAGEEND(obj+sz-1)>>PAGESIZEX);
  /* record full page ranges *//*
  map_add_range( data_map, PAGESTART(obj), PAGEFILLEDSIZE2(obj, sz), 
		 ppr_index, NULL );
  map_inspect( 4, data_map, "data_map" );

  if (myStatus == MAIN || myStatus == SPEC) 
    BOP_protect_range( (void *) obj, sz, PROT_NONE);

  bop_stats.num_malloc ++;

  return (void *) obj;
}*/

#ifdef USE_DL_PREFIX
 void *calloc (size_t nmemb, size_t size) {
   BOP_HardAbort( );

  memaddr_t obj = (memaddr_t) mspace_calloc(serial_space, nmemb, size);
  bop_msg(2, "BOP_calloc %d bytes at 0x%x (pages [%llu, %llu])", nmemb*size, obj, PAGESTART(obj)>>PAGESIZEX, PAGEEND(obj+nmemb*size-1)>>PAGESIZEX);
  /* record full page ranges */
  map_add_range( data_map, PAGESTART(obj), PAGEFILLEDSIZE2(obj, nmemb*size), 
		 ppr_index, NULL );
  map_inspect( 4, data_map, "data_map" );

  if (myStatus == MAIN || myStatus == SPEC) 
    BOP_protect_range( (void *) obj, nmemb*size, PROT_NONE);

  bop_stats.num_malloc ++;

  return (void *) obj;

 }

 void free(void* mem) {
   return dlfree (mem);
 }
#endif

 void *BOP_valloc( size_t sz ) {
   BOP_hard_abort( "BOP pagefill malloc" );
   memaddr_t obj =  (memaddr_t) valloc( sz );
   bop_stats.num_malloc ++;
   bop_msg(2, "BOP_valloc %d bytes at 0x%llx (pages [%llu, %llu])", sz, obj, PAGESTART(obj)>>PAGESIZEX, PAGEEND(obj+sz-1)>>PAGESIZEX);

  map_add_range( data_map, obj, PAGEFILLEDSIZE(sz), 
		 ppr_index, NULL );
  map_inspect( 4, data_map, "data_map" );

  if (myStatus == MAIN || myStatus == SPEC) 
    BOP_protect_range( (void*) obj, sz, PROT_NONE);

  return (void *) obj;
 }
  

void *BOP_malloc_priv(size_t sz) {
  BOP_hard_abort( "BOP malloc priv" );

  memaddr_t obj = (memaddr_t) mspace_calloc(priv_heap, 1, sz);
  map_add_range( priv_objs, obj, sz, ppr_index, NULL );

  bop_msg(2, "BOP_malloc_priv %d bytes at 0x%x (pages [%llu, %llu])", sz, obj, PAGESTART(obj)>>PAGESIZEX, PAGEEND(obj+sz)>>PAGESIZEX);

  bop_stats.num_malloc_priv ++;

  return (void *) obj;
}

void BOP_global_var(void *var, int sz, char *a, char *b) {
  BOP_hard_abort( "BOP mark global var" );

  memaddr_t addr = (memaddr_t) var;
  /* add the variable to the data map */
  /* data map uses actual addresses but aligned to page boundaries */
  map_add_range(data_map, PAGESTART(addr), PAGEFILLEDSIZE2(addr, sz), 
		ppr_index, NULL );
  /*bop_msg(1, "Global var at 0x%llx (page %lld) of %d bytes, %s %s", addr, addr >> PAGESIZEX, sz, a, b);*/

  bop_stats.num_global_var ++;
}

/* must check at BOP_alloc_init to ensure it has no overlap with the data_map */
void BOP_global_var_priv(void *var, int sz, char *a, char *b) {
  BOP_hard_abort( "BOP mark global priv var" );

  memaddr_t addr = (memaddr_t) var;
  map_add_range( priv_vars, addr, sz, ppr_index, NULL );
  /*bop_msg(1, "Unmonitored global var at 0x%llx (page %lld) of %d bytes, %s %s", addr, addr >> PAGESIZEX, sz, a, b);*/

  bop_stats.num_global_priv_var ++;
}

void priv_heap_free(void) {
  mem_range_t *ranges; unsigned i, num;
  map_to_array( priv_objs, &ranges, &num);
  for (i=0; i<num; i++)
    free((void*) ranges[i].base);
  free(ranges); 
}  

/* called after all global variables are recorded */
void priv_var_check(void) {
  /* ensure private vars and global vars have no overlap.*/
  mem_range_t *ranges; unsigned i, num;
  mem_range_t conflict;
  map_to_array( priv_vars, &ranges, &num);
  for (i=0; i<num; i++) {
    mem_range_t priv;
    priv.base = PAGESTART(ranges[i].base);
    priv.size = PAGEFILLEDSIZE2(ranges[i].base, ranges[i].size);
    assert(! map_overlaps(data_map, &priv, &conflict) );
  }
  free(ranges);
}

/* called at the start of each task */
void priv_var_reset(void) {

  map_inspect(4, priv_vars, "priv_vars");
  /* re-initialize all private vars */
  mem_range_t *ranges; unsigned i, num;
  map_to_array( priv_vars, &ranges, &num);
  for (i=0; i<num; i++)
    memset((void *) ranges[i].base, 0, ranges[i].size);
  free(ranges);
}


