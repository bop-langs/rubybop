/* This is a public domain general purpose hash table package written by Peter Moore @ UCB. */

/* static	char	sccsid[] = "@(#) st.c 5.1 89/12/14 Crucible"; */

#include "config.h"
#include "defines.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <limits.h>
#include "st.h"

typedef struct st_table_entry st_table_entry;

struct st_table_entry {
    unsigned int hash;
    st_data_t key;
    st_data_t record;
    st_table_entry *next;
};

#define ST_DEFAULT_MAX_DENSITY 5
#define ST_DEFAULT_INIT_TABLE_SIZE 11

    /*
     * DEFAULT_MAX_DENSITY is the default for the largest we allow the
     * average number of items per bin before increasing the number of
     * bins
     *
     * DEFAULT_INIT_TABLE_SIZE is the default for the number of bins
     * allocated initially
     *
     */
static int numcmp(long, long);
static int numhash(long);
static struct st_hash_type type_numhash = {
    numcmp,
    numhash,
};

/* extern int strcmp(const char *, const char *); */
static int strhash(const char *);
static struct st_hash_type type_strhash = {
    strcmp,
    strhash,
};

static void rehash(st_table *);

/* can't define malloc as xmalloc when both cbop/src/external/malloc.h and defines.h are present because they have different function signatures for malloc.

#ifdef RUBY
#define malloc xmalloc
#define calloc xcalloc
#endif
*/

#include <cbop/src/bop_ports.h>
#include <cbop/src/bop_api.h>
#include <ppr.h>
#include <cbop/src/external/malloc.h>
extern mspace metacow_space;
extern char in_ordered_region;

/* begin generated. generator code in ppr.h */
#define table_clear_bf(table)  (table->bop_flags)&=~(BF_NEW|BF_USE|BF_MOD|BF_BOREC|BF_SUBOBJ|BF_META)
#define table_ppr_new_p(table) (table->bop_flags&BF_NEW)
#define table_use_p(table) (table->bop_flags&BF_USE)
#define table_promise_p(table) (table->bop_flags&BF_MOD)
#define table_meta_p(table) (table->bop_flags&BF_META)
#define table_meta(table) table->bop_flags|=BF_META
void bop_scan_table(st_table *);
/*
#define pot_add_table(table) if (!(table->bop_flags&BF_BOREC) && !(table->bop_flags&BF_META)) {st_insert(ppr_pot, (st_data_t) table, (st_data_t) &bop_scan_table); table->bop_flags|=BF_BOREC;}
#define table_ppr_new(table) table_clear_bf(table); if (task_parallel_p && !(table->bop_flags&BF_META)) {pot_add_table(table); bop_msg(5,"table_ppr_new %llx -- %s:%d",table,__FILE__,__LINE__); (table->bop_flags)|=BF_NEW;}
#define table_use_all(table) if (task_parallel_p && !(table->bop_flags&BF_META)) {pot_add_table(table); table->bop_flags&=~BF_SUBOBJ; bop_msg(5,"table_use_all %llx -- %s:%d",table,__FILE__,__LINE__); table->bop_flags|=BF_USE;}
#define table_promise_all(table) if (task_parallel_p && !(table->bop_flags&BF_META)) {pot_add_table(table); table->bop_flags&=(~BF_SUBOBJ); bop_msg(5,"table_promise_all %llx %llx -- %s:%d",table, table->bop_flags, __FILE__,__LINE__); table->bop_flags|=BF_MOD;}
#define table_clobber_all(table) table_use_all(table); table_promise_all(table)
#define table_use_entry(table,base,len) if (task_parallel_p && !(table->bop_flags&BF_META)) if (table->bop_flags|BF_SUBOBJ) {BOP_use(table, sizeof(st_table)); BOP_use(base,len); bop_msg(5,"table_use_entry %llx, %llx, %lld bytes -- %s:%d",table, base, len, __FILE__,__LINE__);} else table_use_all(table)
#define table_promise_entry(table,base,len) if (task_parallel_p && !(table->bop_flags&BF_META)) if (table->bop_flags|BF_SUBOBJ) {BOP_promise(table, sizeof(st_table)); BOP_promise(base,len); bop_msg(5,"table_promise_entry %llx, %llx, %lld bytes -- %s:%d",table, base, len, __FILE__,__LINE__);} else table_promise_all(table)
*/
#define pot_add_table(table) {if (!(table->bop_flags&BF_BOREC) && table!=ppr_pot) {st_insert(ppr_pot, (st_data_t) table, (st_data_t) &bop_scan_table); table->bop_flags|=BF_BOREC;}}
#define table_ppr_new(table) {table_clear_bf(table); if (task_parallel_p && table!=ppr_pot) {pot_add_table(table); bop_msg(5,"table_ppr_new %llx -- %s:%d",table,__FILE__,__LINE__); (table->bop_flags)|=BF_NEW; if (in_ordered_region) bop_scan_table(table);}}
#define table_use_all(table) {if (task_parallel_p && table!=ppr_pot) {pot_add_table(table); table->bop_flags&=~BF_SUBOBJ; bop_msg(5,"table_use_all %llx -- %s:%d",table,__FILE__,__LINE__); table->bop_flags|=BF_USE; if (in_ordered_region) bop_scan_table(table);}}
#define table_promise_all(table) {if (task_parallel_p && table!=ppr_pot) {pot_add_table(table); table->bop_flags&=(~BF_SUBOBJ); bop_msg(5,"table_promise_all %llx %llx -- %s:%d",table, table->bop_flags, __FILE__,__LINE__); table->bop_flags|=BF_MOD; if (in_ordered_region) bop_scan_table(table);}}
#define table_clobber_all(table) {table_use_all(table); table_promise_all(table);}
#define table_use_entry(table,base,len) {if (task_parallel_p && table!=ppr_pot) if (table->bop_flags|BF_SUBOBJ) {BOP_use(table, sizeof(st_table)); BOP_use(base,len); bop_msg(5,"table_use_entry %llx, %llx, %lld bytes -- %s:%d",table, base, len, __FILE__,__LINE__);} else table_use_all(table);}
#define table_promise_entry(table,base,len) {if (task_parallel_p && table!=ppr_pot) if (table->bop_flags|BF_SUBOBJ) {BOP_promise(table, sizeof(st_table)); BOP_promise(base,len); bop_msg(5,"table_promise_entry %llx, %llx, %lld bytes -- %s:%d",table, base, len, __FILE__,__LINE__);} else table_promise_all(table);}
/* end generated */


