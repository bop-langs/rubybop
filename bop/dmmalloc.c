//#define NDEBUG			//defined: no debug variables or asserts.
#define CHECK_COUNTS      //defined: enable assert messages related to correct counts for each size class
//#define VISUALIZE

#ifndef NDEBUG
#include <locale.h> //commas numbers (debug information)
#endif
#include <stdio.h> //print errors
#include <pthread.h> //mutex
#include <stdlib.h> //system malloc & free
#include <string.h> //memcopy
#include <assert.h> //debug
#include <stdbool.h> //boolean types
#include <unistd.h> //get page size
#include "dmmalloc.h"
#include "malloc_wrapper.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#define LOG(x) llog2(x)
#else
#include <math.h>
#define LOG(x) log2(x)
#endif

//Alignment based on word size
#if __WORDSIZE == 64
#define ALIGNMENT 8
#elif __WORDSIZE == 32
#define ALIGNMENT 4
#else
#error "need 32 or 64 bit word size"
#endif


//debug macros
#ifdef CHECK_COUNTS
#define ASSERT_VALID_COUNT(which) \
	if(which != -1 && SEQUENTIAL){\
		if(!((headers[(which)] == NULL && counts[(which)] == 0)  || \
			 (headers[(which)] != NULL && counts[(which)] > 0))){\
			 printf("\n%s:%d: invalid counts on which= %d counts[which]=%d headers[which]=%p\n",\
			 	__FILE__, __LINE__, which, counts[which], headers[which]);\
			 abort();\
		}\
	}
#define CHECK_EMPTY(which)\
	if(which != -1 && SEQUENTIAL){\
		if(!(headers[(which)] == NULL && counts[(which)] == 0)){\
			 printf("\n%s:%d: non-empty which= %d counts[which]=%d headers[which]=%p\n",\
			 	__FILE__, __LINE__, which, counts[which], headers[which]);\
			 abort();\
		}\
	}
#else
#define ASSERT_VALID_COUNT(which)
#define CHECK_EMPTY(which)
#endif

#ifdef NDEBUG
#define PRINT(msg) printf("dmmalloc: %s at %s:%d\n", msg, __FILE__, __LINE__)
#else
#define PRINT(msg)
#endif


//alignement/ header macros
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HSIZE (ALIGN((sizeof(header))))
#define HEADER(vp) ((header *) (((char *) (vp)) - HSIZE))
#define CAST_SH(h) ((union header *) (h))
#define CAST_H(h) ((header*) (h))
#define CHARP(p) (((char*) (p)))
#define PAYLOAD(hp) ((header *) (((char *) (hp)) + HSIZE))
#define PTR_MATH(ptr, d) ((CHARP(ptr)) + d)
#define ASSERTBLK(head) assert ((head)->allocated.blocksize > 0);

//class size macros
#define NUM_CLASSES 12
#define CLASS_OFFSET 4 //how much extra to shift the bits for size class, ie class k is 2 ^ (k + CLASS_OFFSET)
#define MAX_SIZE sizes[NUM_CLASSES - 1]
#define SIZE_C(k) ALIGN((1 << (k + CLASS_OFFSET)))	//allows for iterative spliting
#define MIN(a, b) ((a) < (b) ? (a) : (b))

//grow macros
#define BLKS_1 50
#define BLKS_2 50
#define BLKS_3 50
#define BLKS_4 50
#define BLKS_5 50
#define BLKS_6 50
#define BLKS_7 50
#define BLKS_8 50
#define BLKS_9 50
#define BLKS_10 50
#define BLKS_11 50
#define BLKS_12 50
#define GROW_S ((BLKS_1 * SIZE_C(1)) + (BLKS_2 * SIZE_C(2)) + \
				(BLKS_3 * SIZE_C(3)) + (BLKS_4 * SIZE_C(4)) + \
				(BLKS_5 * SIZE_C(5)) + (BLKS_6 * SIZE_C(6)) + \
				(BLKS_7 * SIZE_C(7)) + (BLKS_8 * SIZE_C(8)) + \
				(BLKS_9 * SIZE_C(9)) + (BLKS_10 * SIZE_C(10)) +\
				(BLKS_11 * SIZE_C(11)) + (BLKS_12 * SIZE_C(12)) )

//BOP macros & structures
#define SEQUENTIAL 1		//just for testing, will be replaced with actual macro
#define BOP_promise(x, s)   //again just for testing. Will be replaced with the actual function once the library is added in

