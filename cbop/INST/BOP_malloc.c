#include <stdio.h>
#include "bop_api.h"
#include "external/malloc.h"
#include <limits.h>
#include <string.h>

#ifndef mask
#if __WORDSIZE == 64
#define mask 0xfffffffffffffffc
#elif __WORDSIZE == 32
#define mask 0xfffffffc
#else
#define mask 0
#endif
#endif

/*

10/14/2011, Chen Ding
Simplifying the design, fixing a minor error, reusing malloc/free to
implement calloc and realloc, removing unneeded interfaces.

Zachary Fletcher
6/9/2011
BOP_malloc.c allows for use of malloc in parallel
Each mySpecOrder (if BOP_GroupSize = 2, there are 2, if BOP_GroupSize = 4, there are 4, etc.) has its own mspace.

BOP_malloc_init() must be called before BOP_malloc is used in the program
BOP_malloc(size_t size) should be used like malloc
BOP_calloc(size_t n_elements, size_t elem_size) should be used like malloc
BOP_free(void* mem) should be used like free
BOP_reset() should be called when the PPR group finishes

6/10/2011: currently BOP_malloc(size_t size), BOP_malloc_init(),
BOP_calloc(size_t n_elements, size_t elem_size) and BOP_free() work
  
 
8/1/2011: Added BOP_ppr_malloc and BOP_ppr_calloc

 */

#define BOP_use( addr, size )  BOP_record_read( addr, size )
#define BOP_promise( addr, size )  BOP_record_write( addr, size )

extern int mySpecOrder; //starts at 0-n where n = GroupSize - 1

static int default_mspace_size = 24000000;
mspace* BOP_malloc_m; //array of mspaces
int BOP_malloc_m_sz = 0;
//int* BOP_malloc_msize; //array of size of each mspace
int num_frees = 100;
static void*** BOP_malloc_frees; //array of an array of pointers to be freed; one array per mspace
static int free_place = 0; //index of that array
static int mspace_distance = 0; //distance between start of mspace and first allocated element

static mspace shared_mspace = NULL;

void bop_malloc_init( int num_spaces ) {
    int i;

    /* Do nothing if there are enough initialized already.  Otherwise add. */
    if ( BOP_malloc_m_sz > num_spaces )
      return;

    int base_free;
    if ( BOP_malloc_m_sz == 0 ) {
	BOP_malloc_m = malloc(num_spaces * sizeof(BOP_malloc_m) );
	BOP_malloc_frees = malloc(num_spaces * sizeof( void *) );
	base_free = 0;
    }
    else {
      BOP_malloc_m = realloc( BOP_malloc_m, num_spaces * sizeof(BOP_malloc_m) );
      BOP_malloc_frees = realloc( BOP_malloc_frees, num_spaces * sizeof( void *) );
      base_free = BOP_malloc_m_sz;
    }

    for (i = base_free; i < num_spaces; i++) {
	  int len = (num_frees + 1) * sizeof(void*);
	  BOP_malloc_frees[i] = malloc( len );
	  memset( BOP_malloc_frees[i], 0, len );
    }
	  
    for (i = base_free; i < num_spaces; i++) {
        BOP_malloc_m[i] = create_mspace(default_mspace_size, 0, 0);
    }

    if (mspace_distance == 0) {
	void* temp = mspace_malloc(BOP_malloc_m[0], 0);
	mspace_distance = (long int) temp - (long int) BOP_malloc_m[0];
    }

    BOP_malloc_m_sz = num_spaces;
}

void* BOP_malloc(size_t size) {
    size_t* ret;

    int ppr_id = mySpecOrder;
    assert( ppr_id < BOP_malloc_m_sz );

    if (ppr_id >= 0) {
        size_t* s = (size_t*) BOP_malloc_m[ppr_id];
        size_t free_space = s[3];

        if (free_space < (size + (sizeof (size_t) * 3))) 
	  BOP_abort_spec("Too much memory is being allocated, must be done sequentially\n");

	ret = mspace_malloc(BOP_malloc_m[ppr_id], size);

	size_t chunk = ret[-1] & mask;

	BOP_promise(&ret[-1], ((int) chunk) + 2 * sizeof (size_t));
	BOP_promise(BOP_malloc_m[ppr_id], mspace_distance);
    } else if (ppr_id == -1) {
        ret = dlmalloc(size);
    } else {
        BOP_abort_spec("Something strange in BOP_malloc.c 1\n");
    }

    return (void *) ret;
}

