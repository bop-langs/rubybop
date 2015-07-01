#include <stdlib.h>  /* for NULL */
#include "bop_api.h"  /* for PAGESIZEX */
#include "bop_map.h"
#include "bop_ports.h"
#include "external/malloc.h"

static void two_tasks_test(void);

extern bop_port_t bop_alloc_port;
extern bop_port_t bop_merge_port;

int main( )
{
	printf("The tests begin: \n");
	two_tasks_test( );
	task_status = UNDY;
	printf("The tests end! \n");
	return 0;  /* must return 0 for rake test to succeed */
}

static void init_main(void);
static void init_undy(void);
static void init_spec(int);

static void task_commit(void);
static void group_commit(void);

static void two_tasks_test( void )
{
	BOP_set_group_size( 2 );
	char *a, *b ;
	a = NULL;
	b = NULL;

	printf("\tcase 1\n");
	printf("\t1\n");
	/* case 1 */
	/**
	      ppr1                   ppr2
	   a = malloc          b = malloc
	*/

	printf("\t2\n");
	init_main();

	printf("\t3\n");
	a = malloc(10);
	BOP_record_write( &a, sizeof( char * ) );

	printf("\t4\n");
	*a = 'c';
//	BOP_record_write( a, sizeof( char ) );
	*(a+1) = 'd';
//	BOP_record_write( a+1, sizeof( char ) );

	task_commit();

	printf("\t5\n");
	a = NULL;
	init_spec(1);
	b = malloc(20);
	BOP_record_write( &b, sizeof( char * ) );

	printf("\t6\n");
	*b = '1';
//	BOP_record_write( b, sizeof( char ) );
	*(b+1) = '2';
//	BOP_record_write( b+1, sizeof( char ) );

	task_commit();
	printf("\t7\n");

	a = NULL;
	b = NULL;
	group_commit();
	printf("\t8\n");
	printf("a : addr [%p], value %c\n", a, *a );
	printf("b : addr [%p], value %c\n", b, *b );
	printf("\t9\n");

	printf("\tcase 2\n");
	/* case 2 */
	/**
	         ppr1                   ppr2
	      a = malloc         b = malloc
	      free(a)
	      a = NULL
	*/

	printf("\t2\n");
	init_main();

	printf("\t3\n");
	a = malloc(10);
	BOP_record_write( &a, sizeof( char * ) );

	printf("\t4\n");
	*a = 'c';
	*(a+1) = 'd';
	free(a);
	a = NULL;
	BOP_record_write( &a, sizeof( char * ) );

	task_commit();

	printf("\t5\n");
	a = NULL;
	init_spec(1);
	b = malloc(20);
	BOP_record_write( &b, sizeof( char * ) );

	printf("\t6\n");
	*b = '1';
	*(b+1) = '2';

	task_commit();
	printf("\t7\n");

	a = NULL;
	b = NULL;
	group_commit();
	printf("\t8\n");
	printf("a : addr [%p] \n", a); /* should be NULL */
	printf("b : addr [%p], value %c\n", b, *b );
	printf("\t9\n");

	printf("\tcase 3\n");
	/* case 3 */
	/**
	      ppr1            ppr2
	 a = malloc
	                      free(a)
	                      a = NULL
	*/
	printf("a_addr %p, b_addr %p\n", &a, &b);

	printf("\t2\n");
	init_main();
	printf("\t3\n");
	a = malloc(10);
	BOP_record_write( &a, sizeof( char * ) );

	printf("\t4\n");
	*a = 'c';
	*(a+1) = 'd';

	task_commit();

	printf("\t5\n");
//	a = NULL;
	init_spec(1);
//	b = malloc(20);
//	BOP_record_write( &b, sizeof( char * ) );

	printf("\t6\n");
//	*b = '1';
//	*(b+1) = '2';
	free(a);
	a = NULL;
	BOP_record_write( &a, sizeof( char * ) );

	task_commit();
	printf("\t7\n");

	a = NULL;
	b = NULL;
	group_commit();
	printf("\t8\n");
	printf("a : addr [%p]\n", a);
//	printf("b : addr [%p], value %c\n", b, *b );
	printf("\t9\n");

	printf("\tcase 4\n");
	/* case 4 */
	/**
	      ppr1                         ppr2
	   a = malloc               b = malloc
	   ------------------------------------------ commit
	      ppr3                         ppr4                      ppr5
	   c = malloc
	                                free(b)               free(a)
	   ------------------------------------------ commit
	*/

	printf("a_addr %p, b_addr %p\n", &a, &b);

	printf("\t2\n");
	init_main();

	printf("\t3\n");
	a = malloc(10);
	BOP_record_write( &a, sizeof( char * ) );

	printf("\t4\n");
	*a = 'c';
	*(a+1) = 'd';

	task_commit();

	printf("\t5\n");
	a = NULL;
	init_spec(1);

	b = malloc(20);
	BOP_record_write( &b, sizeof( char * ) );

	printf("\t6\n");
	*b = '1';
	*(b+1) = '2';

	task_commit();
	printf("\t7\n");

	a = NULL;
	b = NULL;
	group_commit();
	printf("\t8\n");
	printf("a : addr [%p], value %c\n", a, *a);
	printf("b : addr [%p], value %c\n", b, *b);
	printf("\t9\n");

	char * tmp_a = a;
	char * tmp_b = b;

	//----------------------------- commit once
	task_status = SEQ;
	BOP_set_group_size( 3 );

	ppr_pos = PPR;
	char *c, *d;
	c = NULL;
	d = NULL;

	printf("\t10\n");
	init_main();

	printf("\t11\n");
	c = malloc(30);
	BOP_record_write( &c, sizeof( char * ) );

	printf("\t12\n");
	*c = 'm';
	*(c+1) = 'n';

	task_commit();

	printf("\t13\n");
	c = NULL;
	init_spec(1);

	free(b);
	b = NULL;
	BOP_record_write( &b, sizeof( char * ) );

	task_commit();

	printf("\t14\n");

	b = tmp_b;
	c = NULL;
	init_spec(2);

	free(a);
	a = NULL;
	BOP_record_write( &a, sizeof( char * ) );

	task_commit();

	printf("\t15\n");

	a = tmp_a ;
	b = tmp_b ;
	c = NULL;
	d = NULL;
	group_commit();
	printf("\t16\n");
	printf("a : addr [%p]\n", a);
	printf("b : addr [%p]\n", b);
	printf("c : addr [%p], value %c\n", c, *c);
	printf("\t17\n");
}

extern int ppr_index;

static void task_init( task_status_t status, int order ) {
	task_status = status;
	spec_order  = order;
	ppr_pos = PPR;
	ppr_index++;
	bop_alloc_port.ppr_task_init();
	bop_merge_port.ppr_task_init();
}

static void init_main( void ) {
	bop_alloc_port.ppr_group_init();
	bop_merge_port.ppr_group_init();
	task_init(MAIN, 0);
}

static void init_undy( void ) {
	task_init(UNDY, -1);
}

static void init_spec( int order )  {
	task_init(SPEC, order);
}

static void task_commit( void ) {
	/* Nothing to do for alloc port, these functions undefined */
	assert( bop_merge_port.ppr_check_correctness() );
	bop_merge_port.data_commit();
	ppr_pos = GAP;
}

static void group_commit( void ) {
	bop_alloc_port.task_group_commit();
	bop_merge_port.task_group_commit();
	task_status = SEQ;
}
