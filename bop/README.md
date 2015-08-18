
PPR Task Memory Allocators
==========================

# Files

* dmmalloc.* -> Divide and Merge Malloc. This is the default and most tested malloc.
* comalloc.* -> Co-malloc. An alternative alpha malloc implementation which is bop safe.
* malloc_wrapper.* -> Provides malloc family of functions. Calling malloc yields malloc_wrapper:malloc -> dmmalloc:dm_malloc.

The wrapper also provides references to the system (eg libc or dlmalloc) malloc functions, which is how dmmalloc gets more memory.

# Divide & Merge Malloc

A dual-stage malloc implementation to support safe PPR forks
Each stage (sequential/no PPR tasks running) and a PPR tasks’ design is the same, a basic size-class allocator. The complications come when PPR_begin is called:
Allocating
A PPR task is given part of the parent’s free lists to use for its memory. This ensures that there will be no ‘extra’ conflicts at commit time.
If there is not enough memory, the parent gets new memory from the system and then gives it to the PPR task (GROUP_INIT)
If a PPR task runs out of memory, it must abort speculation. Calls to the underlying malloc are not guaranteed to not conflict with other.
The under study maintains access to the entire free list. Since either the understudy or the PPR tasks will survive past the commit stage, this is still safe.
At commit time, the free lists of PPR tasks are merged along with the standard BOP changes. This allows memory not used by PPR tasks to be reclaimed and used later.
Freeing
when a PPR task frees something from the global heap (something it did not allocate, eg it was either allocated by a prior PPR task or before and PPRs were started) it marks as freed and moves to a new list. This list is parsed at commit time and is always accepted. We cannot immediately move it into the free list since when allocating a new object of that size. If multiple PPR tasks do this (which is correct in sequential execution) and both allocate the new object, the merge will fail.

DM Malloc large objects:

Size classes need to be finite, so there will be some sizes not handled by this method, the work around is:
allocation: if in PPR task, abort if not use DL malloc.

free: when one of these is freed, check the block size. if it’s too large for any size class it was allocated with dl malloc. use dl free OR if sufficiently large divide up for use in our size classes.


# Comalloc

This approach to PPR Memory allocation uses Doug Lea's memory allocator (dlmalloc) to allocate private heaps that are then managed by the separate allocator, comalloc. The allocator identifies the allocations done by itself and dlmalloc by a special bit chosen by the programmer. When not executing a PPR task, the allocator simple forwards allocation calls to dlmalloc.


# Key-Value monitoring
Some initial bookkeeping code is included in key_value_checks.* files. The goal of this is to have special monitoring that allows memory to be moved around between speculative tasks and to allow higher-level things like growing an array in Ruby (<< operator) to be marked more explicitly. The old pure-memory address monitoring is mostly correct except in the case of growing an array.
The correctness here is slightly different. Each operation stores a R/W with the key-value pairs. These are stored in ```key_val_entry``` structs. Correctness is still, of course, based on the R/W sets for each speculative task and are now further broken down by object
Intended use:
An existing struct representing a key-value data structure (eg arrays, hashtables) will add a reference to a ```key_val_object``` structure which will need to be initialized at group init. The ```key_value_object``` contains an mspace (from dmalloc) for its memory uses. These are shared between processes with locking etc from dlmalloc (external/malloc).
Instead of storing the memory address and size, the key/value instances are stored for each marking.
There are several improvements that are going to be made to this, but the corrent implementation includes error checking and function prototypes are unlikely to change significantly. Most changes will be in performance by switching to a hashtable and reducint the memory footprint.
