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
void carve(int); //divide up avaliable memory
void initialize_group(int); //set end pointers for this ppr task

//data accessors for merge time
void malloc_merge(void);
void malloc_merge_counts(bool); //counts get updated AFTER abort status is known
#endif
