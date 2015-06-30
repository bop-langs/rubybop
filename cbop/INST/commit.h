#ifndef _BOP_COMMIT_H_
#define _BOP_COMMIT_H_

#include "bop_map.h"

void create_group_meta( void );  // initial allocation
void group_meta_data_init(int group_max);  // called once before a group
void group_meta_data_free( void );

int check_correctness(int myGroupID, map_t *read_map, map_t *write_map);
void copyout_write_map(int myGroupID, map_t *write_map, map_t *pw_pages);
void copyout_written_pages(int myGroupID);
void copyin_update_pages(int sourceGroupID);

/* for early termination */
int is_last_member(int memberID);
void set_last_member(int memberID);

/* for inter-task pipe-based signaling: wait calls return the
   ppr_index of the posting task */
void post_undy_created( );    /* send the pid */
int wait_undy_created(void);  /* return the pid of the sender */

void post_undy_conceded(int ppr_id);
int wait_undy_conceded(void);

/* left to right signaling */
void post_write_map_done(int member_id, int ppr_id);
int wait_write_map_done(int member_id);
void post_data_done(int member_id, int ppr_id);
int wait_data_done(int member_id);

/* right to left signaling */
void post_check_done(int member_id, int ppr_id);
int wait_check_done(int member_id);

/* hard abort flag */
int get_hard_abort_flag( void );
void set_hard_abort_flag( void );
void clear_hard_abort_flag( void );

/* utilities for debugging */
void inspect_commit_data_store( int );

#endif
