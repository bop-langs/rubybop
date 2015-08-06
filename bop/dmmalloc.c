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
#include "bop_api.h"
#include "bop_ports.h"
#include "utils.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#define LOG(x) llog2(x)
#else
#include <math.h>
#define LOG(x) log2(x)
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*NOTE DM_BLOCK_SIZE is only assessed at library compile time, so this is not visible for the user to change
 *Don't try to use this in compiling a bop program, it will not work
 *I examined getting this to work like BOP_Verbose and Group_Size, but it would likely cause more of a slowdown
 *then it is worth*/


#define FREEDLIST_IND -10
//BOP macros & structures
#define SEQUENTIAL() 1
// (bop_mode == SERIAL || BOP_task_status() == SEQ || BOP_task_status() == UNDY) 		//might need to go back and fix

typedef struct {
    header *start[DM_NUM_CLASSES];
    header *end[DM_NUM_CLASSES];
} ppr_list;

ppr_list *regions = NULL;
//header info
header *headers[DM_NUM_CLASSES] = {[0 ... DM_NUM_CLASSES - 1] = NULL};	//current heads of free lists


header* allocatedList= NULL; //list of items allocated during PPR-mode NOTE: info of allocated block
header* freedlist= NULL; //list of items freed during PPR-mode. NOTE: has info of an allocated block

header* ends[DM_NUM_CLASSES] = {[0 ... DM_NUM_CLASSES - 1] = NULL}; //end of lists in PPR region

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
static int split_attempts[DM_NUM_CLASSES];
static int split_gave_head[DM_NUM_CLASSES];
#endif

#define FORCE_INLINE inline __attribute__((always_inline))

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
/**Note: This is the INDEX of the old array of the class index, ie param 0
 * means SIZE class 1. The original implementation had an array that had these values*/
const FORCE_INLINE size_t size_of_klass(int klass_index){
	return SIZE_C(klass_index + 1);
}
const FORCE_INLINE int goal_blocks(int klass){
	switch(klass){
		case 0 ... 3:
			return DM_BLOCK_SIZE * 10;
		case 4 ... 12:
			return DM_BLOCK_SIZE;
		case 13 ... 16:
			return MAX(DM_BLOCK_SIZE / (klass - 8), 1);
		default:
			return 1;
	}
}
static int * goal_counts(){
	static int goal[DM_NUM_CLASSES] = {[0 ... (DM_NUM_CLASSES -1)] = -1};
	int ind;
	if( goal[0] == -1)
  	for(ind = 0; ind < DM_NUM_CLASSES; ind++)
  		goal[ind] = goal_blocks(ind);
  return goal;
}
void dm_check(void* payload) {
    if(payload == NULL)
			return;
    header* head = HEADER (payload);
    ASSERTBLK (head);
}
static inline size_t align(size_t size, size_t alignment) {
    int log = LOG(alignment);
    bop_assert(alignment == (1 << log));
    return (((size) + (alignment-1)) & ~(alignment-1));
}
/**Get the index corresponding to the given size. If size > MAX_SIZE, then return -1*/
static inline int get_index (size_t size) {
    bop_assert (size == ALIGN (size));
    bop_assert (size >= HSIZE);
    //Space is too big.
    if (size > MAX_SIZE)
        return -1;			//too big
    //Computations for the actual index, off set of -5 from macro & array indexing
    int index = LOG(size) - DM_CLASS_OFFSET - 1;

    if (index == -1 || size_of_klass(index) < size)
        index++;
    bop_assert (index >= 0 && index < DM_NUM_CLASSES);
    bop_assert (size_of_klass(index) >= size); //this size class is large enough
    bop_assert (index == 0 || size_of_klass(index - 1) < size); //using the minimal valid size class
    return index;
}
/**Locking functions*/
#ifdef USE_LOCKS
static pthread_mutex_t lock;
static inline void get_lock() {
    pthread_mutex_lock(&lock);
}
static inline void release_lock() {
    pthread_mutex_unlock(&lock);
}
#else
static inline void get_lock() {/*Do nothing*/}
static inline void release_lock() {/*Do nothing*/}
#endif //use locks
/** Bop-related functionss*/

/** Divide up the currently allocated groups into regions.
	Insures that each task will have a percentage of a SEQUENTIAL() goal*/

static int* count_lists(bool is_locked){ //param unused
	static int counts[DM_NUM_CLASSES];
	int i, loc_count;
	if( ! is_locked)
		get_lock();
	header * head;
	for(i = 0; i < DM_NUM_CLASSES; i++){
		loc_count = 0;
		for(head = headers[i]; head; head = CAST_H(head->free.next))
			loc_count ++;
		counts[i] = loc_count;
	}
	if( ! is_locked )
		release_lock();
	return counts;
}

