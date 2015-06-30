#include <stdlib.h>  /* for NULL */
#include "../bop_api.h"  /* for PAGESIZEX */
#include "../commit.h"
#include "../bop_map.h"  /* always include after bop_api.h */
#include "../data_store.h"  /* data store functions */

static void base_interface_tests(void);
static void data_copy_tests(void);

extern map_t *write_map;

int main( ) {
  BOP_init( );  /* for setting up the priv_heap */
  write_map = new_merge_map( );

  group_meta_data_init( 8 );
  base_interface_tests( );
  group_meta_data_free( );

  group_meta_data_init( 8 );
  data_copy_tests( );
  return 0;  /* must return 0 for rake test to succeed */
}

char pages[10 << PAGESIZEX];

static void data_copy_tests(void) {
  map_t *t0_write_map, *t1_write_map;

  bop_msg(1, "data_copy_tests");

  /* create the access maps */
  t0_write_map = new_merge_map( );
  t1_write_map = new_merge_map( );

  /* t0 writes to page 1.  t1 writes to page 2. */ 
  pages[0] = '0';
  map_add_range( t0_write_map, (memaddr_t) &pages[0], PAGESIZE, 0, NULL );
  pages[PAGESIZE] = '1';
  map_add_range( t1_write_map, (memaddr_t) &pages[PAGESIZE], PAGESIZE, 1, NULL );

  /* t1 checks for correctness */
  copyout_write_map( 0, t0_write_map, NULL);
  post_write_map_done( 0, 0 );
  wait_write_map_done( 0 );
  int passed = check_correctness( 1, t1_write_map, t1_write_map );
  /* assert(passed == 1); */
  copyout_write_map( 1, t1_write_map, NULL);
  post_write_map_done( 1, 1 );

  inspect_commit_data_store( 1 ); 

  /* t0 and t1 copies their changed pages */
  copyout_written_pages( 0 );
  copyout_written_pages( 1 );
  post_data_done( 0, 0 );
  wait_data_done( 0 );
  post_data_done( 1, 1 );
  
  /*  t2 copies in both pages. */
  pages[0] = '2';
  pages[PAGESIZE] = '2';
  copyin_update_pages( 0 );
  copyin_update_pages( 1 );
  assert(pages[0] == '0');
  assert(pages[PAGESIZE] == '1');
}

static void base_interface_tests(void) {
  map_t *t0_write_map, *t1_read_map, *t1_write_map, *t2_read_map;

  /* create the access maps */
  t0_write_map = new_merge_map( );
  t1_read_map = new_merge_map( );
  t1_write_map = new_merge_map( );
  t2_read_map = new_merge_map( );

  /* task 0 writes pages [1,2] and [10, 12] */
  map_add_range( t0_write_map, 1 << PAGESIZEX, 2 * PAGESIZE, 0, NULL );
  map_add_range( t0_write_map, 10 << PAGESIZEX, 3 * PAGESIZE, 0, NULL );
  
  /* task 1 reads pages [5,6] */
  map_add_range( t1_read_map, 5 << PAGESIZEX, 2 * PAGESIZE, 1, NULL );
  
  /* task 1 writes [5,6] and [11, 11] */
  map_add_range( t1_write_map, 5 << PAGESIZEX, 2 * PAGESIZE, 1, NULL );
  map_add_range( t1_write_map, 11 << PAGESIZEX, PAGESIZE, 1, NULL );

  map_inspect( 1, t0_write_map, "t0_write_map" );
  map_inspect( 1, t1_write_map, "t1_write_map" );
  map_inspect( 1, t1_read_map, "t1_read_map" );

  copyout_write_map( 0, t0_write_map, NULL);
  
  int passed = check_correctness( 1, t1_read_map, t1_read_map);
  printf("passed is %d\n", passed);
  assert(passed == 1);

  passed = check_correctness( 1, t1_read_map, t1_write_map);
  printf("passed is %d\n", passed);
  assert(passed == 0);

  copyout_write_map( 1, t1_write_map, NULL);

  /* task 2 reads/writes [3, 3] */
  map_add_range( t2_read_map, 3 << PAGESIZEX, PAGESIZE, 2, NULL );
  
  passed = check_correctness( 2, t2_read_map, t2_read_map );
  assert(passed == 1);

  post_undy_created( );
  wait_undy_created();

  post_undy_conceded(11);
  wait_undy_conceded();
  
  post_write_map_done(2, 12);
  wait_write_map_done(2);

  post_data_done(3, 13);
  wait_data_done(3);

  post_check_done(2, 22);
  wait_check_done(2);
  
}
