/**Divide & Merge Malloc

A dual-stage malloc implementation to support safe PPR forks
Each stage (sequential/no PPR tasks running) and a PPR tasks’ design is the same, a basic size-class allocator. The complications come when PPR_begin is called:
Allocating
A PPR task is given part of the parent’s free lists to use for its memory. This ensures that there will be no ‘extra’ conflicts at commit time.Td
If there is not enough memory, the parent gets new memory from the system and then gives it to the PPR task (GROUP_INIT)
If a PPR task runs out of memory, it must abort speculation. Calls to the underlying malloc are not guaranteed to not conflict with other.
The under study maintains access to the entire free list. Since either the understudy or the PPR tasks will survive past the commit stage, this is still safe.
At commit time, the free lists of PPR tasks are merged along with the standard BOP changes. This allows memory not used by PPR tasks to be reclaimed and used later.
Freeing
when a PPR task frees something from the global heap (something it did not allocate, eg it was either allocated by a prior PPR task or before and PPRs were started) it marks as freed and moves to a new list. This list is parsed at commit time and is always (???) accepted. We cannot immediately move it into the free list since when allocating a new object of that size. If multiple PPR tasks do this (which is correct in sequential execution) and both allocate the new object, the merge will fail. 	

Large objects:
Size classes need to be finite, so there will be some sizes not handled by this method, the work around is:
    allocation: if in PPR task, abort if not use DL malloc. 
    free: when one of these is freed, check the block size. if it’s too large for any size class it was allocated with dl malloc. use dl free OR if sufficiently large divide up for use in our size classes.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memcpy */
#include <assert.h>

#include "dmmalloc.h"

//Variable bit identifier code.
//#define BIT_IDENTIFIER k uses the kth lowest bit
#define BIT_IDENTIFIER 3
#define HEADER_IDENTIFIER_MASK (1<<(BIT_IDENTIFIER-1))

#define IS_ALLOCED_BY_DM(HEADER_P) (HEADER_P & HEADER_IDENTIFIER_MASK)

//bit masks for marking large objects when allocated. We have 2 bits to play with
#ifndef MASK
#if __WORDSIZE == 64
#define MASK 0xfffffffffffffffc
#define ALIGNMENT 8
#elif __WORDSIZE == 32
#define mask 0xfffffffc
#define ALIGNMENT 4
#else
#define MASK 0
#endif
#endif



//alignment macros
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HSIZE (ALIGN((sizeof(header))))

//header macros
#define HEADER(vp) ((vp) - (sizeof(header)))
#define CASTH(h) ((struct header *) (h))

//class size macros
#define NUM_CLASSES 8
#define MAX_SIZE sizes[NUM_CLASSES - 1]
#define base_size ALIGNMENT //the smallest usable payload, anthing smaller than ALIGNMENT gets rounded up
#define SIZE_C(k) (((k)*ALIGN((((base_size) + HSIZE))))) //allows for successive spliting

typedef struct{
    header * start[NUM_CLASSES];
    header * end[NUM_CLASSES];
} ppr_list;

ppr_list* regions;


//header info
header* headers[NUM_CLASSES];
header* ends[NUM_CLASSES]; //end of lists in PPR region

int  sizes[NUM_CLASSES] = {SIZE_C(1), SIZE_C(2), SIZE_C(3), SIZE_C(4),
                           SIZE_C(5), SIZE_C(6), SIZE_C(7), SIZE_C(8)};
int counts[NUM_CLASSES];

header* allocatedList = NULL;
header* freelist = NULL;

