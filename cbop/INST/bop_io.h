#ifndef _BOP_IO_H_
#define _BOP_IO_H_

/* for use at program start */
void bop_io_init( );  

/* each time a new speculation group is spawned */
void bop_io_group_init( int group_size );
void bop_io_group_close( int group_size ) ;
/* IO routines are declared in bop_api.h and bop.h */

#endif
