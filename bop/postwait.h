/** @file postwait.h
 *	@brief Header file for bop utility functions such as
 *	blocking/unblocking, reporting conflicts, and bop messages
 *	
 *	@author Rubybop
 */

#ifndef __POSTWAIT_H
#define __POSTWAIT_H

#include "bop_api.h"

/** @param addr_t id1
 *	@param addr_t id2
 *	@return void
 */
void channel_chain(addr_t id1, addr_t id2);
/** @param addr_t id
 *	@param addr_t base
 *	@param unsigned size
 *	@return void
 */
void channel_fill(addr_t id, addr_t base, unsigned size);
/** @param addr_t id
 *	@return void
 */
void channel_post(addr_t id);
/** @param addr_t id
 *	@return void
 */
void channel_wait(addr_t id);

#endif /* __POSTWAIT_H */