#define alloc(type)						 \
  table_meta_p(table)? meta_malloc(type):		 \
    (type*)xmalloc(sizeof(type))				 
    
#define Calloc(n,s)						 \
  (table_meta_p(table)?						 \
   (char*)mspace_calloc(metacow_space, (n), (s)):		 \
   (char*)xcalloc((n),(s)))					 

#define EQUAL(table,x,y) ((x)==(y) || (*table->type->compare)((x),(y)) == 0)

#define do_hash(key,table) (unsigned int)(*(table)->type->hash)((key))
#define do_hash_bin(key,table) (do_hash(key, table)%(table)->num_bins)

/*
 * MINSIZE is the minimum size of a dictionary.
 */

#define MINSIZE 8

/*
Table of prime numbers 2^n+a, 2<=n<=30.
*/
static long primes[] = {
	8 + 3,
	16 + 3,
	32 + 5,
	64 + 3,
	128 + 3,
	256 + 27,
	512 + 9,
	1024 + 9,
	2048 + 5,
	4096 + 3,
	8192 + 27,
	16384 + 43,
	32768 + 3,
	65536 + 45,
	131072 + 29,
	262144 + 3,
	524288 + 21,
	1048576 + 7,
	2097152 + 17,
	4194304 + 15,
	8388608 + 9,
	16777216 + 43,
	33554432 + 35,
	67108864 + 15,
	134217728 + 29,
	268435456 + 3,
	536870912 + 11,
	1073741824 + 85,
	0
};

static int
new_size(size)
    int size;
{
    int i;

#if 0
    for (i=3; i<31; i++) {
	if ((1<<i) > size) return 1<<i;
    }
    return -1;
#else
    int newsize;

    for (i = 0, newsize = MINSIZE;
	 i < sizeof(primes)/sizeof(primes[0]);
	 i++, newsize <<= 1)
    {
	if (newsize > size) return primes[i];
    }
    /* Ran out of polynomials */
    return -1;			/* should raise exception */
#endif
}

#ifdef HASH_LOG
static int collision = 0;
static int init_st = 0;

