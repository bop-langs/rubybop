#ifndef MALLOC_WRAPPER_H
#define MALLOC_WRAPPER_H
#include <stddef.h>
void * malloc(size_t);
void * realloc(void *, size_t);
void free(void *);
void * calloc(size_t, size_t);

#ifdef UNSUPPORTED_MALLOC
int posix_memalign(void**, size_t alignment, size_t size);
void* aligned_malloc(size_t alignment, size_t size);
size_t malloc_usable_size(void*);
void* memalign(size_t size, size_t boundary);
void* aligned_alloc(size_t size, size_t boundary);
void* valloc(size_t size);
struct mallinfo mallinfo();
#endif

void * sys_malloc(size_t);
void * sys_realloc(void *, size_t);
void sys_free(void *);
void * sys_calloc(size_t, size_t);
size_t sys_malloc_usable_size(void*);
int sys_posix_memalign(void**, size_t, size_t);
#endif
