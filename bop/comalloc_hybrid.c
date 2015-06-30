

#include <stdlib.h>
#include "malloc.c"

#include <stdio.h>


#define VERBOSE 0

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT) -1) & ~(ALIGNMENT- 1))

//Pointer Size
#define PSIZE 8

#define CHUNKSIZE (1<<17)

#define WSIZE 4
#define DSIZE 8

static const MAX_FREE = CHUNKSIZE-16-PSIZE;
//#define BIT_IDENTIFIER k uses kth lowest bit
#define BIT_IDENTIFIER 3
#define HEADER_IDENTIFIER_MASK (1<<(BIT_IDENTIFIER-1))

#define IS_ALLOCED_BY_DM(PAYLOAD_P) ((*(int*)(PAYLOAD_P-4)) & HEADER_IDENTIFIER_MASK)

//Read-write operations
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define PUTL(p, val) (*(unsigned long int *) (p) = (val))

//General operations
#define LPACK(size, alloc) ((size | alloc | HEADER_IDENTIFIER_MASK))
#define MAX(x, y) ((x)> (y) ? (x) : (y))
//Block size operations

#define GET_SIZE(p) (GET(p) & ~(ALIGNMENT-1))
#define GET_ALLOC(p) (GET(p) & 0x1)

//Header-footer operations
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//Adjacent block macros
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define malloc(size) lmalloc(size)
#define realloc(ptr, size) lrealloc(ptr, size)
#define free(ptr) lfree(ptr)

//Heap to be managed on top of dl
void *lazy_heap;

int init = 0;
int blockcheck = 0;
int mdriverflag = 0;
int lmalloc_init();

int allocs = 0;
void *lmalloc(size_t size);
void lfree(void *bp);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void blocksize_check();
static void blocksize_check2(void *chunk);

void init_chunk(void*);
void dlfree(void *bp);
void *dlmalloc(size_t size);
void *dlrealloc(void *bp, size_t size);
void *dlcalloc(size_t, size_t);


void *calloc(size_t elems, size_t num)
{
    return dlcalloc(elems, num);
}

void *realloc_in_place(void * bp, size_t size)
{
    return dlrealloc_in_place(bp, size);
}

void *memalign(size_t size, size_t ptr)
{
    return dlmemalign(size, ptr);
}

int posix_memalign(void ** bp, size_t a, size_t b)
{
    return dlposix_memalign(bp, a, b);
}

void *valloc(size_t n)
{
    return dlvalloc(n);
}

void *mallopt(int a, int b)
{
    return dlmallopt(a, b);
}

void *pvalloc(size_t a)
{
    return dlpvalloc(a);
}
void bulk_free(void ** ptr, size_t elems)
{
    return dlbulk_free(ptr, elems);
}
#define LISTCOUNT 32
void *lastp;
//Fixed set of pointers to heap sectors, ordered by powers of two.
void *seglist[LISTCOUNT];

static inline int get_index(int size)
{
    int x;
    asm( "\tbsr %1, %0\n"
	 : "=r"(x)
	 : "r" (x)
	);
    return x;
}

int get_index2(int size)
{
    int index = 0;
    //Space is too big. Return maximum possible index. 
    if (size >= (1<<(LISTCOUNT-1)))
	return LISTCOUNT-1;
    //Divide by two until we have zero to get index.
    while ((size>>=1)>0)
	index++;
    return index;
}

void report(char *msg)
{
	if(VERBOSE)
	{
		printf("%s\n", msg);
	}
	return;
}

int lmalloc_init()
{
    if (mdriverflag)
	return 0;
    int i;
	mdriverflag = 1;
    if (VERBOSE)
    	printf("Empty ptr: %x\n", lazy_heap);
    lazy_heap = dlmalloc(CHUNKSIZE);
    printf("Chunk alloced by dm: %d\n", IS_ALLOCED_BY_DM(lazy_heap));
    if (!VERBOSE)
    printf("Dl malloced heap: %p\n", lazy_heap);
    //Set all ordered nextfits to null
    for (i = 0 ; i<LISTCOUNT; i++)
    {
	seglist[i] = NULL;
    }
    init_chunk(lazy_heap);
    lazy_heap+=4*WSIZE;
    if (VERBOSE)
    {
	report("Memory initialized\n");
    }
    if (VERBOSE)
    	blocksize_check();
    return 0;
}

