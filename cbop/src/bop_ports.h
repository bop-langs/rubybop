#ifndef _BOP_PORTS_H_
#define _BOP_PORTS_H_

#include "bop_api.h"

typedef struct {
  void (*ppr_group_init)( void );
  void (*ppr_task_init)( void );
  int (*ppr_check_correctness)( void );
  void (*report_conflict)( addr_t );
  void (*data_commit)( void );
  void (*task_group_commit)( void );
  void (*task_group_succ_fini)( void );
  void (*undy_init)( void );
  void (*undy_succ_fini)( void );
} bop_port_t;

void register_port( bop_port_t *port, char * desc );

/* common vars needed in a port */

extern volatile task_status_t task_status;
extern volatile ppr_pos_t ppr_pos;
extern bop_mode_t bop_mode;
extern int spec_order;
// extern int ppr_index; *Don't* use it directly, call BOP_ppr_index( ) instead

#define task_parallel_p (ppr_pos == PPR && (task_status == MAIN || task_status == SPEC))

// should be defined in the files that include bop_ports.h: extern mspace metacow_space;  
#define meta_malloc(type) (type *)mspace_malloc(metacow_space, sizeof(type))
#define meta_calloc(num, type) (type*)mspace_calloc(metacow_space, (num), sizeof(type))

#endif
