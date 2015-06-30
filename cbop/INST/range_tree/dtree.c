#include <assert.h>

/* Use ppr private malloc for meta data */
#include "../external/malloc.h"
extern mspace meta_space;

#include "../bop_api.h"  /* for bop_msg */
#include "../bop_map.h"
#include "../utils.h"

/* The code for this function is adapted from
   http://www.link.cs.cmu.edu/link/ftp-site/splaying/top-down-splay.c */
range_node_t * splay (range_node_t * t, memaddr_t key) {
    range_node_t N, *l, *r, *y;
    if (t == NULL) return t;
    N.lc = N.rc = NULL;
    l = r = &N;

    for (;;) {
	if (key < t->r.base) {
	    if (t->lc != NULL && key < t->lc->r.base) {
		y = t->lc; t->lc = y->rc; y->rc = t; t = y;
	    }
	    if (t->lc == NULL) break;
	    r->lc = t; r = t; t = t->lc;
	} else if (key > t->r.base) {
	    if (t->rc != NULL && key > t->rc->r.base) {
		y = t->rc; t->rc = y->lc; y->lc = t; t = y;
	    }
	    if (t->rc == NULL) break;
	    l->rc = t; l = t; t = t->rc;
	} else break;
    }
    l->rc=t->lc; r->lc=t->rc; t->lc=N.rc; t->rc=N.lc;
    return t;
}

char overlap(mem_range_t *b, mem_range_t *r, memaddr_t *base, size_t *size) {
  if(b->base <= r->base) {
    if(b->base+b->size > r->base) {
      if(b->base+b->size <= r->base+r->size) {
        *base = r->base;
        *size = b->base+b->size-r->base;
      } else {
        *base = r->base;
        *size = r->size;
      }
      return 1;  /* true */
    } else {
      return 0;  /* false */
    }
  } else {
    if(r->base+r->size > b->base) {
      if(r->base+r->size <= b->base+b->size) {
        *base = b->base;
        *size = r->base+r->size-b->base;
      } else {
        *base = b->base;
        *size = b->size;
      }
      return 1;  /* true */
    } else {
      return 0;  /* false */
    }
  }
}

static range_node_t * new_node( mspace msp, memaddr_t base, size_t size, int task, void *obj ) {
  range_node_t *n = mspace_calloc(msp, 1, sizeof(range_node_t));
  n->r.base = base;
  n->r.size = size;
  n->r.task_id = task;
  n->r.rec = obj;
  n->lc = n->rc = NULL;
  return n;
}

static void union_ranges( mem_range_t *r1, mem_range_t *r2, memaddr_t *ubase, size_t *usize) {
  *ubase = min( r1->base, r2->base );
  *usize = max( r1->base + r1->size, 
		r2->base + r2->size ) - *ubase;
  return;
}

static void free_subtree( range_node_t *t, map_t *map ) {
  if (t == NULL) return;
  free_subtree( t->lc, map );
  free_subtree( t->rc, map );
  t->lc = NULL;
  t->rc = NULL;
  mspace_free( map->residence, t );
  map->sz --;
}

static range_node_t *absorb_left( range_node_t *node, 
				  mem_range_t * range, map_t *map ) {
  if ( node == NULL ) return NULL;

  if ( range->base > node->r.base+node->r.size ) {
    node->rc = absorb_left( node->rc, range, map );
    return node;
  }
  else {
    memaddr_t oldbase = range->base;
    range->base = min( node->r.base, range->base );
    range->size = oldbase + range->size - range->base;
    range_node_t *ret = node->lc;

    node->lc = NULL;
    free_subtree( node, map );

    return absorb_left( ret, range, map );
  }
}

static range_node_t *absorb_right( range_node_t *node,
				   mem_range_t *range, map_t *map ) {
  if ( node == NULL ) return NULL;

  if ( range->base + range->size < node->r.base ) {
    node->lc = absorb_right( node->lc, range, map );
    return node;
  }
  else {
    memaddr_t rb = max( range->base + range->size,
			node->r.base + node->r.size );
    range->size = rb - range->base;
    range_node_t *ret = node->rc;
    
    node->rc = NULL;
    free_subtree( node, map );

    return absorb_right( ret, range, map );
  }
}

mem_range_t *map_add_key_obj( map_t *map, memaddr_t key, void *obj ) {
  assert( !map->uses_merge );
  assert( map->is_hash );
  mem_range_t *r = map_add_range( map, key, 1, -1, obj );
  return r;
}

