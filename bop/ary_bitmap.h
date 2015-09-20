/**	@file ary_bitmap.h
 *	@author Rubybop
 */

#ifndef _ARY_BITMAP_H_
#define _ARY_BITMAP_H_
#include <stdint.h>

/**	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return void*
 */
void *ary_malloc_with_map(uint32_t length, uint32_t elem_size);
/**	@param void* var
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return void*
 */
void *ary_realloc_with_map(void *var, uint32_t length, uint32_t elem_size);

/**	@name Unsigned
 *	@brief each entry an unsigned number, 32 bits
 */
///@{
/**	@param uint32_t ary_size
 *	@return uint32_t
 */
uint32_t ary_map_entries( uint32_t ary_size );
/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return uint32_t*
 */
uint32_t *get_ary_use_map( void *base, uint32_t length, uint32_t elem_size );
/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return uint32_t*
 */
uint32_t *get_ary_mod_map( void *base, uint32_t length, uint32_t elem_size );
///@}

/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return void
 */
void ary_maps_reset( void *base, uint32_t length,  uint32_t elem_size );
/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@param uint32_t idx
 *	@return void
 */
void ary_use_elem( void *base, uint32_t length,
		   uint32_t elem_size, uint32_t idx );
/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@param uint32_t idx
 *	@return void
 */
void ary_promise_elem( void *base, uint32_t length,
		       uint32_t elem_size, uint32_t idx );
/**	@param void *base
 *	@param uint32_t length
 *	@param uint32_t elem_size
 *	@return void
 */
void scan_ary_maps( void *base, uint32_t length, uint32_t elem_size );

#endif
