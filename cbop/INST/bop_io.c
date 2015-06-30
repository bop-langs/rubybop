#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "bop_api.h"

static FILE* volatile ppr_std_out[MAX_GROUP_CAP+1]; //File for buffering outputs to the stdout.
static FILE* volatile ppr_std_err[MAX_GROUP_CAP+1]; //File for buffering outputs to the stderr.

void bop_io_init( ) {	
  int i;
  for ( i = 0 ; i < MAX_GROUP_CAP + 1 ; i ++ ){
    ppr_std_out[ i ] = NULL;
    ppr_std_err[ i ] = NULL;
  }
}

static void file_init( FILE ** fp ) {
  FILE *fd = tmpfile();
  *fp = fd;
  assert( *fp != NULL);
  /* tried to drain the file this way but it doesn't work
     assert( ftruncate( *fp, 0 ) == -1 ); */
}

void bop_io_group_init( int group_size ) {
  int i;
  assert( group_size <= MAX_GROUP_CAP );
  for ( i = 0 ; i < group_size ; i ++ ) {
    file_init( (FILE **) & ppr_std_out[ i ] );
    file_init( (FILE **) & ppr_std_err[ i ] );
  }
  /* for the understudy */
  file_init( (FILE **) & ppr_std_out[ MAX_GROUP_CAP ] );
  file_init( (FILE **) & ppr_std_err[ MAX_GROUP_CAP ] );
}
void bop_io_group_close( int group_size){
  int i;
  assert( group_size <= MAX_GROUP_CAP );
  for ( i = 0 ; i < group_size ; i ++ ) {
    fclose(  ppr_std_out[ i ] );
    fclose(  ppr_std_err[ i ] );
  }
  /* for the understudy */
  fclose(  ppr_std_out[ MAX_GROUP_CAP ] );
  fclose(  ppr_std_err[ MAX_GROUP_CAP ] );
}
static FILE *get_ppr_stream( FILE *out_or_err, int my_status, int spec_order) {
  assert( out_or_err == stdout || out_or_err == stderr );
  switch (my_status) {
  case SEQ:
    return out_or_err;
  case UNDY:
    if ( out_or_err == stdout ) 
      return ppr_std_out[MAX_GROUP_CAP];
    else 
      return ppr_std_err[MAX_GROUP_CAP];
  default:
    if ( out_or_err == stdout ) 
      return ppr_std_out[ spec_order ];
    else 
      return ppr_std_err[ spec_order ];
  }
}

int BOP_printf(const char *format, ...)
{
  va_list v;
  va_start(v, format);

  FILE *stream = get_ppr_stream( stdout, myStatus, mySpecOrder );
  return vfprintf( stream, format, v );
}

/* stop speculation if printing to a file */
int BOP_fprintf(FILE *stream, const char *format, ...)
{
	va_list v;
	va_start(v, format);

	if ( stream == stderr || stream == stdout ) {
	  FILE *ppr_stream = get_ppr_stream( stream, myStatus, mySpecOrder );
	  return vfprintf( ppr_stream, format, v );
	}

	BOP_hard_abort( "BOP fprintf" );
	return vfprintf(stream, format, v);
}

#include "external/malloc.h"
extern mspace meta_space;

/* insert the content of src file to dst */
static void bop_insert_output( FILE *src, FILE *dst ) {
  //get size of src
  fseek(src, 0, SEEK_END);
  off_t src_size = ftell(src);
  rewind(src);

  //allocate buffer, read-src
  char *src_buf = (char *) mspace_malloc(meta_space, (size_t)src_size);
  fread(src_buf, src_size, 1, src);

  //get size of dst
  fseek(dst, 0, SEEK_END);
  off_t dst_size = ftell(dst);
  rewind(dst);

  //allocate buffer, read-src
  char *dst_buf = (char *) mspace_malloc(meta_space, (size_t)dst_size);
  fread(dst_buf, dst_size, 1, dst);

  //write the buffer to (end of) dst
  ftruncate( dst, 0 );
  fwrite(src_buf, src_size, 1, dst);
  fwrite(dst_buf, dst_size, 1, dst);
  
  /* not really a need to free bufs because the caller of this function
     will die */
}

static void bop_append_output( FILE *src, FILE *dst ) {
  //get size of src
  fseek(src, 0, SEEK_END);
  off_t size = ftell(src);
  rewind(src);

  //allocate buffer, read-src
  char *src_buf = (char *) mspace_malloc(meta_space, (size_t)size);
  fread(src_buf, size, 1, src);

  //write the buffer to (end of) dst
  fwrite(src_buf, size, 1, dst);
}

/* called by any task to send the buffered content (if any) to stdout
   or stderr */
static void bop_dump_io( FILE *stream, int status, int spec_order ) {
  char strBuf[1024];
  int nRead;

  if ( myStatus == SEQ ) return;  /* no buffering */

  FILE *ppr_stream = get_ppr_stream( stream, status, spec_order );
  rewind( ppr_stream );

  while(1) {	
    nRead = fread(strBuf, sizeof(char), 1024, ppr_stream);
    if(nRead>0)
      fwrite(strBuf, sizeof(char), nRead, stream);
    else 
      break;
  }
}

void BOP_DumpStdout( void ) {
  bop_dump_io( stdout, myStatus, mySpecOrder );
}

void BOP_DumpStderr( void ) {
  bop_dump_io( stderr, myStatus, mySpecOrder );
}

/* append src content to the end of dst */
static void bop_append_io( int src_spec_order, int dst_spec_order ) {
  assert( myStatus == MAIN || myStatus == SPEC );
  
  FILE *src, *dst;
  src = get_ppr_stream( stdout, myStatus, src_spec_order);
  dst = get_ppr_stream( stdout, myStatus, dst_spec_order);
  bop_append_output( src, dst );

  src = get_ppr_stream( stderr, myStatus, src_spec_order);
  dst = get_ppr_stream( stderr, myStatus, dst_spec_order);
  bop_append_output( src, dst );
  fflush( NULL );
}

void BOP_DumpGroupOutput( void ) {
  int my_spec_order;

#ifdef BOP_SEQ
  /* possible to reach an exit during a GAP */
  if ( myStatus == GAP ) myStatus = SPEC;
#endif

  /* MAIN is possible when coming from BOP_HardAbort */
  assert( myStatus == SPEC || myStatus == UNDY || myStatus == MAIN );
  
  if ( myStatus == SPEC ) {
    int i;
    for ( i = 1 ; i <= mySpecOrder ; i ++ )
      bop_append_io( i, 0 );  /* append task i output to task 0 */
    my_spec_order = 0;
  }
  else if(myStatus == MAIN)
  		my_spec_order = 0;
  else {
  	bop_dump_io( stdout, MAIN, 0 );
    bop_dump_io( stderr, MAIN, 0 );
    my_spec_order = MAX_GROUP_CAP;
  	}
  bop_dump_io( stdout, myStatus, my_spec_order );
  bop_dump_io( stderr, myStatus, my_spec_order );
  fflush( NULL );

  bop_msg(2,"output committed (%d)", my_spec_order);
}
