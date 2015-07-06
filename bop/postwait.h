#ifndef __POSTWAIT_H
#define __POSTWAIT_H

#include "bop_api.h"

void channel_chain( addr_t id1, addr_t id2 );
void channel_fill( addr_t id, addr_t base, unsigned size );
void channel_post( addr_t id );
void channel_wait( addr_t id );

#endif /* __POSTWAIT_H */