void init_chunk(void *chunk_ptr)
{
    //1 word padding for 16 byte alignment
    //dlmalloc guarantees 16byte alignment, so three pads plus the header will result in a 16B-aligned payload
    PUT(chunk_ptr, 0);
    //Prologue header/footer
    PUT(chunk_ptr + (WSIZE), LPACK(DSIZE, 1));
    PUT(chunk_ptr + (2*WSIZE), LPACK(DSIZE, 1));
    PUT(chunk_ptr + (3*WSIZE), LPACK(CHUNKSIZE-16-PSIZE, 0));
    //Epilogue
    PUT(chunk_ptr + CHUNKSIZE-PSIZE-WSIZE, LPACK(0, 1));
    PUTL(chunk_ptr + CHUNKSIZE-PSIZE, 0);
    place(chunk_ptr+4*WSIZE, 16);
}


void *lmalloc(const size_t size)
{
    //if (allocs % 10000 == 0)
    //printf("Mallocs: %d\n", ++allocs);
    if (!init)
	lmalloc_init();
    if (VERBOSE)
	printf("Malloc request for %d bytes\n", size);
    size_t asize;
    size_t extendsize;
    char *bp;
    //Size is 0. No memory required.
    if (size == 0)
	  return NULL;
    //Size below minimum requirement. Adjust to minimum. 
    if (size<= DSIZE)
	asize = 2*DSIZE;
    else
	asize = ALIGN(size)+16;
    if (asize > MAX_FREE)
    {
	//printf("Request (%d bytes) too big for allocation\n", size);
	//printf("PROBLEMS\n\n\n");
	//getchar();
	return dlmalloc(size);
    }
    //Find a free block if possible
    if ((bp = find_fit(asize)) != NULL)
    {	//Free block found. Using already requested space
		place(bp, asize);
		if (VERBOSE)
		{
			printf("Returned %p (fit)\n", bp);
			printf("Hdr: %p\n", bp-WSIZE);
			printf("Ftr: %p\n", bp+GET_SIZE(HDRP(bp)) - DSIZE);
		}
		if (blockcheck) blocksize_check();
		//printf("Returned %p |(alloced_by_dm: %d)\n", bp, IS_ALLOCED_BY_DM(bp));
		if (lastp == bp)
		{
		    //printf("returned pointer twice\n");
		    // getchar();
		}
		lastp = bp;
        return bp;
    }
    return NULL;
}

void append_chunk(void *newchunk_ptr)
{
    //printf("append:\n");
    //blocksize_check();
        void *ptr;
	int count = 0;
	ptr = lazy_heap-(4*WSIZE);
	for (; (*((unsigned long int*)(ptr+(CHUNKSIZE-PSIZE)))); ptr = *((unsigned long int*)(ptr+CHUNKSIZE-PSIZE)))
	{
	    //printf("chunk jump: %p\n", ptr);
	    count++;
	}
	*((unsigned long int*)(ptr+CHUNKSIZE-PSIZE)) = newchunk_ptr;
	if (VERBOSE)
	{
	    printf("\nAppended chunk: %p\n", (*(unsigned long int*)(ptr+CHUNKSIZE-PSIZE)));
	    printf("\nCorrect chunk: %p\n", newchunk_ptr);
	    printf("Appended at: %p\n", ptr+CHUNKSIZE-PSIZE);
	}
	//printf("Total jumps: %d\n", count);
	//printf("After append: \n");
	//blocksize_check();
	//getchar();
}
static void *find_fit(size_t asize)
{
    void *bp;
    int index;
    int end_of_chunk = 0;
    int jumped = 0;
    //Search from last position if possible. Otherwise go from heap start.
search:
    if (!jumped)
    bp = (seglist[(index = get_index(asize))] ? seglist[index] : lazy_heap);
		while (GET_SIZE(HDRP(bp)) > 0)
		{
		    //Current block is free and the size fits there
		  if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
		  {
			  //Look from here next time we request a space in this range
			  seglist[index] = bp;
			  if(VERBOSE)
			  	printf("Returned by fit: %x\n", bp);
			  return bp;
		  }
		  bp = NEXT_BLKP(bp);
		}
		bp+=2*WSIZE;
		if (!(*(long int*) bp))
		{
		    void *newchunk = dlmalloc(CHUNKSIZE);
		    if (!newchunk)
		    {
			printf("Could not allocate more memory\n");
			return NULL;
		    }
		    init_chunk(newchunk);
		    append_chunk(newchunk);
		    //printf("Append for %d\n", asize);
		}
		if ((*(long int*) bp))
		{
			bp =*(unsigned long int*)bp+4*WSIZE;
		       	//getchar();
			//blocksize_check2(bp);
			//printf("bp jumped chunks to %p\n", bp);
			jumped = 1;
			goto search;
		}
    return NULL;
}

