#include "../bop_api.h"  // for mem_range_t, BOP_Abort
#include "../bop_map.h"
#include "postwait.h"

map_t *ordered_writes = NULL;

extern map_t *pw_sent;

const memaddr_t base_cid = 123456;   /* an unusual offset to add to the ppr_index to create a hopefully non-concflicting channel id */

void register_ordered_write( memaddr_t base, size_t sz ) {
  if ( ordered_writes != NULL )
    map_add_range( ordered_writes, base, sz, ppr_index, NULL );
}

void BOP_ordered_begin( void ) {
  if ( myStatus != MAIN && myStatus != SPEC ) return;

  if ( ordered_writes != NULL ) {
    bop_msg( 2, "BOP_pre_ordered warning: a previous ordered section is likely not finished properly");
    map_clear( ordered_writes );
  }
  else 
    ordered_writes = new_merge_map( );

  if ( myStatus == SPEC )
    BOP_wait( base_cid + ppr_index - 1 );

}

void BOP_ordered_end( void ) {
  if ( myStatus != MAIN && myStatus != SPEC ) return;
 
  assert( ordered_writes != NULL );

  char suc = sb_post_channel( base_cid + ppr_index,
			      ordered_writes,
			      ppr_index );


  if ( suc ) {
    map_inspect( 1, ordered_writes, "posted writes in bop_ordered region");
    map_union( pw_sent, ordered_writes );
  }
  else 
    bop_msg( 1, "BOP warning: posting changes in the ordered section failed");

  map_free( ordered_writes );
  ordered_writes = NULL;
}