static void
stat_col()
{
    FILE *f = fopen("/tmp/col", "w");
    fprintf(f, "collision: %d\n", collision);
    fclose(f);
}
#endif

static st_table*
internal_st_init_table_with_size(is_meta, type, size)
    char is_meta;
    struct st_hash_type *type;
    int size;
{
    st_table *tbl;

#ifdef HASH_LOG
    if (init_st == 0) {
	init_st = 1;
	atexit(stat_col);
    }
#endif

    size = new_size(size);	/* round up to prime number */

    tbl = is_meta? 
      (st_table*) mspace_malloc(metacow_space, sizeof(st_table)):
      (st_table*) malloc(sizeof(st_table));

    if ( is_meta ) {
      table_clear_bf(tbl); 
      table_meta( tbl );
    }
    else
      table_ppr_new( tbl );

    tbl->type = type;
    tbl->num_entries = 0;
    tbl->num_bins = size;
    st_table *table = tbl;
    tbl->bins = (st_table_entry **)Calloc(size, sizeof(st_table_entry*));

    return tbl;
}

st_table*
st_init_table_with_size(type, size)
    struct st_hash_type *type;
    int size;
{
  return internal_st_init_table_with_size(0, type, size);
}

st_table*
st_init_table(type)
    struct st_hash_type *type;
{
    return st_init_table_with_size(type, 0);
}

st_table*
st_init_numtable(void)
{
    return st_init_table(&type_numhash);
}

st_table*
st_init_numtable_with_size(size)
    int size;
{
    return st_init_table_with_size(&type_numhash, size);
}

st_table*
meta_st_init_numtable_with_size(size)
    int size;
{
  return internal_st_init_table_with_size(1, &type_numhash, size);
}

st_table*
st_init_strtable(void)
{
    return st_init_table(&type_strhash);
}

st_table*
st_init_strtable_with_size(size)
    int size;
{
    return st_init_table_with_size(&type_strhash, size);
}

void
st_free_table(table)
    st_table *table;
{
    register st_table_entry *ptr, *next;
    int i;

    if ( ppr_pot != NULL && table != ppr_pot ) {
      BOP_promise( table, sizeof( st_table ) );
      st_delete( ppr_pot, (st_data_t *) &table, NULL );
    }

    for(i = 0; i < table->num_bins; i++) {
	ptr = table->bins[i];
	while (ptr != 0) {
	    next = ptr->next;
	    free(ptr);
	    ptr = next;
	}
    }
    free(table->bins);
    free(table);
}

#define PTR_NOT_EQUAL(table, ptr, hash_val, key) \
((ptr) != 0 && (ptr->hash != (hash_val) || !EQUAL((table), (key), (ptr)->key)))

#ifdef HASH_LOG
#define COLLISION collision++
#else
#define COLLISION
#endif

#define FIND_ENTRY(table, ptr, hash_val, bin_pos) do {\
    bin_pos = hash_val%(table)->num_bins;\
    ptr = (table)->bins[bin_pos];\
    if (PTR_NOT_EQUAL(table, ptr, hash_val, key)) {\
	COLLISION;\
	while (PTR_NOT_EQUAL(table, ptr->next, hash_val, key)) {\
	    ptr = ptr->next;\
	}\
	ptr = ptr->next;\
    }\
} while (0)

int
st_lookup(table, key, value)
    st_table *table;
    register st_data_t key;
    st_data_t *value;
{
    unsigned int hash_val, bin_pos;
    register st_table_entry *ptr;

    hash_val = do_hash(key, table);
    FIND_ENTRY(table, ptr, hash_val, bin_pos);

    if (ptr == 0) {
	return 0;
    }
    else {
	if (value != 0)  *value = ptr->record;
	table_use_entry( table, ptr, sizeof( st_table_entry ) );
	return 1;
    }
}

#define ADD_DIRECT(table, key, value, hash_val, bin_pos)\
do {\
    st_table_entry *entry;\
    if (table->num_entries/(table->num_bins) > ST_DEFAULT_MAX_DENSITY) {\
	rehash(table);\
        bin_pos = hash_val % table->num_bins;\
    }\
    \
    entry = alloc(st_table_entry);	\
    \
    entry->hash = hash_val;\
    entry->key = key;\
    entry->record = value;\
    entry->next = table->bins[bin_pos];\
    table->bins[bin_pos] = entry;\
    table->num_entries++;\
} while (0)

