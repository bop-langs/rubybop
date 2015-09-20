/** @file bop_merge.h
 *	@author Rubybop
 */
#ifndef __BOP_MERGE_H
#define __BOP_MERGE_H

#include "bop_map.h"

/** @param map_t *patch
 *	@param map_t *change_set
 *	@param mspace space
 *	@return void
 */
void create_patch( map_t *patch, map_t *change_set, mspace space );
/** @param map_t *patch
 *	@return void
 */
void clear_patch( map_t *patch );

#endif /* __BOP_MERGE_H */
