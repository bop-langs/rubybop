#ifndef DM_MALLOC_H

#define DM_MALLOC_H
#include <stddef.h>

//prototypes
void * dm_malloc(size_t);
void * dm_realloc(void *, size_t);
void dm_free(void *);
void * dm_calloc(size_t, size_t);
void dm_print_info(void);
size_t dm_malloc_usable_size(void*);

//initializers
void carve(int); //divide up avaliable memory
void initialize_group(int);

typedef union{
	//NOTE: the two nexts must be the same address for sum utility functions in dmmalloc.c
	struct{
		struct header * next;   // ppr-allocated object list
		size_t blocksize; // which free list to insert freed items into
		char padding[7]; //....
	} allocated;
	struct{
        //doubly linked free list for partioning
		struct header * next;
		struct header * prev;
		
	} free;
} header;

//data accessors for merge time
void get_lists(header* freed, header* allocated); //give data
void update_internal_lists(header * freed, header * allocated); //update my data
#endif