int
st_insert(table, key, value)
    register st_table *table;
    register st_data_t key;
    st_data_t value;
{
    unsigned int hash_val, bin_pos;
    register st_table_entry *ptr;

    table_use_all( table );

    hash_val = do_hash(key, table);
    FIND_ENTRY(table, ptr, hash_val, bin_pos);

    if (ptr == 0) {
	ADD_DIRECT(table, key, value, hash_val, bin_pos);
	table_promise_all( table );
	return 0;
    }
    else {
	ptr->record = value;
	// table_promise_all( table );
        table_promise_entry( table, ptr, sizeof( st_table_entry ) );
	return 1;
    }
}

void
st_add_direct(table, key, value)
    st_table *table;
    st_data_t key;
    st_data_t value;
{
    unsigned int hash_val, bin_pos;

    table_clobber_all( table );

    hash_val = do_hash(key, table);
    bin_pos = hash_val % table->num_bins;
    ADD_DIRECT(table, key, value, hash_val, bin_pos);
}

static void
rehash(table)
    register st_table *table;
{
    register st_table_entry *ptr, *next, **new_bins;
    int i, old_num_bins = table->num_bins, new_num_bins;
    unsigned int hash_val;

    if (table != ppr_pot) 
      BOP_use( table->bins, sizeof( st_table_entry *)*table->num_bins );
    new_num_bins = new_size(old_num_bins+1);
    new_bins = (st_table_entry**)Calloc(new_num_bins, sizeof(st_table_entry*));

    for(i = 0; i < old_num_bins; i++) {
	ptr = table->bins[i];
	while (ptr != 0) {
	    next = ptr->next;
	    hash_val = ptr->hash % new_num_bins;
	    ptr->next = new_bins[hash_val];
	    new_bins[hash_val] = ptr;
	    ptr = next;
	}
    }
    free(table->bins);
    table->num_bins = new_num_bins;
    table->bins = new_bins;
}

st_table*
st_copy(old_table)
    st_table *old_table;
{
    st_table *new_table;
    st_table_entry *ptr, *entry;
    int i, num_bins = old_table->num_bins;

    /* rbop does not use this function for its internal st tables */
    // assert( !(old_table->bop_flags|=BF_META) );

    st_table *table = old_table;
    new_table = alloc(st_table);
    if (new_table == 0) {
	return 0;
    }

    *new_table = *old_table;
    new_table->bins = (st_table_entry**)
       Calloc((unsigned)num_bins, sizeof(st_table_entry*));

    if (new_table->bins == 0) {
	free(new_table);
	return 0;
    }

    table_use_all( old_table );
    for(i = 0; i < num_bins; i++) {
	new_table->bins[i] = 0;
	ptr = old_table->bins[i];
	while (ptr != 0) {
	    entry = alloc(st_table_entry);
	    if (entry == 0) {
		free(new_table->bins);
		free(new_table);
		return 0;
	    }
	    *entry = *ptr;
	    entry->next = new_table->bins[i];
	    new_table->bins[i] = entry;
	    ptr = ptr->next;
	}
    }
    return new_table;
}

int
st_delete(table, key, value)
    register st_table *table;
    register st_data_t *key;
    st_data_t *value;
{
    unsigned int hash_val;
    st_table_entry *tmp;
    register st_table_entry *ptr;

    table_use_all( table );

    hash_val = do_hash_bin(*key, table);
    ptr = table->bins[hash_val];

    if (ptr == 0) {
	if (value != 0) *value = 0;
	return 0;
    }

    if (EQUAL(table, *key, ptr->key)) {
	table->bins[hash_val] = ptr->next;
	table->num_entries--;
	if (value != 0) *value = ptr->record;
	*key = ptr->key;
	free(ptr);
	table_promise_all( table );
	return 1;
    }

    for(; ptr->next != 0; ptr = ptr->next) {
	if (EQUAL(table, ptr->next->key, *key)) {
	    tmp = ptr->next;
	    ptr->next = ptr->next->next;
	    table->num_entries--;
	    if (value != 0) *value = tmp->record;
	    *key = tmp->key;
	    free(tmp);
	    table_promise_all( table );
	    return 1;
	}
    }

    return 0;
}

