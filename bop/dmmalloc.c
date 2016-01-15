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


short malloc_panic = 0;

//BOP macros & structures
#define SEQUENTIAL() (malloc_panic || bop_mode == SERIAL || BOP_task_status() == SEQ || BOP_task_status() == UNDY)

typedef struct {
    header *start[DM_NUM_CLASSES];
    header *end[DM_NUM_CLASSES];
} ppr_list;

static ppr_list *regions = NULL;
//header info
static header *headers[DM_NUM_CLASSES] = {[0 ... DM_NUM_CLASSES - 1] = NULL};	//current heads of free lists
static header* ends[DM_NUM_CLASSES] = {[0 ... DM_NUM_CLASSES - 1] = NULL}; //end of lists in PPR region

static header* freedlist[DM_NUM_CLASSES] = {[0 ... DM_NUM_CLASSES - 1] = NULL}; //list of items freed during PPR-mode.
static header* large_free_list = NULL;
static header* allocated_lists[DM_NUM_CLASSES]= {[0 ... DM_NUM_CLASSES - 1] = NULL}; //list of items allocated during PPR-mode NOTE: info of allocated block

//helper prototypes
static inline int get_index (size_t);
static inline void grow (int);
static inline void free_now (header *);
static inline bool list_contains (header * list, header * item);
static bool remove_from_alloc_list (header *);
static inline void add_next_list (header**, header *);
static inline void add_freed_list (header*);
static inline header *dm_split (int which, int larger);
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
    int index = LOG(size) - DM_CLASS_OFFSET - 1;

    if (index == -1 || size_of_klass(index) < size) // this is how it gets rounded up only if needed
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

static int* count_lists(bool is_locked){
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
/** BOP Port functions */

void carve () {
  bop_assert(SEQUENTIAL());
  int tasks = BOP_get_group_size();
  if( regions != NULL){
    dm_free(regions); //dm_free -> don't have lock
  }
  regions = dm_calloc (tasks, sizeof (ppr_list) );
  get_lock(); //now locked
  grow(tasks); //need to already have the lock

  int * counts = count_lists(true); // true -> we have the lock
  bop_assert (tasks >= 2);

  int index, count, j, r;
  header *current_headers[DM_NUM_CLASSES];
  header *temp = (header*) -1;
  for (index = 0; index < DM_NUM_CLASSES; index++)
    current_headers[index] = CAST_H (headers[index]);
  //actually split the lists
  for (index = 0; index < DM_NUM_CLASSES; index++) {
    bop_assert(counts[index] > 0);
    count = counts[index] / tasks;
    bop_assert(count > 0);
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
      bop_assert(regions[r].start[index] != regions[r].end[index]);
    }
  }
  release_lock();
}

/**set the range of values to be used by this PPR task*/
void initialize_group () {
  bop_msg(2,"DM Malloc initializing spec task %d", spec_order);
  // bop_assert(! SEQUENTIAL() );
  int group_num = spec_order;
  ppr_list my_list = regions[group_num];
  int ind;
  for (ind = 0; ind < DM_NUM_CLASSES; ind++) {
    ends[ind] = my_list.end[ind];
    if(task_status != MAIN)
      headers[ind] = my_list.start[ind];
    if(group_num != 0){
      bop_assert(headers[ind] != NULL);
    }else{
      bop_assert(task_status == MAIN);
    }
    bop_assert(headers[ind] != ends[ind]);
    // bop_msg(3, "DM malloc task %d header[%d] = %p", spec_order, ind, headers[ind]);
  }
#ifndef NDEBUG
  for(ind = 0; ind < DM_NUM_CLASSES; ind++){
    bop_assert(headers[ind] != NULL);
    bop_assert(headers[ind]->free.next != NULL);
  }
#endif
}


/** Merge
1) Promise everything in both allocated and free list
*/
//NOTE: only the heads should be promised. The payloads should be the job of the caller?
void malloc_promise() {
    header* head;
    int i;
    int allocs = 0, frees = 0;
    for(i = 0; i < DM_NUM_CLASSES; i++){
      for(head = allocated_lists[i]; head != NULL; head = CAST_H(head->allocated.next)){
        BOP_promise(head, head->allocated.blocksize); //playload matters
        allocs++;
      }
    }
    for(i=0; i < DM_NUM_CLASSES; i++){
      for(head = freedlist[i]; head != NULL; head = CAST_H(head->free.next)){
        BOP_promise(head, HSIZE); //payload doesn't matter
        frees++;
      }
    }
    for(head = large_free_list; head != NULL; head = CAST_H(head->free.next)){
      BOP_promise(head, HSIZE); //payload doesn't matter
      frees++;
    }
    bop_msg(3, "Number of promised frees: \t%d", frees);
    bop_msg(3, "Number of promised allocs: \t%d", allocs);
}