void* BOP_calloc(size_t n_elements, size_t elem_size) {
    void* ret;

    ret = BOP_malloc( n_elements * elem_size );
    memset( ret, 0, n_elements * elem_size );
    return ret;
}

void BOP_free(void* mem) {
    if (mySpecOrder >= 0) {
      assert( mySpecOrder < BOP_malloc_m_sz );

        if (get_mspace(mem) == BOP_malloc_m[mySpecOrder]) {
            size_t* free_pointer = (size_t*) mem;
            size_t chunk = free_pointer[-1] & mask;
            BOP_promise(&free_pointer[-1], ((int) chunk) + 2 * sizeof (size_t));
            BOP_promise(BOP_malloc_m[mySpecOrder], mspace_distance);
	    // Not needed because this ppr "owns" the mspace
            // BOP_use(&free_pointer[-1], ((int) chunk) + 2 * sizeof (size_t));
            dlfree(mem);
        } else {
            if (free_place < num_frees) {
                BOP_malloc_frees[mySpecOrder][free_place] = mem;
                free_place++;
            } else {
                //right now if there are more than 100 frees in a ppr, that ppr fails
                BOP_abort_spec("Too many frees in the PPR");
            }
        }
    } else if (mySpecOrder == -1) {
        dlfree(mem);
    } else {
        BOP_abort_spec("Something strange in BOP_malloc.c 2\n");
    }
}

/* realloc is tricky.  First we can't use realloc since a ppr may have to allocate in a mspace it doesn't own.  The solution is to allocate a new one and free the old one.  DLmalloc design, however, may use a part of a chunk. Its malloc-copy-free steps at the end of DL's internal_realloc would be flawed in our context because the DL's design assumption that the original chunk can't be extended.  I am over-allocating to satisfy that assumption.  A future improvement is to check whether this is the same mspace so we can use realloc. */  
void* BOP_realloc(void* mem, size_t newsize) {
    void* ret;
    int ppr_id = mySpecOrder;

    if (ppr_id >= 0) {
      assert( ppr_id < BOP_malloc_m_sz );

      size_t size = get_malloc_bytes(mem);
      if ( newsize <= size ) return mem;

      ret = BOP_malloc( newsize + 100 ); // to avoid allocation in the same chunk
      memcpy( ret, mem, size );
      BOP_free( mem );
    } else if (ppr_id == -1) {
        ret = dlrealloc(mem, newsize);
    } else {
        assert(0);
    }
    return ret;
}

void BOP_reset( void ) {
    int i;
    for (i = 0; i < curr_group_cap; i++) {
        int j;
        for (j = 0; j < num_frees; j++) {
            if (BOP_malloc_frees[i][j] == NULL) { //if array is full, end loop
                break;
            }
            dlfree(BOP_malloc_frees[i][j]);
            bop_msg(5, "Freed %x\n", BOP_malloc_frees[i][j]);
            BOP_malloc_frees[i][j] = NULL;
        }
    }
    free_place = 0;
}

void* shared_malloc(size_t size) {
    if (shared_mspace == NULL) {
        shared_mspace = create_mspace(default_mspace_size, 0, 1);
    }
    //printf("shared malloc\n");
    return mspace_malloc(shared_mspace, size);
}

void* shared_calloc(size_t n_elements, size_t elem_size) {
    if (shared_mspace == NULL) {
        shared_mspace = create_mspace(default_mspace_size, 0, 1);
    }
    //printf("shared calloc\n");
    return mspace_calloc(shared_mspace, n_elements, elem_size);
}