typedef struct {
    header *start[NUM_CLASSES];
    header *end[NUM_CLASSES];
} ppr_list;

ppr_list *regions = NULL;

//header info
header *headers[NUM_CLASSES];	//current heads of free lists


const unsigned int sizes[NUM_CLASSES] = { SIZE_C (1), SIZE_C (2), SIZE_C (3), SIZE_C (4),
                                          SIZE_C (5), SIZE_C (6), SIZE_C (7), SIZE_C (8),
                                          SIZE_C (9), SIZE_C (10), SIZE_C (11), SIZE_C (12)
                                        };
const int goal_counts[NUM_CLASSES] = { BLKS_1, BLKS_2, BLKS_3, BLKS_4, BLKS_5, BLKS_6,
                                       BLKS_7, BLKS_8, BLKS_9, BLKS_10, BLKS_11, BLKS_12
                                     };

static int counts[NUM_CLASSES] = {0,0,0,0,0,0,0,0,0,0,0,0};
static int promise_counts[NUM_CLASSES]; //the merging PPR tasks promises these values, which are the then handled by the surving PPR task. The surviving task copies the counts OR (on abort) adds the total counts that the PPR task got, which is constant for all tasks
static pthread_mutex_t lock;

header* allocatedList= NULL; //list of items allocated during PPR-mode
header* freedlist= NULL; //list of items freed during PPR-mode

header* ends[NUM_CLASSES]; //end of lists in PPR region

//helper prototypes
static inline int get_index (size_t);
static inline void grow (int);
static inline void free_now (header *);
static inline bool list_contains (header * list, header * item);
static inline header* remove_from_alloc_list (header *);
static inline void add_next_list (header**, header *);
static inline header *dm_split (int which);
static inline int index_bigger (int);
static inline size_t align(size_t size, size_t align);
static inline void get_lock();
static inline void release_lock();

#ifndef NDEBUG
static int grow_count = 0;
static int growth_size = 0;
static int missed_splits = 0;
static int splits = 0;
static int multi_splits = 0;
static int split_attempts[NUM_CLASSES];
static int split_gave_head[NUM_CLASSES];
#endif

/** x86 assembly code for computing the log2 of a value.
		This is much faster than math.h log2*/
static inline int llog2(const int x) {
    int y;
    __asm__ ( "\tbsr %1, %0\n"
              : "=r"(y)
              : "r" (x)
            );
    return y;
}
void dm_check(void* payload) {
    assert (payload != NULL);
    header* head = HEADER (payload);
    ASSERTBLK (head);
}
static inline size_t align(size_t size, size_t alignment) {
    int log = LOG(alignment);
    assert(alignment == (1 << log));
    return (((size) + (alignment-1)) & ~(alignment-1));
}
/**Get the index corresponding to the given size. If size > MAX_SIZE, then return -1*/
static inline int get_index (size_t size) {
    assert (size == ALIGN (size));
    assert (size >= HSIZE);
    //Space is too big.
    if (size > MAX_SIZE)
        return -1;			//too big
    //Computations for the actual index, off set of -5 from macro & array indexing
    int index = LOG(size) - CLASS_OFFSET - 1;

    if (index == -1 || sizes[index] < size)
        index++;
    assert (index >= 0 && index < NUM_CLASSES);
    assert (sizes[index] >= size); //this size class is large enough
    assert (index == 0 || sizes[index - 1] < size); //using the minimal valid size class
    return index;
}
/**Locking functions*/
static inline void get_lock() {
    pthread_mutex_lock(&lock);
}
static inline void release_lock() {
    pthread_mutex_unlock(&lock);
}
/** Bop-related functionss*/

/** Divide up the currently allocated groups into regions.
	Insures that each task will have a percentage of a sequential goal*/
void carve (int tasks) {
    assert (tasks >= 2);
    grow(tasks / 1.5);
    if (regions != NULL) //remove old regions information
        dm_free (regions);		//don't need old bounds anymore
    regions = dm_malloc (tasks * sizeof (ppr_list));
    int index, count, j, r;
    header *current_headers[NUM_CLASSES];
    header *temp = NULL;
    for (index = 0; index < NUM_CLASSES; index++)
        current_headers[index] = CAST_H (headers[index]);
    //actually split the lists
    for (index = 0; index < NUM_CLASSES; index++) {
        count = counts[index] /= tasks;
        for (r = 0; r < tasks; r++) {
            regions[r].start[index] = current_headers[index];
            for (j = 0; j < count && temp; j++) {
                temp = CAST_H (current_headers[index]->free.next);
            }
            current_headers[index] = temp;
            if (r != tasks - 1) {
                //the last task has no tail, use the same as seq. exectution
                assert (temp != NULL);
                regions[r].end[index] = CAST_H (temp->free.prev);
            }
        }
    }
}