void dm_malloc_undy_init(){
  return; //TODO this method should actually work, but it causes Travis to fail.
  //called when the under study begins. Free everything in the freed list
  bop_assert(SEQUENTIAL());
  header * current, *  next;
  int ind;
  for(ind = 0; ind < DM_NUM_CLASSES; ind++){
    for(current = freedlist[ind]; current != NULL; current = CAST_H(current->free.next)){
      dm_free(PAYLOAD(current));
    }
    freedlist[ind] = NULL;
  }
  for(current = large_free_list; current != NULL; current = CAST_H(current->free.next)){
    dm_free(PAYLOAD(current));
  }
  for(ind = 0; ind < DM_NUM_CLASSES; ind++){
    for(current = allocated_lists[ind]; current != NULL; current = next){
      next = CAST_H(current->allocated.next);
      current->allocated.next = NULL;
    }
    allocated_lists[ind] = NULL;
  }
}

static inline void grow (const int tasks) {
  //THIS IS THE NEW GROW FUNCTION!
    int class_index, blocks_left, size;
    if(tasks > 1)
      bop_debug("growing tasks = %d", tasks);
#ifndef NDEBUG
    grow_count++;
#endif
		int * goal_counts_arr = goal_counts();
    //compute the number of blocks to allocate
    // size_t growth = HSIZE;
    int blocks[DM_NUM_CLASSES];
		int * counts = count_lists(true); //we have the lock
    for(class_index = 0; class_index < DM_NUM_CLASSES; class_index++) {
        blocks_left = tasks * goal_counts_arr[class_index] - counts[class_index];
        blocks[class_index] =  blocks_left >= 0 ? blocks_left : 0;
        // growth += blocks[class_index] * size_of_klass(class_index);
    }
    header * head, * list_top;

    char* space_head;
    int current_block;
    char * debug_ptr;
    for (class_index = 0; class_index < DM_NUM_CLASSES; class_index++) {
        size = size_of_klass(class_index);
        space_head = sys_calloc (blocks[class_index], size);	//system malloc, use byte-sized type
        bop_assert (blocks[class_index] == 0 || space_head != NULL);	//ran out of sys memory

        for(current_block = 0; current_block < blocks[class_index]; current_block++){
          //set up this space
          head = (header *) & (space_head[current_block * size]);
          //make sure the last B in the head is writtable
#ifndef NDEBUG
          if(grow_count > 1){ /** Utilization tests */
            debug_ptr  = (char*) head;
            //check stack is growing correctly
            bop_assert(current_block == 0 ||
                headers[class_index] == (header *) & space_head[(current_block - 1) * size]);
            // calloc'd return & seperate from previous head
            bop_assert( *debug_ptr == (char) 0);
            // last byte is readable
            bop_assert(current_block == blocks[class_index] - 1 || debug_ptr[size -1] == (char) 0);
            //adjacent in memory to previous block
            bop_assert( current_block == 0 || debug_ptr[-1] == 'x'); //previous byte is the last of the previous
            debug_ptr[size - 1] = 'x'; //This is an array, so -1 since the highest array index is -1 than length
          }
#endif
          list_top = headers[class_index];
          // add_next_list( &headers[class_index], head);
          if(list_top == NULL){
            headers[class_index] = head;
            head->free.next = head->free.prev = NULL;
          }
          else{
            head->free.next = CAST_UH(list_top);
            list_top->free.prev = CAST_UH(head);
            headers[class_index] = head;
          }
        }
        //post-size class division checks
#ifndef NDEBUG
        if(grow_count > 1){
            //ensure that this list is not a giant loop
            header * current;
            for(current = CAST_H(headers[class_index]->free.next); current; current = CAST_H(current->free.next)){
              bop_assert(current != headers[class_index]);
              bop_assert(current != CAST_H(current->free.next));
            }
        }
#endif
    }
}

