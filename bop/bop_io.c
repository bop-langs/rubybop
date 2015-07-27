#include <stdlib.h>
#include "bop_api.h"
#include "bop_ports.h"
#include "external/malloc.h"
static mspace io_mspace = NULL;

typedef struct {
  size_t alloc;
  size_t used;
  char *data;
} output_buffer;

static output_buffer *output_buffers = NULL;
static output_buffer undy_buffer = { 0, 0, NULL };
static unsigned num_buffers = 0;
static inline int all_buffs_empty(){
  int x;
  for(x = 0; x < num_buffers; x++)
    if(output_buffers[x].used != 0)
      return 1;
  return 0;
}
int BOP_printf(const char *format, ...)
{
  va_list v, v2;
  va_start(v, format);
  int size;
  output_buffer *buf;

  switch (BOP_task_status()) {
  case SEQ:
  case MAIN:
    size = vfprintf( stdout, format, v );
    va_end(v);
    return size;
  case UNDY:
    buf = &undy_buffer;
    break;
  case SPEC:
    assert( output_buffers != NULL );
    buf = &output_buffers[BOP_spec_order() - 1]; /* - main */
  default:
    abort(); //never reaches here
  }
  assert(buf->data != NULL);

  char *data          = buf->data + buf->used - 1; /* For '\0' */
  size_t avail        = buf->alloc - buf->used;
  assert(*data == '\0');

  /* Try to store */
  va_copy(v2, v);
  size = vsnprintf(data, avail, format, v2);
  va_end(v2);
  if (size < 0) {
    /* return errors */
    va_end(v);
    return size;
  }

  /* Tried to use more space than we have */
  if (size > avail) {
    size_t new_size = buf->alloc;
    do {
      new_size *= 2;
    } while (new_size < buf->used + size);

    if (BOP_task_status() == SPEC) {
      data = (char *) mspace_realloc(io_mspace, buf->data, new_size);
      if (data == NULL)
        BOP_abort_spec("Ran out of output buffer");
    } else  {
      data = (char *) realloc(buf->data, new_size);
    }
    if (data == NULL) {
      va_end(v);
      return -1;
    }

    buf->data  = data;
    buf->alloc = new_size;
    avail = new_size - buf->used;
    assert(avail >= size);

    data += buf->used - 1; /* For '\0' */
    va_copy(v2, v);
    size = vsnprintf(data, avail, format, v2);
    va_end(v2);
  }

  buf->used += size;
  assert( buf->used <= buf->alloc );

  va_end(v);
  return size;
}

static void init_buffer(output_buffer *buf, char *data, size_t size) {
    assert(data != NULL);
    assert(size >= 1);
    buf->alloc   = size;
    buf->used    = 1; /* For '\0' */
    buf->data    = data;
    buf->data[0] = '\0';
}

/* stop speculation if printing to a file */
int BOP_fprintf(FILE *stream, const char *format, ...)
{
  va_list v;
  BOP_abort_spec("File output not yet supported in I/O port");
  va_start(v, format);

  return vfprintf(stream, format, v);
}

static void io_group_init( void ) {
  num_buffers = BOP_get_group_size() - 1; /* - main */

  /* Create a shared memory space */
  if (!io_mspace)
    io_mspace = create_mspace(0, 1, 1); /* Default size, locked, shared */
  assert(io_mspace != NULL);

  /* Create a set of output buffers */
  assert(output_buffers == NULL || all_buffs_empty());
  output_buffers = mspace_calloc(io_mspace, num_buffers, sizeof(output_buffer));
  assert(output_buffers != NULL);

  /* Initialize understudy buffer */
  assert(undy_buffer.data == NULL);
  init_buffer(&undy_buffer, malloc(1), 1);

  /* Initialize buffers */
  int i;
  for (i = 0; i < num_buffers; ++i) {
    char *data = mspace_malloc(io_mspace, 1);
    init_buffer(&output_buffers[i], data, 1);
  }
  /* From this point forward, only the task will access its buffer */
}

static void free_all_buffers( void ) {
  /* Free buffers */
  free(undy_buffer.data);
  undy_buffer.data = NULL;

  int i;
  for (i = 0; i < num_buffers; ++i)
    mspace_free(io_mspace, output_buffers[i].data);
  mspace_free(io_mspace, output_buffers);
  output_buffers = NULL;
}

static void io_group_succ( void ) {
  /* Print specs */
  int i;
  for (i = 0; i < num_buffers; ++i)
    printf("%s", output_buffers[i].data);

  free_all_buffers( );
}

static void io_undy_succ( void ) {
  /* Print undy */
  printf("%s", undy_buffer.data);

  free_all_buffers( );
}
void io_on_malloc_rescue(){
  free_all_buffers();
}
bop_port_t bop_io_port = {
  .ppr_group_init       = io_group_init,
  .task_group_succ_fini = io_group_succ,
  .undy_succ_fini       = io_undy_succ
};