/**set the range of values to be used by this PPR task*/
void initialize_group (int group_num) {
    ppr_list my_list = regions[group_num];
    int ind;
    for (ind = 0; ind < NUM_CLASSES; ind++) {
        ends[ind] = my_list.end[ind];
        headers[ind] = my_list.start[ind];
    }
}

/** Merge
1) Promise everything in both allocated and free list
2) Integrate counts based off of abort
*/
void malloc_promise() {
    header* head;
    for(head = allocatedList; head != NULL; head = CAST_H(head->allocated.next))
        BOP_promise(head, head->allocated.blocksize); //playload matters
    for(head = freedlist; head != NULL; head = CAST_H(head->allocated.next))
        BOP_promise(head, HSIZE); //payload doesn't matter
    memcpy(counts, promise_counts, sizeof(counts));
    BOP_promise(promise_counts, sizeof(counts));
}
void malloc_merge_counts(bool aborted) {
    int index;
    const int * array = aborted ? goal_counts : promise_counts;
    for(index = 0; index < NUM_CLASSES; index++)
        counts[index] += array[index];
}

/** Standard malloc library functions */
//Grow the managed space so that each size class as tasks * their goal block counts
static inline void grow (const int tasks) {
    int class_index, blocks_left, size;
    PRINT("growing");
#ifndef NDEBUG
    grow_count++;
#endif
    //compute the number of blocks to allocate
    size_t growth = HSIZE;
    int blocks[NUM_CLASSES];

    for(class_index = 0; class_index < NUM_CLASSES; class_index++) {
        blocks_left = tasks * goal_counts[class_index] - counts[class_index];
        blocks[class_index] =  blocks_left >= 0 ? blocks_left : 0;
        growth += blocks[class_index] * sizes[class_index];
    }
    char *space_head = sys_calloc (growth, 1);	//system malloc, use byte-sized type
    assert (space_head != NULL);	//ran out of sys memory
    header *head;
    for (class_index = 0; class_index < NUM_CLASSES; class_index++) {
        size = sizes[class_index];
        counts[class_index] += blocks[class_index];
        if (headers[class_index] == NULL) {
            //list was empty
            headers[class_index] = CAST_H (space_head);
            space_head += size;
            blocks[class_index]--;
        }
        for (blocks_left = blocks[class_index]; blocks_left; blocks_left--) {
            CAST_H (space_head)->free.next = CAST_SH (headers[class_index]);
            head = headers[class_index];
            head->free.prev = CAST_SH (space_head); //the header is readable
            head = CAST_H (space_head);
            space_head += size;
            headers[class_index] = head;
        }
    }
#ifndef NDEBUG //sanity check, make sure the last byte is allocated
    header* check = headers[NUM_CLASSES - 1];
    char* end_byte = ((char*) check) + sizes[NUM_CLASSES - 1];
    *end_byte = '\0'; //write an arbitary value
#endif
}
// Get the head of the free list. This uses get_index and additional logic for PPR execution
static inline header * get_header (size_t size, int *which) {
    header* found = NULL;
    //requested allocation is too big
    if (size > MAX_SIZE) {
        *which = -1;
        return NULL;
    } else {
        *which = get_index (size);
        found = headers[*which];
    }
    //clean up
    if (found == NULL || (!SEQUENTIAL && CAST_SH(found) == ends[*which]->free.next))
        return NULL;
    return found;
}