void carve () {
	int tasks = BOP_get_group_size();
	if( regions != NULL)
		dm_free(regions); //dm_free -> don't have lock
	regions = dm_calloc (tasks, sizeof (ppr_list));
	get_lock(); //now locked
	int * counts = count_lists(true); // true -> we have the lock
	grow(tasks); //need to already have the lock
	bop_assert (tasks >= 2);

	int index, count, j, r;
	header *current_headers[DM_NUM_CLASSES];
	header *temp = (header*) -1;
	for (index = 0; index < DM_NUM_CLASSES; index++)
	current_headers[index] = CAST_H (headers[index]);
	//actually split the lists
	for (index = 0; index < DM_NUM_CLASSES; index++) {
		count = counts[index] / tasks;
		for (r = 0; r < tasks; r++) {
			regions[r].start[index] = current_headers[index];
			temp = CAST_H (current_headers[index]->free.next);
			for (j = 0; j < count && temp; j++) {
				temp = CAST_H(temp->free.next);
			}
			current_headers[index] = temp;
      // the last task has no tail, use the same as seq. exectution
      bop_assert (temp != (header*) -1);
      regions[r].end[index] = temp != NULL ? CAST_H (temp->free.prev) : NULL;
		}
	}
	release_lock();
}

/**set the range of values to be used by this PPR task*/
void initialize_group () {
		bop_msg(2,"DM Malloc initializing spec task %d", spec_order);
	  int group_num = spec_order;
    ppr_list my_list = regions[group_num];
    int ind;
    for (ind = 0; ind < DM_NUM_CLASSES; ind++) {
        ends[ind] = my_list.end[ind];
        headers[ind] = my_list.start[ind];
				bop_assert(headers[ind] != ends[ind]); //
				// bop_msg(3, "DM malloc task %d header[%d] = %p", spec_order, ind, headers[ind]);
    }
}

/** Merge
1) Promise everything in both allocated and free list
*/
//NOTE: only the heads should be promised. The payloads should be the job of the caller?
void malloc_promise() {
    header* head;
    for(head = allocatedList; head != NULL; head = CAST_H(head->allocated.next))
        BOP_promise(head, head->allocated.blocksize); //playload matters
    for(head = freedlist; head != NULL; head = CAST_H(head->free.next))
        BOP_promise(head, HSIZE); //payload doesn't matter
}

