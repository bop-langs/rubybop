/** @file dmmalloc.h
 *	@brief Header file for our implementation of malloc (dmmalloc)
 *
 *	This contains the macros, malloc/bop functions, and wordsize alignment
 *
 *	@author Rubybop
 */

#ifndef DM_MALLOC_H
#define DM_MALLOC_H

//! Used with Debug flags
#define DM_DEBUG
#include <stddef.h>
#include <stdbool.h>

/** dm structs, unions etc.
 *  NOTE: the two nexts must be the same address for some utility functions in dmmalloc.c
 */
typedef union {
	/** allocated lists */
    struct {
        union header * next;    /**< ppr-allocated object list */
        size_t blocksize;       /**< which free list to insert freed items into */
    } allocated;
    /** doubly linked free list for partitioning */
    struct {
        union header * next;
        union header * prev;
    } free;
} header;

/** @brief Divide and merge malloc. Supports safe PPR forks
 *  @param size_t Block size
 *  @return void
 */
void * dm_malloc(size_t);
/** @brief Re-allocates memory using malloc and free
 *          Use system realloc with large->large sizes in SEQUENTIAL() mode
 *  @param void* Void pointer
 *  @param size_t Block size
 *  @return void
 */
void * dm_realloc(void *, size_t);
/** @brief Free a block if any of the following are true:
 *  1) Any sized block running in SEQ mode
 *  2) Small block allocated and freed by this PPR task
 *  A free is queued to be free'd at BOP commit time otherwise
 *  @param void* Void pointer
 *  @return void
 */
void dm_free(void *);
/** @brief Different PPR memory allocation method that uses Doug Lea's
 *          memory allocator (dlmalloc). Bop safe
 *  @param size_t Block size
 *  @param size_t Block size
 *  @return void
 */
void * dm_calloc(size_t, size_t);
/** @brief Prints debug info
 *  @param void
 *  @return void
 */
void dm_print_info(void);
/** @brief Looks for free block size
 *  @param void* Void printer
 *  @return size_t Block size
 */
size_t dm_malloc_usable_size(void*);

/** @name BopFunctions
 *      bop-related functions
 */
///@{
/** @brief Dive up available memory
 *  @return void
 */
void carve();
/** @brief Set end pointers for this ppr task
 *  @return void
 */
void initialize_group();
///@}

/** @name DataAccess
 *  @brief data accessors for merge time
 */
///@{
void malloc_merge(void);
/** @brief Counts get updated AFTER abort status is known
 *  @return void
 */
void malloc_merge_counts(bool);
///@}

//! alignment based on word size. depends on 32 or 64 bit
#if __WORDSIZE == 64
#define ALIGNMENT 8
#elif __WORDSIZE == 32
#define ALIGNMENT 4
#else
#error "need 32 or 64 bit word size"
#endif

//! malloc config macros
#ifndef DM_BLOCK_SIZE
#define DM_BLOCK_SIZE 200
#endif

//! alignment & header macros
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HSIZE (ALIGN((sizeof(header))))
#define HEADER(vp) ((header *) (((char *) (vp)) - HSIZE))
#define CAST_UH(h) ((union header *) (h))
#define CAST_H(h) ((header*) (h))
#define CHARP(p) (((char*) (p)))
#define PAYLOAD(hp) ((header *) (((char *) (hp)) + HSIZE))
#define PTR_MATH(ptr, d) ((CHARP(ptr)) + d)
#define ASSERTBLK(head) bop_assert ((head)->allocated.blocksize > 0);

//! class size macros
#define DM_NUM_CLASSES 16
#define DM_CLASS_OFFSET 4 /**< how much extra to shift the bits for size class, ie class k is 2 ^ (k + DM_CLASS_OFFSET) */
#define MAX_SIZE SIZE_C(DM_NUM_CLASSES)
#define SIZE_C(k) (ALIGN((1 << (k + DM_CLASS_OFFSET))))	/**< allows for iterative spliting */
#define DM_MAX_REQ (ALIGN((MAX_SIZE) - (HSIZE)))


#endif
