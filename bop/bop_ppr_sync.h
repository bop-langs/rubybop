#ifndef __BOP_PPR_SYNC_H
#define __BOP_PPR_SYNC_H

void ppr_sync_data_reset( void );

int partial_group_get_size( void );
void partial_group_set_size( int psize );

void signal_check_done( int passed );
void signal_commit_done( void );
void signal_undy_conceded( void );
void signal_undy_created( int pid );

void wait_prior_check_done( void );
void wait_group_commit_done( void );
void wait_next_commit_done( void );
void wait_undy_conceded( void );
void wait_undy_created( void );

/* Call ports */
void ppr_group_init( void );
void ppr_task_init( void );
void undy_succ_fini( void );
void ppr_group_init( void );
void ppr_task_init( void );
int  ppr_check_correctness( void );
/* void report_conflict( void ); Name conflicts with something in utils.h */
void data_commit( void );
void task_group_commit( void );
void task_group_succ_fini( void );
void undy_init( void );
void undy_succ_fini( void );

#endif /* __BOP_PPR_SYNC_H */
