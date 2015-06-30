#ifndef POSTWAIT_H
#define POSTWAIT_H

#include "../bop_api.h"  // for memaddr_t
#include "../bop_map.h"  // for shm_map_t

typedef struct _pw_channel_t {
  memaddr_t chid;
  int sender, earliest_receiver;
  char is_posted;
  shm_map_t *collection;  /* made nil for inactive channels */
} pw_channel_t;


/* Local methods implemented in local.c */
void alloc_pw_meta_data( void );
void clear_local_pw_meta_data( void );

/* Operations for bop library.  Not for the parallelized code. */
void check_postwait( memaddr_t addr, size_t size );
map_t *combine_pw_sent_recv( void );  /* changing pw_sent into union */

void create_switch_board( void );
void init_switch_board_pre_spec_group( void );

pw_channel_t *sb_wait_channel( memaddr_t chid, int requester );
char sb_post_channel( memaddr_t chid, map_t *ch_data, int sender );

void sender_conflict_check( memaddr_t write_addr );

/* Library methods in exp_board.c */
void create_exp_board( void );
void clear_local_exp_meta_data( void );
void init_exp_board_pre_spec_group( void );
void exp_board_inspect( int verbose, unsigned task_id );

/* Library methods for bop_ordered (in ordered.c ) */
void BOP_ordered_begin( void );
void BOP_ordered_end( void );
void register_ordered_write( memaddr_t base, size_t sz );
extern map_t *ordered_writes;  /* for faster checking */
#endif
