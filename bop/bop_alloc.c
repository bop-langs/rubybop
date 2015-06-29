#include <stdlib.h> /* for NULL */
#include <string.h> /* for memcpy */
#include "bop_api.h"
#include "external/malloc.h"
#include <bop_ports.h>

/**
 * Design -
 *
 */

#ifndef mask
#if __WORDSIZE == 64
#define mask 0xfffffffffffffffc
#elif __WORDSIZE == 32
#define mask 0xfffffffc
#else
#define mask 0
#endif
#endif

/* msps - mspaces; msp - mspace */

#define DEFAULT_MSP_SIZE 100000000
#define DEFAULT_GUEST_FREE_LEN 100

mspace * all_msps = NULL; /* array of mspaces */
static int curr_group_size = 0;
static int msp_distance = 0; /* distance between start of mspace and first allocated element */

mspace meta_msp = NULL; /* shared meta mspace */
typedef struct _guest_free_list {
	int alloc;
	int used;
	void **freed_mems;
} guest_free_list;

guest_free_list *guest_free_lists = NULL;

void init_guest_free_lists( int from_which )
{
	int i;
	for ( i=0; i < from_which; i++ ) {
		guest_free_lists[i].used  = 0;
		memset( guest_free_lists[i].freed_mems, 0, guest_free_lists[i].alloc * sizeof(void *)) ;
	}

	for ( i=from_which; i < BOP_get_group_size(); i++ ) {
		guest_free_lists[i].freed_mems = (void **) mspace_malloc( meta_msp, DEFAULT_GUEST_FREE_LEN * sizeof(void *));
		guest_free_lists[i].alloc = DEFAULT_GUEST_FREE_LEN;
		guest_free_lists[i].used  = 0;
		memset( guest_free_lists[i].freed_mems, 0, DEFAULT_GUEST_FREE_LEN * sizeof(void *)) ;
	}
}

void BOP_malloc_init( void )
{
	unsigned num_mspaces = BOP_get_group_size();
	if ( bop_mode == SERIAL ) {
	  /* used in C bop unit tests to run any number of mspaces in the serial mode */
	  num_mspaces = num_mspaces == 1? 2 : num_mspaces;
	  // num_mspaces = 1;
	}

	if ( curr_group_size > num_mspaces ) return;

	/* create all mspaces, one for each task */
	if ( curr_group_size == 0 ) {
		all_msps = malloc( num_mspaces * sizeof(all_msps) );
	}
	else {
		all_msps = realloc( all_msps, num_mspaces * sizeof(all_msps) );
	}

	int i;
	for ( i=curr_group_size; i < num_mspaces; i++ ) {
		all_msps[i] = create_mspace(DEFAULT_MSP_SIZE, 0, 0);
		bop_msg( 3, "created copy & merge mspace at %p", all_msps[i] );
		if ( msp_distance == 0 ) {
		  void* temp = mspace_malloc(all_msps[i], 1);
		  msp_distance = (long int) temp - (long int) all_msps[0];
		}
		else {
		  mspace_malloc(all_msps[i], 1);
		}
	}

	/* create one meta memory space, shared by all tasks */
	if ( meta_msp == NULL )
		meta_msp = create_mspace(0, 1, 1); /* Default size, locked, shared */
	assert( meta_msp != NULL ) ;
	bop_msg( 3, "created metadata mspace at %p", meta_msp );

	/* create guest free lists */
	if ( guest_free_lists == NULL ) {
		guest_free_lists = mspace_calloc(meta_msp, num_mspaces, sizeof(guest_free_list) );
	}
	else {
		guest_free_lists = mspace_realloc(meta_msp, guest_free_lists, num_mspaces * sizeof(guest_free_list) ) ;
	}
	assert(guest_free_lists != NULL);

	init_guest_free_lists( curr_group_size );

	curr_group_size = num_mspaces;
}

void *_BOP_malloc(size_t size, char *filename, unsigned lineno)
{
	if ( meta_msp == NULL ) return malloc(size);
	size_t* ret = NULL;

//    int self = spec_order;
	/* understudy process -1 uses the curr_group_size-1 mspace */
	int self = BOP_ppr_index( ) % curr_group_size;
    assert( self < curr_group_size );

    if ( self >= 0 ) {
        size_t* s = ( size_t* ) all_msps[self];
        size_t free_space = s[3];

        if ( free_space < ( size + ( sizeof ( size_t ) * 3 ) ) )
			BOP_abort_spec("Too much memory is being allocated, must be done sequentially\n");

        ret = mspace_malloc( all_msps[self], size );
	bop_msg( 5, "BOP malloc chunk %p, size %zd, space %d -- %s:%d", ret, size, self, filename, lineno );

        size_t chunk = ret[-1] & mask;
        BOP_promise( &ret[-1], ( (int) chunk ) + 2 * sizeof ( size_t ) );
        BOP_promise( all_msps[self], msp_distance );

    }
	else {
        BOP_abort_spec("### The control flow should not be here in BOP_malloc \n");
    }

    return (void *) ret;
}

void* _BOP_calloc(size_t n_elements, size_t elem_size, char *filename, unsigned lineno)
{
  if ( meta_msp == NULL ) return calloc(n_elements, elem_size);
	void* ret;

    ret = _BOP_malloc( n_elements * elem_size, filename, lineno );
    memset( ret, 0, n_elements * elem_size );
    return ret;
}