int get_index(int size)
{
    int index = 0;
    //Space is too big. Return maximum possible index.                          
    if (size >= (1<<(NUM_CLASSES-1)))
        return NUM_CLASSES-1;
    //Divide by alignment until we have zero to get index.                            
    while ((size/ALIGNMENT(base_size + HSIZE)>0)
        index++;
    return index;
}

/** Divide up the currently allocated groups into regions*/
void carve(int tasks){
    assert(tasks >= 2);
    regions = dm_malloc(tasks * sizeof(ppr_list));
    int index, count, j, r = 0;
    header* current_headers[NUM_CLASSES];
    header * temp = NULL;
    for(index = 0; index < NUM_CLASSES; index++)
        current_headers[index] = (header *) headers[index];
    //actually split the lists
    for(index = 0; index < NUM_CLASSES; index++){
        count = sizes[index] / tasks;
        for(r = 0; r < tasks; r++){
            regions[r].start[index] = current_headers[index];
            for(j = 0; j < count && temp; j++){
                temp = (header *) current_headers[index]->free.next;
            }
            current_headers[index] = temp;
            if(r != tasks - 1){
                //the last task has no tail, use the same as seq. exectution
                assert(temp != NULL);
                regions[r].end[index] = (header *) temp->free.prev;
            }
        }
    }
}

/**set the range of values to be used by this PPR task*/
void initialize_group(int group_num){
    ppr_list my_list = regions[group_num];
    int ind;
    for(ind = 0; ind < NUM_CLASSES; ind++){
        ends[ind] = my_list.end[ind];
        headers[ind] = my_list.start[ind];
    }
}


/**size: alligned size, includes space for the header*/
static inline header * get_header(size_t size, int * which){
    header * found = NULL;
    int ind;
    //requested allocation is too big
    if(size > MAX_SIZE){
        *which = -1;
        return NULL;
    }else{
    		*which = get_index(size);
            found = headers[*which];
         }
    }
    //clean up
    if(found == NULL || (/*TODO IN_PPR_TASK && */ CASTH(found) == ends[*which]->free.next))
        return NULL;
    return found;
}


//actual malloc implementations
void * dm_malloc(size_t size){
    //get the right header
    size_t alloc_size = ALIGN(size + HSIZE);
    int which = -2;
    header * block = get_header(alloc_size, &which);
    if(block == NULL){
        //no item in list. Either correct list is empty OR huge block
        if(which == -1 /*&& !PPR*/){
            //use system malloc
            block = malloc(alloc_size);
            //don't need to add to free list, just set information
            block->allocated.blocksize = alloc_size; //huge, can check at free for the edge case
        }else{
            //TODO BOP_ABORT() if in PPR
        }
        return block;
    }
    //update the new size class head
    headers[which] = (header *) block->free.next;
    
    if(allocatedList == NULL)
        allocatedList = block;
    else{
        block->allocated.next = (void *) allocatedList;
        allocatedList = block;
    }
    block->allocated.blocksize = sizes[which];
    //update counts
    counts[which]--;
    return (block + HSIZE);
}

void * dm_calloc(size_t n, size_t size){
    void * allocd  = dm_malloc(size * n);
    memset(allocd, 0, size * n);
    return allocd;
}

void * dm_realloc(void * ptr, size_t new_size){
    void * payload = dm_malloc(new_size);
    if(payload == NULL){
        //failed to allocate new memory
        return NULL;    
    }
    header * old_head = HEADER(ptr);
    size_t old_size = old_head->allocated.blocksize;

    memcpy(payload, ptr, old_size);
    free(ptr);
    return payload;
}

void dm_free(void* ptr){
    header * free_header = HEADER(ptr);
    int which = -1, size = free_header->allocated.blocksize;
    header * append_list;
    if(0 /*TODO BOP_Running*/){
        //just add to the free list   
        append_list = freelist;     
    }else{
        if(size > MAX_SIZE){
            free(free_header); //system free
            return;
        }
        append_list =  get_header(size, &which);
        //not in PPR mode, can immediately add to the free list
        headers[which] = free_header;    
        counts[which]++;
    }
    //append to the relevant list
    if(append_list == NULL){
        *append_list = *free_header;
        free_header->free.next = free_header->free.prev = NULL; //one item in the list
    }else{
        append_list->free.prev = CASTH(free_header);
        free_header->free.next = CASTH(append_list);
        append_list = free_header;
    }
}