void *map_search_key( map_t *map, memaddr_t addr ) {
  assert( map->is_hash );
  mem_range_t *k = map_contains( map, addr );
  if ( k != NULL) 
    return k->rec;
  else
    return NULL;
}

mem_range_t *map_add_range(map_t *map, memaddr_t base, size_t size, int task, void *obj) {
  size_t sz;
  assert( map != NULL );
  assert( size > 0 );

  bop_msg( 6, "map %llx adding range %llx, size %u, task id %d, obj %llx", map, base, size, task, obj );

  range_node_t * n = new_node( map->residence, base, size, task, obj );

  if(map->root == NULL) {
    map->root = n;
    map->sz = 1;
    return & map->root->r;
  }
  
  range_node_t *top = splay( map->root, base);

  /* add a range without merging */
  if ( !map->uses_merge ) {
    if ( base < top->r.base ) {
      n->lc = top->lc;
      n->rc = top;
      top->lc = NULL;
    }
    else if ( base > top->r.base ) {
      n->rc = top->rc;
      n->lc = top;
      top->rc = NULL;
    }
    else { /* range already exists */
      assert( base == top->r.base && size == top->r.size && obj == top->r.rec);
      mspace_free( map->residence, n );
      map->root = top;
      return & map->root->r;
    }

    map->sz ++;
    top = n;
    map->root = top;

    if ( size > 1 ) {
      /* check for possible overlap with a neighbor */
      range_node_t *prev = top->lc;
      while ( prev != NULL) prev = prev->rc;
      if ( prev != NULL) 
	assert( top->r.base >= prev->r.base + prev->r.size );
      range_node_t *nxt = top->rc;
      while ( nxt != NULL ) nxt = nxt->lc;
      if ( nxt != NULL)
	assert( top->r.base + top->r.size < nxt->r.base );
    }

    bop_msg( 5, "map %llx added range %llx, size %u, task id %d, objs %llx %llx", map, map->root->r.base, map->root->r.size, map->root->r.task_id, map->root->r.rec, map->root->r.rec2 );

    return & map->root->r;
  }

  /* if there is overlap, expand the root node */
  memaddr_t cbase; size_t csize;
  char has_overlap = overlap( &n->r, &top->r, &cbase, &csize);
  char bordering = 0;  /* false */
  if ( n->r.base < top->r.base )
    bordering = (n->r.base + n->r.size == top->r.base);
  else
    bordering = (top->r.base + top->r.size == n->r.base );

  if (has_overlap || bordering) {
    memaddr_t ubase; size_t usize;
    union_ranges( &n->r, &top->r, &ubase, &usize );
    top->r.base = ubase;
    top->r.size = usize;
    top->lc = absorb_left( top->lc, &top->r, map );
    top->rc = absorb_right( top->rc, &top->r, map );

    map->root = top;
    mspace_free( map->residence, n );

    bop_msg( 5, "map %llx added range %llx, size %u, task id %d, objs %llx %llx", map, map->root->r.base, map->root->r.size, map->root->r.task_id, map->root->r.rec, map->root->r.rec2 );

    return & map->root->r;
  }

  if ( base < top->r.base ) {
    n->lc = top->lc;
    n->rc = top;
    top->lc = NULL;
    map->sz ++;
    top = n;
    top->lc = absorb_left( top->lc, &top->r, map );
  }
  else if ( base > top->r.base ) {
    n->rc = top->rc;
    n->lc = top;
    top->rc = NULL;
    map->sz ++;
    top = n;
    top->rc = absorb_right( top->rc, &top->r, map );
  }
    
  map->root = top;

    bop_msg( 5, "map %llx added range %llx, size %u, task id %d, objs %llx %llx", map, map->root->r.base, map->root->r.size, map->root->r.task_id, map->root->r.rec, map->root->r.rec2 );

  return & map->root->r;
}


static memaddr_t subtree_size( range_node_t *n ) {
  if (n == NULL) return 0;
  return n->r.size + subtree_size( n->lc ) + subtree_size( n->rc );
}

memaddr_t map_size_in_bytes( map_t *map ) {
  assert( map != NULL );

  return subtree_size( map->root );
}

char overlap(mem_range_t *b, mem_range_t *r, memaddr_t *base, size_t *size);


static char subtree_overlaps( range_node_t *n, mem_range_t *inp, memaddr_t *base, size_t *size ) {
  if (n == NULL) return 0;  /* false */

  if ( n->r.base + n->r.size <= inp->base )
    return subtree_overlaps( n->rc, inp, base, size );

  if ( n->r.base > inp->base + inp->size )
    return subtree_overlaps( n->lc, inp, base, size );

  if ( n->r.base > inp->base ) {
    mem_range_t head;
    head.base = inp->base;
    head.size = n->r.base - inp->base;
    char overlap = subtree_overlaps( n->lc, &head, base, size );
    if ( overlap ) return overlap;
  }
  
  return overlap( & n->r, inp, base, size );
}

