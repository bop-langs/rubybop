#include "dmmalloc.h"
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

//http://stackoverflow.com/questions/262439/create-a-wrapper-function-for-malloc-and-free-in-c

//Send standard mallocs to their DM equivalents
void* malloc(size_t s){
	return dm_malloc(s);
}

void* realloc(void *p , size_t s){
	return dm_realloc(p, s);
}
void free(void * p){
	return dm_free(p);
}
void * calloc(size_t sz, size_t n){
	return dm_calloc(sz, n);
}
//Wrappers for use by dmmalloc.c
inline void * sys_malloc(size_t s){
	void *(*libc_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    printf("wrapped malloc\n");
    return libc_malloc(s);
}
inline void * sys_realloc(void * p , size_t size){
	void *(*libc_remalloc)(void*, size_t) = dlsym(RTLD_NEXT, "realloc");
    printf("wrapped REMALLOC\n");
    return libc_remalloc(p, size);
}
inline void sys_free(void * p){
	void *(*libc_free)(void*) = dlsym(RTLD_NEXT, "free");
    printf("wrapped FREE\n");
    libc_free(p);
}
inline void * sys_calloc(size_t s, size_t n){
	void *(*libc_calloc)(size_t, size_t) = dlsym(RTLD_NEXT, "calloc");
    printf("wrapped CALLOC\n");
    return libc_calloc(s, n);
}


