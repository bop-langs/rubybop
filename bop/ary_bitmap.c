#include <string.h>
#include <external/malloc.h>
#include <bop_api.h>
#include <ary_bitmap.h>
#include <stdlib.h>

/* each entry an unsigned number, 32 bits */
uint32_t ary_map_entries( uint32_t ary_size ) {
  return (ary_size >> 5) + ((ary_size & 0x1f)? 1: 0);
}

void *ary_malloc_with_map( uint32_t length, uint32_t elem_size ) {
  void *ary = malloc( length*(elem_size) + ary_map_entries(length) * sizeof(uint32_t) * 2 );
  ary_maps_reset( ary, length, elem_size );
  return ary;
}

void *ary_realloc_with_map( void *mem, uint32_t new_len, uint32_t elem_size ) {
  void *ary = realloc( mem, new_len*elem_size + ary_map_entries(new_len) * sizeof(uint32_t) * 2 );
  ary_maps_reset( ary, new_len, elem_size );
  return ary;
}

uint32_t *get_ary_use_map( void *base, uint32_t length, uint32_t elem_size ) {
  return (uint32_t *) (((char*)base) + elem_size * length);
}

uint32_t *get_ary_mod_map( void *base, uint32_t length, uint32_t elem_size ) {
  return (uint32_t *) (((char*)base) + elem_size * length + ary_map_entries(length) * sizeof(uint32_t));
}

void ary_maps_reset( void *base, uint32_t length,  uint32_t elem_size ) {
  char * maps_base = (char*) get_ary_use_map( base, length, elem_size );
  memset( maps_base, 0, ary_map_entries( length ) * 2 * sizeof( uint32_t ) );
  BOP_promise( maps_base, ary_map_entries( length ) * 2 * sizeof( uint32_t ) );
}

static void mark_bit( uint32_t *map, uint32_t idx ) {
  uint32_t word_idx = idx >> 5;
  uint32_t offset = idx & 0x1f;
  map[ word_idx ] |= 0x1 << offset;
}

void ary_use_elem( void *base, uint32_t length,
		   uint32_t elem_size, uint32_t idx ) {
  uint32_t *use_map = get_ary_use_map( base, length, elem_size );
  mark_bit( use_map, idx );
}

void ary_promise_elem( void *base, uint32_t length,
		       uint32_t elem_size, uint32_t idx ) {
  uint32_t *mod_map = get_ary_mod_map( base, length, elem_size );
  mark_bit( mod_map, idx );
}

static void scan_one_map( void *base, uint32_t length, uint32_t elem_size,
			  uint32_t *map, monitor_t *bop_mon ) {
    uint32_t num = ary_map_entries( length );
    int i, j;
    for ( i = 0; i < num; i++ ) {
      uint32_t word = map[ i ];
      uint32_t idx = i << 5;
      for ( j = 0; j < 32; j++ )
	if ( (word >> j) & 0x1 )
	  (*bop_mon)( ((char*) base) + (idx + j) * elem_size, elem_size );
    }
}

void scan_ary_maps( void *base, uint32_t length, uint32_t elem_size ) {
  scan_one_map( base, length, elem_size,
		get_ary_use_map(base, length, elem_size), & BOP_record_read );
  scan_one_map( base, length, elem_size,
		get_ary_mod_map(base, length, elem_size), & BOP_record_write );
}