// BOP-safe malloc implementation based off of size classes.
void *dm_malloc (const size_t size) {
    if(size == 0)
        return NULL;
    get_lock();
    //get the right header
    size_t alloc_size = ALIGN (size + HSIZE);
    int which = -2;
    header *block = get_header (alloc_size, &which);
    assert (which != -2);
    ASSERT_VALID_COUNT(which);
    if (block == NULL) {
        //no item in list. Either correct list is empty OR huge block
        if (SEQUENTIAL && alloc_size > MAX_SIZE) {
            //huge block always use system malloc
            block = sys_calloc (alloc_size, 1);
            if (block == NULL) {
                release_lock();
                return NULL;
            }
            //don't need to add to free list, just set information
            block->allocated.blocksize = alloc_size;
            ASSERTBLK(block);
            release_lock();
            return PAYLOAD (block);
        } else if (which < NUM_CLASSES - 1 && index_bigger (which) != -1) {
#ifndef NDEBUG
            splits++;
#endif
            block = dm_split (which);
            ASSERTBLK(block);
        } else if (SEQUENTIAL) {
#ifndef NDEBUG
            if (index_bigger (which) != -1)
                missed_splits++;
#endif
            grow (1);
            block = headers[which];
            block->allocated.blocksize = sizes[which];
            assert (block != NULL);
        } else {
            //bop_abort
        }
    } else
        block->allocated.blocksize = sizes[which];
    ASSERTBLK(block);
    assert (block != NULL);
    //actually allocate the block
    headers[which] = CAST_H (block->free.next);	//remove from free list
    counts[which]--;
    if(!SEQUENTIAL)
        add_next_list(&allocatedList, block);
    ASSERT_VALID_COUNT(which);
    ASSERTBLK(block);
    release_lock();
    return PAYLOAD (block);
}

// Compute the index of the next lagest index > which st the index has a non-null headers
static inline int index_bigger (int which) {
    if (which == -1)
        return -1;
    which++;
    while (which < NUM_CLASSES) {
        if (headers[which] != NULL)
            return which;
        which++;
    }
    return -1;
}

// Repeatedly split a larger block into a block of the required size
static inline header* dm_split (int which) {
#ifdef VISUALIZE
    printf("s");
#endif
#ifndef NDEBUG
    split_attempts[which]++;
    split_gave_head[which]++;
#endif
    CHECK_EMPTY(which);

    int larger = index_bigger (which);
    header *block = headers[larger];	//block to split up
    header *split = CAST_H((CHARP (block) + sizes[which]));	//cut in half
    assert (block != split);
    //split-specific info sets
    headers[which] = split;	// was null
    headers[larger] = CAST_H (headers[larger]->free.next);
    //remove split up block
    block->allocated.blocksize = sizes[which];

    block->free.next = CAST_SH (split);
    split->free.next = split->free.prev = NULL;

    counts[which] = 2; //for when the count is decremented by dm_malloc
    counts[larger]--;

    assert (block->allocated.blocksize != 0);
    which++;
#ifndef NDEBUG
    if (headers[which] == NULL && which != larger)
        multi_splits++;
    split_gave_head[which]+= larger - which;
#endif
    while (which < larger) {
        //update the headers
        split = CAST_H ((CHARP (split) + sizes[which - 1]));
        memset (split, 0, HSIZE);
        CHECK_EMPTY(which);
        headers[which] = split;
        counts[which] = 1;
        which++;
    }
    return block;
}
// standard calloc using malloc
void * dm_calloc (size_t n, size_t size) {
    assert((n * size) >= n && (n * size) >= size); //overflow
    char *allocd = dm_malloc (size * n);
    if(allocd != NULL)
        memset (allocd, 0, size * n);
    ASSERTBLK(HEADER(allocd));
    return allocd;
}

// Reallocator: use sytem realloc with large->large sizes in sequential mode. Otherwise use standard realloc implementation
void * dm_realloc (void *ptr, size_t gsize) {
    header* old_head;
    header* new_head;
    if(gsize == 0)
        return NULL;
    if(ptr == NULL) {
        new_head = HEADER(dm_malloc(gsize));
        ASSERTBLK (new_head);
        return PAYLOAD (new_head);
    }
    old_head = HEADER (ptr);
    ASSERTBLK(old_head);
    size_t new_size = ALIGN (gsize + HSIZE);
    int new_index = get_index (new_size);
    void *payload;		//what the programmer gets
    if (new_index != -1 && sizes[new_index] == old_head->allocated.blocksize) {
        return ptr;	//no need to update
    } else if (SEQUENTIAL && old_head->allocated.blocksize > MAX_SIZE && new_size > MAX_SIZE) {
        //use system realloc in sequential mode for large->large blocks
        new_head = sys_realloc (old_head, new_size);
        new_head->allocated.blocksize = new_size; //sytem block
        new_head->allocated.next = NULL;
        ASSERTBLK (new_head);
        return PAYLOAD (new_head);
    } else {
        //build off malloc and free
        ASSERTBLK(old_head);
        size_t size_cache = old_head->allocated.blocksize;
        //we're reallocating within managed memory
        payload = dm_malloc (gsize); //use the originally requested size
        if(payload == NULL) //would happen if reallocating a block > MAX_SIZE in PPR
            return NULL;
        size_t copySize = MIN(size_cache, new_size) - HSIZE;
        payload = memcpy (payload, PAYLOAD(old_head), copySize);	//copy memory, don't copy the header
        new_head = HEADER(payload);
        assert (new_index == -1 || new_head->allocated.blocksize == sizes[new_index]);
        old_head->allocated.blocksize = size_cache;
        ASSERTBLK(old_head);
        dm_free (ptr);
        ASSERTBLK(HEADER(payload));
        return payload;
    }
}