void* _BOP_realloc(void* mem, size_t newsize, char *filename, unsigned lineno)
{
  if ( meta_msp == NULL ) return realloc(mem, newsize);
	void* ret = NULL;
//    int self = spec_order;
	/* understudy process -1 uses the curr_group_size-1 mspace */
	int self = BOP_ppr_index( ) % curr_group_size;

    if (self >= 0) {
		assert( self < curr_group_size );

		size_t size = get_malloc_bytes( mem );
		if ( newsize <= size ) return mem;

		ret = _BOP_malloc( newsize + 100, filename, lineno ); // to avoid allocation in the same chunk
		memcpy( ret, mem, size );
		_BOP_free( mem, filename, lineno );

    }
	else {
        BOP_abort_spec("### The control flow should not be here in BOP_realloc \n");
    }

    return ret;
}

void guest_free( int self, void *mem )
{
	guest_free_list *guest_flist = &guest_free_lists[self];
	assert( guest_flist != NULL );

	if ( guest_flist->alloc == guest_flist->used ) {
		guest_flist->alloc *= 2;
		guest_flist->freed_mems = (void **) mspace_realloc(meta_msp, guest_flist->freed_mems, guest_flist->alloc * sizeof(void *));
		assert( guest_flist->freed_mems != NULL );
	}

	bop_msg( 5, "BOP guest_free_lists[%d] adding mem %p at no. %d | alloc[%d], used[%d]", self, mem, guest_flist->used, guest_flist->alloc, guest_flist->used+1);
	guest_flist->freed_mems[guest_flist->used++] = mem;
}

void _BOP_free(void *mem, char *filename, unsigned lineno)
{
  if ( meta_msp == NULL || !task_parallel_p ) return free(mem);
//	int self = spec_order;
	/* understudy process -1 uses the curr_group_size-1 mspace */
//	int self = (spec_order + curr_group_size) % curr_group_size;
        int self = BOP_ppr_index( ) % curr_group_size;

	if ( self >= 0 ) {
		assert( self < curr_group_size );

        if ( get_mspace( mem ) == all_msps[self] ) {
			size_t *free_pointer = (size_t *)mem;
            size_t chunk = free_pointer[-1] & mask;
            BOP_promise(&free_pointer[-1], ((int) chunk) + 2 * sizeof (size_t));
            BOP_promise(all_msps[self], msp_distance);

            mspace_free(all_msps[self], mem);
	    bop_msg( 5, "BOP host free addr %p at mspace %d (%p) -- %s:%d", mem, self, all_msps[self], filename, lineno );
        }
		else {
		  bop_msg( 4, "BOP guest free addr %p at mspace %d (%p) -- %s:%d", mem, self, get_mspace(mem), filename, lineno );
		  guest_free( self, mem);
		}
    }
	else {
        BOP_abort_spec("### The control flow should not be here in BOP_free!");
    }
}

/* Maybe Bug! we don't need check if the calling process is understudy. Should not be understudy! */
void BOP_reset( void )
{
	int self = spec_order;
        if (self < 0) return;
	assert( self < curr_group_size && self >=0 );

	guest_free_lists[self].used  = 0;
	memset( guest_free_lists[self].freed_mems, 0, guest_free_lists[self].alloc * sizeof(void *)) ;
}

void BOP_guest_free_cleanup( void )
{
	int i, j;
	size_t *free_mem_p;
	mspace host_msp;
	for ( i=0; i < BOP_get_group_size(); i++ ) {
		for ( j=0; j < guest_free_lists[i].used; j++ ) {
			free_mem_p = guest_free_lists[i].freed_mems[j];
			host_msp = get_mspace( free_mem_p );

			mspace_free( host_msp, free_mem_p );
			bop_msg( 5, "BOP_guest_free_cleanup free mem %p at mspace %p", free_mem_p, host_msp );
		}
		guest_free_lists[i].used  = 0;
		memset( guest_free_lists[i].freed_mems, 0, guest_free_lists[i].alloc * sizeof(void *)) ;
	}
}

void BOP_malloc_fini( void )
{
	if ( meta_msp == NULL ) return;

	bop_msg( 6, "BOP_alloc_port start free ...");
	int i;
	/* free guest_free_lists */
	for ( i=0; i < curr_group_size; i++ ) {
	  bop_msg( 6, "mspace %p free per space free list %p",  meta_msp, guest_free_lists[i].freed_mems);
		mspace_free( meta_msp, guest_free_lists[i].freed_mems );
	}
	bop_msg( 6, "mspace %p free the list of free lists %p",  meta_msp, guest_free_lists);
	mspace_free( meta_msp, guest_free_lists );
	guest_free_lists = NULL;
	bop_msg( 6, "destroy mspace %p",  meta_msp);
	destroy_mspace( meta_msp );
	meta_msp = NULL;

	/* free all mspaces */
	for ( i=0; i < curr_group_size; i++ ) {
	  bop_msg( 6, "destroy mspace %p",  all_msps[i]);
		destroy_mspace( all_msps[i] );
		all_msps[i] = NULL;
	}
	/* how to free mspaces, already knows it ! */
	bop_msg( 6, "free all_maps %p",  all_msps);
	free( all_msps );
	all_msps = NULL;

	curr_group_size = 0;
}

bop_port_t bop_alloc_port = {
	.ppr_group_init		= BOP_malloc_init,
	.ppr_task_init		= BOP_reset,
	.task_group_commit	= BOP_guest_free_cleanup
};
