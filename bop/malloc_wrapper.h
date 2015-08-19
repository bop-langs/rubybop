/** @file malloc_wrapper.h
 *	@brief Wrapper file to send system malloc functions to dmmalloc
 *
 *	Contains definitions of standard and system malloc functions
 *	These functions will be wrapped to look for memory leaks
 *	
 *	@author Rubybop
 */

#ifndef MALLOC_WRAPPER_H
#define MALLOC_WRAPPER_H
#include <stddef.h>

/** @name Standard
 *	standard malloc definitions
 */
///@{
/**	@param size_t
 *	@return void*
 */
void * malloc(size_t);
/** @param void*
 *	@param size_t
 *	@return void*
 */
void * realloc(void *, size_t);
/** @param void*
 *	@return void
 */
void free(void *);
/** @param size_t
 *	@param size_t
 *	@return void*
 */
void * calloc(size_t, size_t);
///@}

#ifdef UNSUPPORTED_MALLOC
/**	@param void**
 *	@param size_t alignment
 *	@param size_t size
 *	@return int
 */
int posix_memalign(void**, size_t alignment, size_t size);
/**	@param size_t alignment
 *	@param size_t size
 *	@return void*
 */
void* aligned_malloc(size_t alignment, size_t size);
/** @param void*
 *	@return size_t
 */
size_t malloc_usable_size(void*);
/** @param size_t size
 *	@param size_t boundary
 *	@return void*
 */
void* memalign(size_t size, size_t boundary);
/** @param size_t size
 *	@param size_t boundary
 *	@return void*
 */
void* aligned_alloc(size_t size, size_t boundary);
/** @param size_t size
 *	@return void*
 */
void* valloc(size_t size);
struct mallinfo mallinfo();
#endif

/** @name System
 *	system malloc functions
 *	dmmalloc uses these functions instead of the standard malloc functions
 */
///@{
/** @param size_t
 *	@return void*
 */
void * sys_malloc(size_t);
/** @param void*
 *	@param size_t
 *	@return void*
 */
void * sys_realloc(void *, size_t);
/**	@param void*
 *	@return void
 */
void sys_free(void *);
/** @param size_t
 *	@param size_t
 *	@return void*
 */
void * sys_calloc(size_t, size_t);
/** @param void*
 *	@return size_t
 */
size_t sys_malloc_usable_size(void*);
/** @param void**
 *	@param size_t
 *	@param size_t
 *	@return int
 */
int sys_posix_memalign(void**, size_t, size_t);
///@}
#endif
