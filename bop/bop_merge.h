#ifndef __BOP_MERGE_H
#define __BOP_MERGE_H

#include "bop_map.h"

void create_patch( map_t *patch, map_t *change_set, mspace space );
void clear_patch( map_t *patch );

#endif /* __BOP_MERGE_H */
