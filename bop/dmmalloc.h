#ifndef DM_MALLOC_H
#define DM_MALLOC_H

#define DM_DEBUG
#include <stddef.h>
#include <stdbool.h>

//dm structs, unions etc
typedef union {
    //NOTE: the two nexts must be the same address for some utility functions in dmmalloc.c
    struct {
        union header * next;   // ppr-allocated object list
        size_t blocksize; // which free list to insert freed items into
    } allocated;
    struct {
        //doubly linked free list for partioning
        union header * next;
        union header * prev;

    } free;
} header;

//prototypes
void * dm_malloc(size_t);
void * dm_realloc(void *, size_t);
void dm_free(void *);
void * dm_calloc(size_t, size_t);
void dm_print_info(void);
size_t dm_malloc_usable_size(void*);
void dm_check(void*);

//bop-related functions
void carve(); //divide up avaliable memory
void initialize_group(); //set end pointers for this ppr task

//data accessors for merge time
void malloc_merge(void);
void malloc_merge_counts(bool); //counts get updated AFTER abort status is known


//malloc config macros
#ifndef DM_BLOCK_SIZE
#define DM_BLOCK_SIZE 200
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
#define ASSERTBLK(head) bop_assert ((head)->allocated.blocksize > 0);

//class size macros
#define DM_NUM_CLASSES 16
#define DM_CLASS_OFFSET 4 //how much extra to shift the bits for size class, ie class k is 2 ^ (k + DM_CLASS_OFFSET)
#define MAX_SIZE SIZE_C(DM_NUM_CLASSES)
#define SIZE_C(k) (ALIGN((1 << (k + DM_CLASS_OFFSET))))	//allows for iterative spliting



#endif