/* return the smallest overlap.  This is useful for enumerating all overlaps by repeatedly calling this function. */

char map_overlaps( map_t *map, mem_range_t *inp, mem_range_t *c_range) {
  assert( map != NULL );

  memaddr_t base; size_t size;
  char is_overlap = subtree_overlaps( map->root, inp, &base, &size );
  if (is_overlap && c_range != NULL) {
    c_range->base = base;
    c_range->size = size;
  }
  return is_overlap;
}


void map_intersect(map_t *base_map, map_t *ref_map) {
  
  if( !base_map || base_map->root == NULL ) return;

  assert( base_map->uses_merge == ref_map->uses_merge );

  mem_range_t *ranges; unsigned num, i;
  map_to_array( base_map, &ranges, &num );

  map_clear( base_map );
  if ( !ref_map || ref_map->root == NULL ) return;

  for ( i = 0; i < num; ++ i ) {
    mem_range_t range = ranges[ i ];
    mem_range_t orange;
    char overlap;
    do {
      overlap = map_overlaps( ref_map, &range, &orange );
      if (overlap) {
	mem_range_t * n = map_add_range( base_map, orange.base, orange.size, 
					 range.task_id, range.rec );
	memaddr_t old_bound = range.base + range.size;
	range.base = orange.base + orange.size;
	range.size = old_bound - range.base;
      }
    } while (overlap && range.size != 0);
  }

  mspace_free( meta_space, ranges );
  return;
}

void map_union(map_t *base_map, map_t *ref_map) {
  if(!ref_map || ref_map->root == NULL ) return;
  assert( base_map != NULL );

  if ( base_map->uses_merge && !ref_map->uses_merge )
    bop_msg( 2, "map_union warning: a no_merge map is added to a merge map.  Individual node information may be lost");

  mem_range_t *ranges; unsigned num, i;
  map_to_array( ref_map, &ranges, &num );

  for ( i = 0; i < num; ++ i ) {
    mem_range_t range = ranges[ i ];

    if ( ! base_map->uses_merge ) {
      mem_range_t c_range;
      if ( map_overlaps( base_map, &range, &c_range ) ) {
	/* No support for partial overlap yet.  In this case, range is
	   completely covered in the base_map  */
	assert( range.base == c_range.base && range.size == c_range.size );
	continue;
      }
    }
	
    mem_range_t *n = map_add_range( base_map, range.base, range.size, 
				    range.task_id, range.rec );
    n->rec2 = range.rec2;
  }

  mspace_free( meta_space, ranges );
  return;
}

void map_subtract(map_t *base_map, map_t *ref_map) {
  if(!ref_map || ref_map->root == NULL) return;
  if(!base_map || base_map->root == NULL) return;
  
  if ( base_map->uses_merge && !ref_map->uses_merge )
    bop_msg( 2, "map_union warning: a no_merge map is added to a merge map.  Individual node information may be lost");
  
  /* make copy_map be the intersection */
  map_t *copy_map = new_shm_merge_map(base_map->residence);
  map_union(copy_map, base_map);
  map_intersect(copy_map, ref_map);
  
  /* do the subtraction => base_map-copy_map */
  mem_range_t *ranges; unsigned num, i;
  map_to_array(base_map, &ranges, &num);
  
  map_clear(base_map);
  
  for (i=0;i<num;i++) {
    mem_range_t range = ranges[i];
    mem_range_t orange;
    char overlap;
    do {
      overlap = map_overlaps(copy_map, &range, &orange);
      if (overlap) {
	if (orange.base > range.base)
	  map_add_range(base_map, range.base, orange.base-range.base, range.task_id, range.rec);
	memaddr_t old_bound = range.base + range.size;
	range.base = orange.base + orange.size;
	range.size = old_bound - range.base;
      }
    } while (overlap && range.size != 0);
    if (range.size != 0)
      map_add_range(base_map, range.base, range.size, range.task_id, range.rec);
  }
  
  /* free the space */
  mspace_free(meta_space, ranges);
  map_free(copy_map);
  
  return;
}

