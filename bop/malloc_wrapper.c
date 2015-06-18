#include "dmmalloc.h"
#include "malloc_wrapper.h"
#undef NDEBUG
#define _GNU_SOURCE
#define VISUALIZE

#include <dlfcn.h>
#ifdef VISUALIZE
#include <stdio.h>
#endif
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <malloc.h>
#define TABLESIZE 100000
#define CHARSIZE 100

//http://stackoverflow.com/questions/262439/create-a-wrapper-function-for-malloc-and-free-in-c

//prototypes for the dlsym using calloc workaround
void* tempcalloc(size_t, size_t);
static inline void calloc_init();

static void *(*libc_malloc)(size_t) = NULL;
static void *(*libc_realloc)(void*, size_t) = NULL;
static void (*libc_free)(void*) = NULL;
static void *(*libc_calloc)(size_t, size_t) = NULL;
static size_t (*libc_malloc_usable_size)(void*) = NULL;
static int (*libc_posix_memalign)(void**, size_t, size_t) = NULL;
static void *(*calloc_func)(size_t, size_t) = tempcalloc; //part of dlsym workaround

static void *mallocs[TABLESIZE];
static void *callocs[TABLESIZE];
static void *reallocs[TABLESIZE];
static void *frees[TABLESIZE];
static long long mc=0LL;
static long long cc=0LL;
static long long rc=0LL;
static long long fc=0LL;

static char calloc_hack[CHARSIZE];
static short initializing = 0;
//unsupported malloc operations are aborted immediately
void* memalign(size_t size, size_t boundary){
	printf("\nUNSUPPORTED OPERATION memalign\n");
	abort();
}
int posix_memalign (void **memptr, size_t alignment, size_t size){
	printf("\nUNSUPPORTED OPERATION posix_memalign\n");
	abort();
}
void* aligned_alloc(size_t size, size_t boundary){
	printf("\nUNSUPPORTED OPERATION: aligned_alloc\n");
	abort();
}
void* valloc(size_t size){
	printf("\nUNSUPPORTED OPERATION: valloc\n");
	abort();
}
struct mallinfo mallinfo(){
	printf("\nUNSUPPORTED OPERATION: mallinfo\n");
	abort();
}

void* malloc(size_t s){
#ifdef VISUALIZE
	printf("+");
	fflush(stdout);
#endif
	void* p = dm_malloc(s);
	mallocs[mc] = p;
	mc++;
	assert (p != NULL);
	return p;
}
void* realloc(void *p , size_t s){
#ifdef VISUALIZE
	printf(".");
	fflush(stdout);
#endif
	assert (p != calloc_hack);
	void* p2 = dm_realloc(p, s);
	reallocs[rc] = p2;
	rc++;
	assert (p2!=NULL);
	return p2;
}
void free(void * p){
#ifdef VISUALIZE
	printf("-");
	fflush(stdout);
#endif
	if(p == NULL || p == calloc_hack) return;
	
	frees[fc] = p;
	fc++;
	wrapper_debug();
	dm_free(p);
}

size_t malloc_usable_size(void* ptr){
#ifdef VISUALIZE
	printf(" ");
	fflush(stdout);
#endif
	size_t size = dm_malloc_usable_size(ptr);
	assert(size > 0);
	return size;
}


void * calloc(size_t sz, size_t n){
#ifdef VISUALIZE
	printf("0");
	fflush(stdout);
#endif
	calloc_init();
	//make sure the right calloc_func is being used
	assert(calloc_func != NULL);
	assert( (initializing && calloc_func == tempcalloc) || 
			(!initializing && calloc_func == dm_calloc) );
	void* p = calloc_func(sz, n);
	callocs[cc] = p;
	cc++;
	assert (p!=NULL);
	return p;
}

static inline void calloc_init(){
	if(libc_calloc == NULL && !initializing){
		//first allocation
		initializing = 1; //don't recurse
		calloc_func = tempcalloc;
		libc_calloc = dlsym(RTLD_NEXT, "calloc");
		initializing = 0;
		assert(libc_calloc != NULL);
		calloc_func = dm_calloc;
	}
}
void* tempcalloc(size_t s, size_t n){
	if(s * n > CHARSIZE){
		abort();
		return NULL;
	}
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
	assert(p != calloc_hack);
	if(libc_realloc == NULL)
		libc_realloc = dlsym(RTLD_NEXT, "realloc");
	assert(libc_realloc != NULL);
	void* p2 = libc_realloc(p, size);
	assert(p2 != NULL);
	return p2;
}
inline void sys_free(void * p){
	if(p == calloc_hack) 
		return;
	if(libc_free == NULL)
		libc_free = dlsym(RTLD_NEXT, "free");
	assert(libc_free != NULL);
    libc_free(p);
}
inline size_t sys_malloc_usable_size(void* p){
	if(libc_malloc_usable_size == NULL)
		libc_malloc_usable_size = dlsym(RTLD_NEXT, "malloc_usable_size");
	assert (libc_malloc_usable_size != NULL);
	return libc_malloc_usable_size(p);
}
inline int sys_posix_memalign(void** p, size_t a, size_t s){
	if(libc_posix_memalign == NULL)
		libc_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
	assert(libc_posix_memalign != NULL);
	return libc_posix_memalign(p, a, s);
}

inline void * sys_calloc(size_t s, size_t n){
	calloc_init();
	assert(libc_calloc != NULL);
    void* p = libc_calloc(s, n);
    assert (p!=NULL);
	return p;
}

//debug information
void wrapper_debug(){
#ifdef VISUALIZE
	printf("\nmalloc count %lld\n", mc);
	printf("calloc count %lld\n", cc);
	printf("realloc count %lld\n", rc);
	printf("free count %lld\n", fc);
	int found = 0;
	long long f = 0LL;
	long long m = 0LL;
	long long c =  0LL;
	long long r = 0LL;
	for(f = 0LL; f < fc; f++){
		found = 0;
		for(m = 0LL; !found && m < mc; m++){
			if(mallocs[m] == frees[f])
				found = 1;
		}
		for(c = 0LL; !found && c < cc; c++){
			if(callocs[c] == frees[f])
				found = 1;
		}
		for(r = 0LL; !found && r < rc; r++){
			if(reallocs[r] == frees[f])
				found = 1;
		}
		if(found == 0){
			printf("Freed unallocated block: %p", frees[f]);
			abort();
		}
	}
	printf("ALL FREES PASS\n");
#else
	printf("frees not tracked");
#endif
}

#undef CHARSIZE