// Get the head of the free list. This uses get_index and additional logic for PPR execution
static inline header * get_header (size_t size, int *which) {
	header* found = NULL;
	int temp = -1;
  if (size > MAX_SIZE) {
    found = NULL;
    temp = -1;
  }else{
    temp = get_index (size);
    found = headers[temp];
    if ( !SEQUENTIAL() && found != NULL){
      //this will be useless in sequential mode, and useless if found == NULL
      if(ends[temp] != NULL && CAST_UH(found) == ends[temp]->free.next) {
        bop_msg(3, "Something may have gone wrong:\n value of ends[which]: %p\t value of which: %d", ends[temp], temp);
        found = NULL;
      }
    }
  }
  if(which != NULL)
    *which = temp;
	return found;
}
int alist_added = 0;
extern void BOP_malloc_rescue(char *, size_t);
// BOP-safe malloc implementation based off of size classes.
void *dm_malloc (const size_t size) {
	header * block = NULL;
	int which, bigger = -1;
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
			if(SEQUENTIAL()){ //WORKING!
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
		} else if (which < DM_NUM_CLASSES - 1 && (bigger = index_bigger (which)) != -1) {
			block = dm_split (which, bigger);
			ASSERTBLK(block);
		} else if (SEQUENTIAL()) {
      bop_msg(3, "Is in sequential. Growing");
			grow (1);
			goto malloc_begin;
		} else {
			BOP_malloc_rescue("Need to grow the lists in non-SEQUENTIAL()", alloc_size);
			//grow will happen at the next pass through...
			goto malloc_begin; //try again
			//bop_abort
		}
	}
  block->allocated.blocksize = size_of_klass(which);
  bop_assert (headers[which] != CAST_H (block->free.next));
  headers[which] = CAST_H (block->free.next);	//remove from free list
  // Write allocated next information
  if(  !SEQUENTIAL()){
    bop_assert(which != -1); //valid because -1 == too large, can't do in PPR
    add_next_list(&allocated_lists[which], block);
  }
 checks:
	ASSERTBLK(block);
	release_lock();
	return PAYLOAD (block);
}
void print_headers(){
	int ind;
	for(ind = 0; ind < DM_NUM_CLASSES; ind++){
		bop_msg(1, "headers[%d] = %p get_header = %p", ind, headers[ind], get_header(size_of_klass(ind), NULL));
	}
}