static void blocksize_check()
{
    void *bp = lazy_heap;
    printf("blocksize_check()\n");
    int chunkend = 0;
    while (!chunkend)
    {	
	for(; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
	    printf("Size of current block (%p:%p||%s): %dB -- HDR:%p|FTR:%p|COMD: %d\n", bp, bp+GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) ? "allocated" : "free", GET_SIZE(HDRP(bp)), HDRP(bp), FTRP(bp), IS_ALLOCED_BY_DM(bp));
	}
	bp+=2*WSIZE;
	if ((*(long int*) bp))
	{
	    bp = *(long int*)bp+4*WSIZE;
	    printf("ChunkJump: %p\n", bp);
	}
	else
	{
	    printf("No more chunks\n");
	    return;
	}
	printf("------\n");
    }
    return;
}

static void blocksize_check2(void *chunk)
{
   	void *bp;
	printf("called\n");
	for(bp = (chunk); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	    printf("Size of current payload (%p:%p||%s): %dB -- HDR:%p|FTR:%p\n", bp, bp+GET_SIZE(HDRP(bp))-2*WSIZE, GET_ALLOC(HDRP(bp)) ? "allocated" : "free", GET_SIZE(HDRP(bp)), HDRP(bp), FTRP(bp));
	printf("------\n");
}
static void place(void *bp, size_t asize)
{
     size_t csize = GET_SIZE(HDRP(bp));
     //More space than requested enough for a new free block
     if ((csize - asize) >= (2*DSIZE))
     {
	  PUT(HDRP(bp), LPACK(asize, 1));
	  PUT(FTRP(bp), LPACK(asize, 1));
	  bp = NEXT_BLKP(bp);
	  PUT(HDRP(bp), LPACK((csize-asize), 0));
	  PUT(FTRP(bp), LPACK((csize-asize), 0));
     }
     //Not enough remainder for a free block. Use up entire block.
     else
     {
	  PUT(HDRP(bp), LPACK(csize, 1));
	  PUT(FTRP(bp), LPACK(csize, 1));
     }
}

//Realloc to a size too big may require handling.
void *lrealloc(void *bp, size_t size)
{
     void *oldptr = bp;
     void *new;
     size_t copySize;
     size_t oldBlockSize = GET_SIZE(HDRP(bp));
     size_t nextBlockSize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
     size_t total;
     size_t asize;
     if (size == 0)
     {
	  free(bp);
	  return NULL;
     }
     if (!bp)
     {
	  return NULL;
     }
     if (size == oldBlockSize)
	  return bp;
     if (size <= DSIZE)
	  asize = 2*DSIZE;
     else
	  asize = ALIGN(size)+16;
     if (oldBlockSize>=asize)
     {
	  place(bp, asize);
	  return bp;
     }
     if ((!next_alloc) && ((total = nextBlockSize+oldBlockSize) >= asize) && (size>=oldBlockSize))
     {
	  PUT(HDRP(bp), LPACK(total, 1));
	  PUT(FTRP(bp), LPACK(total, 1));
	  place(bp, asize);
	  return bp;
     }
     new = malloc(size);
     if (!new)
	  return NULL;
     copySize = GET_SIZE(HDRP(bp)) - 8;
     if (size < copySize)
	  copySize = size;
     memcpy(new, oldptr, copySize);
     free(oldptr);
     return new;
}


//We need to check if the pointer was allocated by us or not. 
void lfree(void *bp)
{
    return;
    if (!VERBOSE)
	printf("Free request at %p\n", bp);
    if (!IS_ALLOCED_BY_DM(bp))
    {
	printf("Freeing dl pointer\n");
	getchar();
	dlfree(bp);
	return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), LPACK(size, 0));
    PUT(FTRP(bp), LPACK(size, 0));
    //seglist[get_index(size)] = coalesce(bp);
    coalesce(bp);
    if (VERBOSE)
        blocksize_check();
}
static void *coalesce(void *bp)
{
     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
     size_t size = GET_SIZE(HDRP(bp));
     size_t oldsize = size;
     
     //Both adjacent blocks allocated. No coalescing possible.
      if (prev_alloc && next_alloc)
	  return bp;
     //Only previous block is allocated. Merge with next block.
     else if (prev_alloc && !next_alloc)
     {
	  size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
	  PUT(HDRP(bp), LPACK(size, 0));
	  PUT(FTRP(bp), LPACK(size, 0));
     }
     //Only next block is allocated. Merge with previous block.
     else if (!prev_alloc && next_alloc)
     {
	  size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
	  PUT(FTRP(bp), LPACK(size, 0));
	  PUT(HDRP(PREV_BLKP(bp)), LPACK(size, 0));
	  bp = PREV_BLKP(bp);
     }
     //Both prev and next blocks are free. Extend the prev block over to the next block.
     else
     {
	  size+=GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
	  PUT(HDRP(PREV_BLKP(bp)), LPACK(size, 0));
	  PUT(FTRP(NEXT_BLKP(bp)), LPACK(size, 0));
	  bp = PREV_BLKP(bp);
     }
     return bp;
}
