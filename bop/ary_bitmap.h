#ifndef _ARY_BITMAP_H_
#define _ARY_BITMAP_H_
#include <stdint.h>

void *ary_malloc_with_map( uint32_t length, uint32_t elem_size );
void *ary_realloc_with_map( void *var, uint32_t length, uint32_t elem_size );

/* each entry an unsigned number, 32 bits */
uint32_t ary_map_entries( uint32_t ary_size );

uint32_t *get_ary_use_map( void *base, uint32_t lenth, uint32_t elem_size );

uint32_t *get_ary_mod_map( void *base, uint32_t lenth, uint32_t elem_size );

void ary_maps_reset( void *base, uint32_t lenth,  uint32_t elem_size );

void ary_use_elem( void *base, uint32_t lenth,
		   uint32_t elem_size, uint32_t idx );

void ary_promise_elem( void *base, uint32_t lenth,
		       uint32_t elem_size, uint32_t idx );

void scan_ary_maps( void *base, uint32_t length, uint32_t elem_size );

#endif