int
st_delete_safe(table, key, value, never)
    register st_table *table;
    register st_data_t *key;
    st_data_t *value;
    st_data_t never;
{
    unsigned int hash_val;
    register st_table_entry *ptr;

    table_use_all( table );

    hash_val = do_hash_bin(*key, table);
    ptr = table->bins[hash_val];

    if (ptr == 0) {
	if (value != 0) *value = 0;
	return 0;
    }

    for(; ptr != 0; ptr = ptr->next) {
	if ((ptr->key != never) && EQUAL(table, ptr->key, *key)) {
	    table->num_entries--;
	    *key = ptr->key;
	    if (value != 0) *value = ptr->record;
	    ptr->key = ptr->record = never;
	    table_promise_all( table );
	    return 1;
	}
    }

    return 0;
}

static int
delete_never(key, value, never)
    st_data_t key, value, never;
{
    if (value == never) return ST_DELETE;
    return ST_CONTINUE;
}

void
st_cleanup_safe(table, never)
    st_table *table;
    st_data_t never;
{
    int num_entries = table->num_entries;

    st_foreach(table, delete_never, never);
    table->num_entries = num_entries;
}

int
st_foreach(table, func, arg)
    st_table *table;
    int (*func)();
    st_data_t arg;
{
    st_table_entry *ptr, *last, *tmp;
    enum st_retval retval;
    int i;

    table_use_all( table );

    for(i = 0; i < table->num_bins; i++) {
	last = 0;
	for(ptr = table->bins[i]; ptr != 0;) {
	    retval = (*func)(ptr->key, ptr->record, arg);
	    table_promise_all( table );
	    switch (retval) {
	    case ST_CHECK:	/* check if hash is modified during iteration */
	        tmp = 0;
		if (i < table->num_bins) {
		    for (tmp = table->bins[i]; tmp; tmp=tmp->next) {
			if (tmp == ptr) break;
		    }
		}
		if (!tmp) {
		    /* call func with error notice */
		    return 1;
		}
		/* fall through */
	    case ST_CONTINUE:
		last = ptr;
		ptr = ptr->next;
		break;
	    case ST_STOP:
	        return 0;
	    case ST_DELETE:
		tmp = ptr;
		if (last == 0) {
		    table->bins[i] = ptr->next;
		}
		else {
		    last->next = ptr->next;
		}
		ptr = ptr->next;
		free(tmp);
		table->num_entries--;

		table_promise_all( table );
	    }
	}
    }
    return 0;
}

static unsigned long hash_seed = 0;

static int
strhash(string)
    register const char *string;
{
    register int c;

#ifdef HASH_ELFHASH
    register unsigned int h = 0, g;

    while ((c = *string++) != '\0') {
	h = ( h << 4 ) + c;
	if ( g = h & 0xF0000000 )
	    h ^= g >> 24;
	h &= ~g;
    }
    return h;
#elif defined(HASH_PERL)
    register int val = 0;

    while ((c = *string++) != '\0') {
	val += c;
	val += (val << 10);
	val ^= (val >> 6);
    }
    val += (val << 3);
    val ^= (val >> 11);

    return val + (val << 15);
#else
    register unsigned long val = hash_seed;

    while ((c = *string++) != '\0') {
	val = val*997 + c;
	val = (val << 13) | (val >> (sizeof(st_data_t) * CHAR_BIT - 13));
    }

    return val + (val>>5);
#endif
}

static int
numcmp(x, y)
    long x, y;
{
    return x != y;
}

static int
numhash(n)
    long n;
{
    return n;
}

extern unsigned long rb_genrand_int32(void);

void
Init_st(void)
{
    hash_seed = rb_genrand_int32();
}

void
bop_scan_table( table )
     st_table *table;
{
    st_table_entry *ptr, *next;
    int i;

    bop_msg(5, "bop_scan_table %llx, flags %llx", table, table->bop_flags);

    monitor_t *bop_mon =  get_monitor_func( table->bop_flags );
    table_clear_bf( table );

    for(i = 0; i < table->num_bins; i++) {
	ptr = table->bins[i];
	while (ptr != 0) {
	    next = ptr->next;
	    (*bop_mon)( ptr, sizeof( st_table_entry ) ); 
	    ptr = next;
	}
    }
    (*bop_mon)( table->bins, table->num_bins * sizeof( st_table_entry *) );
    (*bop_mon)( table, sizeof( st_table ) );
}
