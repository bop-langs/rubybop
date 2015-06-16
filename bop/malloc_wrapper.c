#include "dmmalloc.h"
#include "malloc_wrapper.h"
#undef NDEBUG
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

//http://stackoverflow.com/questions/262439/create-a-wrapper-function-for-malloc-and-free-in-c

//prototypes for the dlsym using calloc workaround
void* tempcalloc(size_t, size_t);
static inline void calloc_init();

static void *(*libc_malloc)(size_t) = NULL;
static void *(*libc_realloc)(void*, size_t) = NULL;
static void *(*libc_free)(void*) = NULL;
static void *(*libc_calloc)(size_t, size_t) = NULL;
static void *(*calloc_func)(size_t, size_t) = tempcalloc; //part of dlsym workaround

#define CHARSIZE 10000
static char calloc_hack[CHARSIZE];
static short initializing = 0;


void* malloc(size_t s){
	void* p = dm_malloc(s);
	assert (p != NULL);
	return p;
}

void* realloc(void *p , size_t s){
	if(p == calloc_hack || p == NULL){ 
		void* payload = dm_malloc(s);
		if(p != NULL)
			payload = memcpy(payload, p, CHARSIZE);
		assert(payload != NULL);
		return payload;
	}
	void* p2 = dm_realloc(p, s);
	assert (p2!=NULL);
	return p2;
	
}
void free(void * p){
	if(p == NULL || p == calloc_hack) return; //FIXME: NULL shouldn't be happening???
	dm_free(p);
}

void * calloc(size_t sz, size_t n){
	calloc_init();
	assert(calloc_func != NULL);
	void* p = calloc_func(sz, n);
	assert (p!=NULL);
	return p;
}

static inline void calloc_init(){
	if(libc_calloc == NULL && !initializing){
		//first allocation
		initializing = 1; //don't recurse
		calloc_func = tempcalloc;
		libc_calloc = dlsym(RTLD_NEXT, "calloc");
		assert(libc_calloc != NULL);
		calloc_func = dm_calloc;
	}
}
void* tempcalloc(size_t s, size_t n){
	int i;
	for(i = 0; i < CHARSIZE; i++)
		calloc_hack[i] = '\0';
	return calloc_hack;
}

inline void * sys_malloc(size_t s){
	if(libc_malloc == NULL)
		libc_malloc = dlsym(RTLD_NEXT, "malloc");
	assert(libc_malloc != NULL);
    return libc_malloc(s);
}
inline void * sys_realloc(void * p , size_t size){
	assert (p != NULL);
	if(p == calloc_hack){
		void* payload = sys_malloc(size);
		payload = memcpy(payload, p, CHARSIZE);
		assert(payload != NULL);
		return payload;
	}
	if(libc_realloc == NULL)
		libc_realloc = dlsym(RTLD_NEXT, "realloc");
	assert(libc_realloc != NULL);
	void* p2 = libc_realloc(p, size);
	assert(p2 != NULL);
	return p2;
}
inline void sys_free(void * p){
	if(p == NULL || p == calloc_hack) return;
	if(libc_free == NULL)
		libc_free = dlsym(RTLD_NEXT, "free");
	assert(libc_free != NULL);
    libc_free(p);
}
inline void * sys_calloc(size_t s, size_t n){
	calloc_init();
	assert(libc_calloc != NULL);
    void* p = libc_calloc(s, n);
    assert (p!=NULL);
	return p;
}
#undef CHARSIZE