/*
 * Free a block if any of the following are true
 *	1) Any sized block running in SEQ mode
 *	2) Small block allocated and freed by this PPR task.
 *	A free is queued to be free'd at BOP commit time otherwise.
*/
void dm_free (void *ptr) {
    header *free_header = HEADER (ptr);
    ASSERTBLK(free_header);
    if(SEQUENTIAL || remove_from_alloc_list (free_header))
        free_now (free_header);
    else
        add_next_list(&freedlist, free_header);
}
//free a (regular or huge) block now. all saftey checks must be done before calling this function
static inline void free_now (header * head) {
    int which;
    size_t size = head->allocated.blocksize;
    ASSERTBLK(head);
    assert (size >= HSIZE && size == ALIGN (size));	//size is aligned, ie right value was written
    //test for system block
    if (size > MAX_SIZE && SEQUENTIAL) {
        sys_free(head);
        return;
    }
    //synchronised region
    get_lock();
    header *free_stack = get_header (size, &which);
    assert (sizes[which] == size);	//should exactly align
    if (free_stack == NULL) {
        //empty free_stack
        head->free.next = head->free.prev = NULL;
        headers[which] = head;
        counts[which]++;
        release_lock();
        return;
    }
    free_stack->free.prev = CAST_SH (head);
    head->free.next = CAST_SH (free_stack);
    headers[which] = head;
    counts[which]++;
    release_lock();
}
inline size_t dm_malloc_usable_size(void* ptr) {
    header *free_header = HEADER (ptr);
    size_t head_size = free_header->allocated.blocksize;
    if(head_size > MAX_SIZE)
        return sys_malloc_usable_size (free_header) - HSIZE; //what the system actually gave
    return head_size - HSIZE; //even for system-allocated chunks.
}
/*malloc library utility functions: utility functions, debugging, list management etc */
static inline header* remove_from_alloc_list (header * val) {
    //remove val from the list
    if(allocatedList == val) { //was the head of the list
        allocatedList = NULL;
        return val;
    }
    header* current, * prev = NULL;
    for(current = allocatedList; current; prev = current, current = CAST_H(current->allocated.next)) {
        if(current == val) {
            prev->allocated.next = current->allocated.next;
            return current;
        }
    }
    return NULL;
}
static inline bool list_contains (header * list, header * search_value) {
    if (list == NULL || search_value == NULL)
        return false;
    header *current;
    for (current = list; current != NULL; current = CAST_H (current->free.next)) {
        if (current == search_value)
            return true;
    }
    return false;
}
//add an allocated item to the allocated list
static inline void add_next_list (header** list_head, header * item) {
    if(*list_head == NULL)
        *list_head = item;
    else {
        item->allocated.next = CAST_SH(*list_head);
        *list_head = item;
    }
}
/**Print debug info*/
void dm_print_info (void) {
#ifndef NDEBUG
    setlocale(LC_ALL, "");
    int i;
    printf("******DM Debug info******\n");
    printf ("Grow count: %'d\n", grow_count);
    printf("Max grow size: %'d B\n", GROW_S);
    printf("Total managed mem: %'d B\n", growth_size);
    printf("Differnce in actual & max: %'d B\n", (grow_count * (GROW_S)) - growth_size);
    for(i = 0; i < NUM_CLASSES; i++) {
        printf("\tSplit to give class %d (%'d B) %d times. It was given %d heads\n",
               i+1, sizes[i], split_attempts[i],split_gave_head[i]);
    }
    printf("Splits: %'d\n", splits);
    printf("Miss splits: %'d\n", missed_splits);
    printf("Multi splits: %'d\n", multi_splits);
    for(i = 0; i < NUM_CLASSES; i++)
        printf("Class %d had %'d remaining items\n", i+1, counts[i]);
#else
    printf("dm malloc not compiled in debug mode. Recompile without NDEBUG defined to keep track of debug information.\n");
#endif
}
