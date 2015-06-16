#ifndef MALLOC_WRAPPER_H
#define MALLOC_WRAPPER_H
#include <stddef.h>
void * malloc(size_t);
void * realloc(void *, size_t);
void free(void *);
void * calloc(size_t, size_t);
/*#define malloc(s) dm_malloc(s)
#define calloc(n, s) dm_calloc(n, s)
#define free(p) dm_free(p)
#define realloc(p, s) dm_realloc(p, s)*/


inline void * sys_malloc(size_t);
inline void * sys_realloc(void *, size_t);
inline void sys_free(void *);
inline void * sys_calloc(size_t, size_t);
#endif