/** Standard malloc library functions */
//Grow the managed space so that each size class as tasks * their goal block counts
static inline void grow (const int tasks) {
    int class_index, blocks_left, size;
    if(tasks > 1)
      bop_debug("growing tasks = %d", tasks);
#ifndef NDEBUG
    grow_count++;
#endif
		int * goal_counts_arr = goal_counts();
    //compute the number of blocks to allocate
    size_t growth = HSIZE;
    int blocks[DM_NUM_CLASSES];
		int * counts = count_lists(true); //we have the lock
    for(class_index = 0; class_index < DM_NUM_CLASSES; class_index++) {
        blocks_left = tasks * goal_counts_arr[class_index] - counts[class_index];
        blocks[class_index] =  blocks_left >= 0 ? blocks_left : 0;
        growth += blocks[class_index] * size_of_klass(class_index);
    }
    char *space_head = sys_calloc (growth, 1);	//system malloc, use byte-sized type
    bop_assert (space_head != NULL);	//ran out of sys memory
    header *head;
    for (class_index = 0; class_index < DM_NUM_CLASSES; class_index++) {
        size = size_of_klass(class_index);
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
    header* check = headers[DM_NUM_CLASSES - 1];
    char* end_byte = ((char*) check) + MAX_SIZE - 1;
    *end_byte = '\0'; //write an arbitary value
#endif
}
static inline header * extract_header_freed(size_t size){
	//find an free'd block that is large enough for size. Also removes from the list
  return NULL;
	header * list_current,  * prev;
	for(list_current = freedlist, prev = NULL; list_current != NULL;
			prev = list_current,	list_current = CAST_H(list_current->free.next)){
		if(list_current->allocated.blocksize >= size){
			//remove and return
			if(prev == NULL){
				//list_current head of list
        assert(list_current == freedlist);
				freedlist = CAST_H(freedlist->free.next);
				return list_current;
			}else{
				prev->free.next = list_current->free.next;
				return list_current;
			}
		}
	}
	return NULL;
}
// Get the head of the free list. This uses get_index and additional logic for PPR execution
static inline header * get_header (size_t size, int *which) {
	header* found = NULL;
	int temp = -1;
	//requested allocation is too big
	if (size > MAX_SIZE) {
		found = NULL;
		temp = -1;
		goto write_back;
	} else {
		temp = get_index (size);
		found = headers[temp];
		//don't jump to the end. need next conditional
	}
	if ( !SEQUENTIAL() ){
  		if( found == NULL || (ends[temp] != NULL && CAST_SH(found) == ends[temp]->free.next) ) {
  		bop_msg(2, "Area where get_header needs ends defined:\n value of ends[which]: %p\n value of which: %d", ends[*which], *which);
  		//try to allocate from the freed list. Slower
  		found =  extract_header_freed(size);
  		if(found)
  			temp = FREEDLIST_IND;
  		else
  			temp = -1;
  		//don't need go to. just falls through
  	}
  }
	write_back:
	if(which != NULL)
		*which = temp;
	return found;
}
int has_returned = 0;
extern void BOP_malloc_rescue(char *, size_t);
// BOP-safe malloc implementation based off of size classes.
void *dm_malloc (const size_t size) {
	header * block = NULL;
	int which;
	size_t alloc_size;
	if(size == 0)
	return NULL;

	alloc_size = ALIGN (size + HSIZE); //same regardless of task_status
	get_lock();
	//get the right header
 malloc_begin:
	which = -2;
	block = get_header (alloc_size, &which);
	bop_assert (which != -2);
	if (block == NULL) {
		//no item in list. Either correct list is empty OR huge block
		if (alloc_size > MAX_SIZE) {
			if(SEQUENTIAL()){
				//huge block always use system malloc
				block = sys_malloc (alloc_size);
				if (block == NULL) {
					//ERROR: ran out of system memory. malloc rescue won't help
					return NULL;
				}
				//don't need to add to free list, just set information
				block->allocated.blocksize = alloc_size;
				goto checks;
			}else{
				//not SEQUENTIAL(), and allocating a too-large block. Might be able to rescue
				BOP_malloc_rescue("Large allocation in PPR", alloc_size);
				goto malloc_begin; //try again
			}
		} else if (SEQUENTIAL() && which < DM_NUM_CLASSES - 1 && index_bigger (which) != -1) {
#ifndef NDEBUG
			splits++;
#endif
			block = dm_split (which);
			ASSERTBLK(block);
		} else if (SEQUENTIAL()) {
			//grow the allocated region
#ifndef NDEBUG
			if (index_bigger (which) != -1)
			missed_splits++;
#endif
			grow (1);
			goto malloc_begin;
		} else {
			BOP_malloc_rescue("Need to grow the lists in non-SEQUENTIAL()", alloc_size);
			//grow will happen at the next pass through...
			goto malloc_begin; //try again
			//bop_abort
		}
	}
	if(!SEQUENTIAL())
		add_next_list(&allocatedList, block);

	//actually allocate the block
	if(which != FREEDLIST_IND){
		block->allocated.blocksize = size_of_klass(which);
		// ASSERTBLK(block); unneed
		headers[which] = CAST_H (block->free.next);	//remove from free list
	}else{
    bop_msg(2, "Allocated from the free list head addr %p size %u", block, block->allocated.blocksize);
  }
 checks:
	ASSERTBLK(block);
	release_lock();
	has_returned = 1;
	return PAYLOAD (block);
}
void print_headers(){
	int ind;
	// grow(1); //ek
	for(ind = 0; ind < DM_NUM_CLASSES; ind++){
		bop_msg(1, "headers[%d] = %p get_header = %p", ind, headers[ind], get_header(size_of_klass(ind), NULL));
	}
}

// Compute the index of the next lagest index > which st the index has a non-null headers
static inline int index_bigger (int which) {
    if (which == -1)
        return -1;
    which++;
    while (which < DM_NUM_CLASSES) {
        if (get_header(size_of_klass(which), NULL) != NULL)
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
    if(!is_sequential()) bop_msg(4, "Splitting in PPR task");
    int larger = index_bigger (which);
    header *block = headers[larger];	//block to split up
    header *split = CAST_H((CHARP (block) + size_of_klass(which)));	//cut in half
    bop_assert (block != split);
    //split-specific info sets
    headers[which] = split;	// was null PPR Safe
    headers[larger] = CAST_H (headers[larger]->free.next); //PPR Safe
    //remove split up block
    block->allocated.blocksize = size_of_klass(which);

    block->free.next = CAST_SH (split);
    split->free.next = split->free.prev = NULL;

    bop_assert (block->allocated.blocksize != 0);
    which++;
#ifndef NDEBUG
    if (get_header(size_of_klass(which), NULL) == NULL && which != larger)
        multi_splits++;
    split_gave_head[which] += larger - which;
#endif
		bop_assert (which < DM_NUM_CLASSES);
    while (which < larger) {
        //update the headers
        split = CAST_H ((CHARP (split) + size_of_klass(which - 1))); //which - 1 since only half of the block is used here. which -1 === size / 2
				// bop_msg(1, "Split addr %p val %c", split, *((char*) split));
				if(SEQUENTIAL()){
          assert(headers[which] == NULL);
          memset (split, 0, HSIZE);
					headers[which] = split;
				}else{
					//go through dm_free
					split->allocated.blocksize = size_of_klass(which);
					dm_free(split);
				}
        which++;
    }
    return block;
}
// standard calloc using malloc
void * dm_calloc (size_t n, size_t size) {
    header * head;
    char *allocd = dm_malloc (size * n);
    if(allocd != NULL){
        head = HEADER(allocd);
        assert(head->allocated.blocksize >= (size*n));
        memset (allocd, 0, size * n);
    		ASSERTBLK(HEADER(allocd));
		}
    return allocd;
}

// Reallocator: use sytem realloc with large->large sizes in SEQUENTIAL() mode. Otherwise use standard realloc implementation
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
    if (new_index != -1 && size_of_klass(new_index) <= old_head->allocated.blocksize) {
        return ptr;	//no need to update
    } else if (SEQUENTIAL() && old_head->allocated.blocksize > MAX_SIZE && new_size > MAX_SIZE) {
        //use system realloc in SEQUENTIAL() mode for large->large blocks
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
        bop_assert (new_index == -1 || new_head->allocated.blocksize == size_of_klass(new_index));
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
    if(SEQUENTIAL() || remove_from_alloc_list (free_header))
        free_now (free_header);
    else
        add_next_list(&freedlist, free_header);
}
//free a (regular or huge) block now. all saftey checks must be done before calling this function
static inline void free_now (header * head) {
    int which;
    size_t size = head->allocated.blocksize;
    ASSERTBLK(head);
    bop_assert (size >= HSIZE && size == ALIGN (size));	//size is aligned, ie right value was written
    //test for system block
    if (size > MAX_SIZE){
      if(SEQUENTIAL() ) {
        sys_free(head);
      }else{
        get_lock();
        add_next_list(&freedlist, head);
        release_lock();
      }
      return;
    }
    //synchronised region
    get_lock();
    header *free_stack = get_header (size, &which);
    bop_assert (size_of_klass(which) == size);	//should exactly align
    if (free_stack == NULL) {
        //empty free_stack
        head->free.next = head->free.prev = NULL;
        headers[which] = head;
        release_lock();
        return;
    }
    free_stack->free.prev = CAST_SH (head);
    head->free.next = CAST_SH (free_stack);
    headers[which] = head;
    release_lock();
}
inline size_t dm_malloc_usable_size(void* ptr) {
		if(ptr == NULL)
			return 0;
    header *free_header = HEADER (ptr);
    size_t head_size = free_header->allocated.blocksize;
    if(head_size > MAX_SIZE){
        return free_header->allocated.blocksize = sys_malloc_usable_size (free_header) - HSIZE;
          //what the system actually gave, and write back the better-known value. (debugging)
    }
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
            if(prev != NULL){
              prev->allocated.next = current->allocated.next;
            }else{
              allocatedList = NULL;
            }
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
    else if (item != *list_head){ //prevent loops
        item->allocated.next = CAST_SH(*list_head);
        *list_head = item;
    }
}
/**Print debug info*/
void dm_print_info (void) {
#ifndef NDEBUG
    setlocale(LC_ALL, "");
		int * counts = count_lists(true); //we don't actually have the lock, but don't care about thread saftey here
    int i;
		int GROW_S = 0;
		for(i = 0; i < DM_NUM_CLASSES; i++)
			GROW_S += size_of_klass(i);
    printf("******DM Debug info******\n");
    printf ("Grow count: %'d\n", grow_count);
    printf("Max grow size: %'d B\n", GROW_S);
    printf("Total managed mem: %'d B\n", growth_size);
    printf("Differnce in actual & max: %'d B\n", (grow_count * (GROW_S)) - growth_size);
    for(i = 0; i < DM_NUM_CLASSES; i++) {
        printf("\tSplit to give class %d (%'lu B) %d times. It was given %d heads\n",
               i+1, size_of_klass(i), split_attempts[i],split_gave_head[i]);
    }
    printf("Splits: %'d\n", splits);
    printf("Miss splits: %'d\n", missed_splits);
    printf("Multi splits: %'d\n", multi_splits);
    for(i = 0; i < DM_NUM_CLASSES; i++)
        printf("Class %d had %'d remaining items\n", i+1, counts[i]);
#else
    printf("dm malloc not compiled in debug mode. Recompile without NDEBUG defined to keep track of debug information.\n");
#endif
}

bop_port_t bop_alloc_port = {
	.ppr_group_init		= carve,
	.ppr_task_init		= initialize_group,
	.task_group_commit	= malloc_promise
};
