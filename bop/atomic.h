#ifndef ATOMIC_H
#define ATOMIC_H
#include "utils.h"
/* for GCC internals see
   http://gcc.gnu.org/onlinedocs/gcc-4.4.1/gcc/Atomic-Builtins.html */

/* We use only 0-1 values with the lock */

#ifdef USE_PTHREAD_LOCK
#include<pthread.h>
typedef pthread_mutex_t bop_lock_t;
#else
typedef char bop_lock_t;
#endif

/* nop, backoff, and slowpath locking code based on RSTM
http://www.cs.rochester.edu/~sandhya/csc258/assignments/ass2/atomic_ops.h*/

static inline void backoff( int *b ) {
  int i;
    for (i = *b; i; i--)
        nop(); //from utils.h

    if (*b < 4096)
        *b <<= 1;
}

static inline void lock_acquire_slowpath(char *lock)
{
    int b = 64;

    do
    {
        backoff(&b);
    }
    while (__sync_bool_compare_and_swap( (lock), 0, 1 ) == 0);
}

#ifdef USE_PTHREAD_LOCK
static inline void bop_lock_acquire( bop_lock_t *lock ) {
  pthread_mutex_lock (lock);
}
static inline void bop_lock_release( bop_lock_t *lock ) {
  pthread_mutex_unlock (lock);
}
#else
static inline void bop_lock_acquire( bop_lock_t *lock ) {
  if ( __sync_bool_compare_and_swap( (lock), 0, 1 ) == 0 ) lock_acquire_slowpath( lock );
}
#define bop_lock_release( lock ) *(lock) = 0

static inline void bop_wait_flag( bop_lock_t *flag ) {
  int b = 64;
  while ( ! *flag )
    backoff( &b );
}

#endif

#define bop_lock_clear( lock ) bop_lock_release( (lock) )

#endif