mem_range_t *map_contains(map_t *map, memaddr_t addr) {
  assert( map != NULL );
  if ( map->root == NULL ) return NULL;

  map->root = splay(map->root, addr);
  range_node_t *top = map->root;
  if ( addr < top->r.base ) {
    if ( top->lc == NULL) return NULL;
    range_node_t *node = top->lc;
    while ( node->rc != NULL )
      node = node->rc;
    assert( node->r.base <= addr );
    if ( node->r.base + node->r.size > addr )
      return & node->r;
    else
      return NULL;
  }
  else  {
    if( top->r.base + top->r.size > addr) 
      return &map->root->r;
    else
      return NULL;
  }
}

void map_range_to_array(map_t *map, memaddr_t base, size_t size, mem_range_t **ranges,  unsigned *ranges_size) {
  assert( map != NULL );

  range_node_t *rn;
  unsigned i;
  *ranges_size = 0;
  map->root = splay(map->root, base);

  if(map->root->r.base+map->root->r.size > base) {
    while(map->root->r.base < base+size) {
      ++*ranges_size;
      rn = splay(map->root->rc, base);
      rn->lc = map->root;
      map->root->rc = NULL;
      map->root = rn;
    }
  }

  *ranges = mspace_calloc(meta_space, *ranges_size, sizeof(mem_range_t));
  map->root = splay(map->root, base);

  for(i = 0; i < *ranges_size; ++i) {
    ranges[i] = &map->root->r;
    rn = splay(map->root->rc, base);
    rn->lc = map->root;
    map->root->rc = NULL;
    map->root = rn;
  }
}

static void subtree_to_array( range_node_t *node, mem_range_t **ranges, unsigned *indx ) {
  if ( node == NULL ) return;
  subtree_to_array( node->lc, ranges, indx );
  (*ranges)[*indx] = node->r;
  *indx = *indx + 1;
  subtree_to_array( node->rc, ranges, indx );
}

void map_to_array(map_t *map,  mem_range_t **ranges, unsigned *size) {
  assert( map != NULL );

  *ranges = mspace_calloc(meta_space, map->sz, sizeof(mem_range_t));
  *size = map->sz;

  unsigned indx = 0;
  subtree_to_array( map->root, ranges, &indx );
  assert( indx == *size);
}

void map_remove_node( map_t *map, memaddr_t node_base ) {
  assert( map != NULL && map->root != NULL );
  range_node_t *rt = splay(map->root, node_base);
  assert( rt->r.base == node_base );

  range_node_t *x;
  if ( rt->lc == NULL )
    x = rt->rc;
  else {
    x = splay( rt->lc, node_base );
    x->rc = rt->rc;
  }
  map->root = x;
  map->sz --;
  rt->lc = rt->rc = NULL;  /* must do this before the free */
  mspace_free( map->residence, rt );
}

inline void map_clear(map_t *map) {
  free_subtree(map->root, map);
  map->root = NULL;
}

void map_free(map_t *map) {
  free_subtree(map->root, map);
  mspace_free( map->residence, map );
}

unsigned map_inspect(int verbose, map_t *map, char *map_nm) {
  int i;
  range_node_t *rn;
  if ( map == NULL ) {
    bop_msg( verbose, "range tree %s is not yet initialized", map_nm);
    return;
  }

  bop_msg( verbose, "%d range(s) in %s. (mspace %llx, uses_merge %d, is_hash %d)", map->sz, map_nm, map->residence, map->uses_merge, map->is_hash);

  mem_range_t *ranges; unsigned num;
  map_to_array( map, &ranges, &num );

  for(i = 0; i < num; ++i) 
    bop_msg( verbose, "\t(%llx - %llx, page %llu, size %d)", ranges[i].base, ranges[i].base + ranges[i].size, ranges[i].base >> PAGESIZEX, ranges[i].size);

  mspace_free( meta_space, ranges );
  return map->sz;
}

static void subtree_foreach( range_node_t *node, void (*func)( mem_range_t * ) ) {
  if ( node == NULL ) return;
  subtree_foreach( node->lc, func );
  (*func)( &node->r );
  subtree_foreach( node->rc, func );
}

// func must not change base/size in the range.
void map_foreach( map_t *map, void (*func)( mem_range_t * ) ) {
  if ( map == NULL ) return;
  subtree_foreach( map->root, func );
}

/* Overloading of the names. It may be confusing but avoids repetitive code. */
map_t *new_merge_map( void ) {
  return new_shm_merge_map( meta_space );
}

map_t *new_no_merge_map( void ) {
  return new_shm_no_merge_map( meta_space );
}

map_t *new_hash( void ) {
  return new_shm_hash( meta_space );
}