// Compute the index of the next lagest index > which st the index has a non-null headers
static inline int index_bigger (int which) {
    if (which == -1)
        return -1;
    which++;
    int index;
    while (which < DM_NUM_CLASSES) {
      if (get_header(size_of_klass(which), &index) != NULL){
          return which;
      }
      which++;
    }
    return -1;
}
// Repeatedly split a larger block into a block of the required size
static inline header* dm_split (int which, int larger) {
  if(which > 8){
    bop_msg(3, "In large split");
  }
#ifdef VISUALIZE
    printf("s");
#endif
#ifndef NDEBUG
    split_attempts[which]++;
    split_gave_head[which]++;
#endif
    header *block = headers[larger];	//block to split up
    header *split = CAST_H((CHARP (block) + size_of_klass(which)));	//cut in half
    bop_assert (block != split);
    //split-specific info sets

    // headers[which] = split;	// was null PPR Safe
    headers[larger] = CAST_H (headers[larger]->free.next); //PPR Safe
    //remove split up block
    block->allocated.blocksize = size_of_klass(which);

    block->free.next = CAST_UH (split);
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
          release_lock(); //need to let dm_free have the lock
					dm_free(PAYLOAD(split));
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
void * dm_realloc (const void *ptr, size_t gsize) {
    header* old_head,  * new_head;
    size_t new_size = ALIGN(gsize + HSIZE), old_size;
    if(gsize == 0)
        return NULL;
    if(ptr == NULL) {
        new_head = HEADER(dm_malloc(gsize));
        ASSERTBLK (new_head);
        return PAYLOAD (new_head);
    }

    old_head = HEADER (ptr);

    ASSERTBLK(old_head);

    old_size = old_head->allocated.blocksize;
    int new_index = get_index (new_size);

    if (new_index != -1 && size_of_klass(new_index) <= old_head->allocated.blocksize) {
        return (void*) ptr;	//no need to update
    } else if (SEQUENTIAL() && old_head->allocated.blocksize > MAX_SIZE && new_size > MAX_SIZE) {
        //use system realloc in SEQUENTIAL() mode for large->large blocks
        new_head = sys_realloc (old_head, new_size);
        new_head->allocated.blocksize = new_size; //sytem block
        new_head->allocated.next = NULL;
        ASSERTBLK (new_head);
        return PAYLOAD (new_head);
    }
    else {
        //build off malloc and free
        ASSERTBLK(old_head);
        size_t size_cache = old_head->allocated.blocksize;
        //we're reallocating within managed memory

        void* new_payload = dm_malloc(gsize); //malloc will tweak size again.
        if(new_payload == NULL){
          bop_msg(1, "Unable to reallocate %p (size %u) to new size %u", ptr, old_size, new_size);
          return NULL;
        }
        //copy the data
        size_t copy_size = MIN(old_size, new_size) - HSIZE; // block sizes include the header!
        assert(copy_size != new_size - HSIZE);
        bop_assert( HEADER(new_payload)->allocated.blocksize >= (copy_size + HSIZE)); //check dm_malloc gave enough space
        new_payload = memcpy(new_payload, ptr, copy_size); // copy data
        bop_assert( ((header *)HEADER(new_payload))->allocated.blocksize >= copy_size);
        ASSERTBLK(old_head);
        bop_assert(old_head->allocated.blocksize == size_cache);
        dm_free( (void*) ptr);

        return new_payload;
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
    get_lock();
    if(SEQUENTIAL() || remove_from_alloc_list (free_header)){
        release_lock();
        free_now (free_header);
    } else{
       add_freed_list(free_header);
    }

		release_lock();
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
        add_freed_list(head);
        release_lock();
      }
      return;
    }
    //synchronised region
    get_lock();
    header *free_stack = get_header (size, &which);
    bop_assert(size <= MAX_SIZE);
    bop_assert(which != -1);
    if(which != -1)
      bop_assert (size_of_klass(which) == size);	//should exactly align
    if (free_stack == NULL) {
        //empty free_stack
        head->free.next = head->free.prev = NULL;
        headers[which] = head;
        release_lock();
        return;
    }
    free_stack->free.prev = CAST_UH (head);
    head->free.next = CAST_UH (free_stack);
    headers[which] = head;

    release_lock();
}
inline size_t dm_malloc_usable_size(void* ptr) {
		if(ptr == NULL)
			return 0;
    header *free_header = HEADER (ptr);
    size_t head_size = free_header->allocated.blocksize;
    if(head_size > MAX_SIZE){
      head_size = sys_malloc_usable_size(free_header);
    }
    return head_size - HSIZE; //even for system-allocated chunks.
}
/*malloc library utility functions: utility functions, debugging, list management etc */
static bool remove_from_alloc_list (header * val) {
  //remove val from the list
  header* current, * prev = NULL;
  int index;
  for(index = 0; index < DM_NUM_CLASSES; index++){
    for(current = allocated_lists[index]; current; prev = current, current = CAST_H(current->allocated.next)) {
      if(current == val) {
        if(prev == NULL){
          allocated_lists[index] = CAST_H(current->allocated.next);
        }else{
          prev->allocated.next =  CAST_UH(current->allocated.next);
        }
        return true;
      }
    }
  }
  bop_msg(4, "Allocation not found on alloc list");
  return false;
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
static int added_n = 0;
static int nseq_added_n = 0;

static inline void add_next_list (header** list_head, header * item) {
  added_n++;
  if(!SEQUENTIAL())
    nseq_added_n++;
  bop_assert(*list_head != item);
  item->allocated.next = CAST_UH(*list_head); //works even if *list_head == NULL
  *list_head = item;
}

static inline void add_freed_list(header* item){
  size_t size = item->allocated.blocksize;
  int index = get_index(size);
  if(index >= DM_NUM_CLASSES){
    add_next_list(&large_free_list, item);
  }
  item->free.next = CAST_UH(freedlist[index]);
  if(freedlist[index]){
    freedlist[index]->free.prev = CAST_UH(item);
  }
  freedlist[index] = item;
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
	.task_group_commit	= malloc_promise,
  .undy_init = dm_malloc_undy_init
};
