//TODO see if this is needed
//THIS FILE CURRENTLY DOES NOTHING!!!!!!!!!


#include <internal.h>
#include <ppr.h>
#include "../bop/bop_api.h"
#include "../bop/bop_ports.h"

st_table *ppr_pot = NULL;

// Scan function prototype
//  void (*scan)( void *, void (*bop_monitor)( memaddr_t, size_t ) );

static void ppr_pot_init( void ) {
  if ( spec_order <= 1 ) {  // MAIN and first SPEC
    assert(ppr_pot == NULL);
    ppr_pot = st_init_numtable_with_size( 100 );
  }
  else
    assert( ppr_pot->num_entries == 0 );
}

static void pot_removal( void ) {
  st_free_table( ppr_pot );
  ppr_pot = NULL;
}

static void BOP_both_use_promise( void* addr, size_t size ) {
  BOP_use( addr, size );
  BOP_promise( addr, size );
}

monitor_t *get_monitor_func( unsigned long bop_flags )	{
  if ( (bop_flags & (BF_NEW|BF_USE|BF_MOD|BF_SUBOBJ|BF_META)) == 0 )
    return NULL;
  if ( (bop_flags & BF_META) || (bop_flags & BF_SUBOBJ))
    return NULL;
  if ( (bop_flags & BF_USE) && (bop_flags & BF_MOD) )
    return &BOP_both_use_promise;
  if (bop_flags & BF_USE)
    return &BOP_record_read;
  if ( (bop_flags & BF_MOD) || (bop_flags & BF_NEW) )
    return &BOP_record_write;
  bop_msg( 1, "unknown bop flags %llx", bop_flags);
  assert( 0 );
}

static int upload_i(void *obj, void (*scan)(void*)) {
  (*scan)(obj);
  return ST_CONTINUE;
}

void ppr_pot_upload( void ) {
  bop_msg( 3, "ppr_pot_upload %d entries", ppr_pot->num_entries );
  st_foreach( ppr_pot, upload_i, 0 );
}

bop_port_t ruby_monitor = {
  .ppr_task_init         = ppr_pot_init,
  .undy_init             = pot_removal,
  //  .ppr_check_correctness = ppr_is_correct,  done in PPR block so it happens before other ports
  .task_group_commit     = pot_removal
};
