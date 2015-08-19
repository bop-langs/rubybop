/** @file bop_ppr_sync.h
 *	@brief Header file for BOP sync
 *
 *	Contains methods to initialize ppr groups/tasks and handle signals and waiting
 *	Initializes and finds out if the understudy successfully finishes
 *	
 *	@author Rubybop
 */

#ifndef __BOP_PPR_SYNC_H
#define __BOP_PPR_SYNC_H

/** @param void
 *	@return void
 */
void ppr_sync_data_reset(void);

/** @param void
 *	@return int
 */
int partial_group_get_size(void);
/** @param int psize
 *	@return void
 */
void partial_group_set_size(int psize);

/** @param int passed
 *	@return void
 */
void signal_check_done(int passed);
/** @param void
 *	@return void
 */
void signal_commit_done(void);
/** @param void
 *	@return void
 */
void signal_undy_conceded(void);
/** @param int pid
 *	@return void
 */
void signal_undy_created(int pid);

/** @param void
 *	@return void
 */
void wait_prior_check_done(void);
/** @param void
 *	@return void
 */
void wait_group_commit_done(void);
/** @param void
 *	@return void
 */
void wait_next_commit_done(void);
/** @param void
 *	@return void
 */
void wait_undy_conceded(void);
/** @param void
 *	@return void
 */
void wait_undy_created(void);

/** @name Ports
 *	Methods called from bop_ports
 */
///@{
/** @param void
 *	@return void
 */
void ppr_group_init(void);
/** @param void
 *	@return void
 */
void ppr_task_init(void);
/** @param void
 *	@return void
 */
void undy_succ_fini(void);
/** @param void
 *	@return void
 */
void ppr_group_init(void);
/** @param void
 *	@return void
 */
void ppr_task_init(void);
/** @param void
 *	@return int
 */
int  ppr_check_correctness(void);
/* void report_conflict( void ); Name conflicts with something in utils.h */
/** @param void
 *	@return void
 */
void data_commit(void);
/** @param void
 *	@return void
 */
void task_group_commit(void);
/** @param void
 *	@return void
 */
void task_group_succ_fini(void);
/** @param void
 *	@return void
 */
void undy_init(void);
/** @param void
 *	@return void
 */
void undy_succ_fini(void);
///@}

#endif /* __BOP_PPR_SYNC_H */
