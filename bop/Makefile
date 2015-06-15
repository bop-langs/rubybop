CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o

CFLAGS = -W  -ldl -ggdb #--no-as-needed

DEPS = dmmalloc.h malloc_wrapper.h

srcdir = .

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
	
wrapper: dmmalloc.o malloc_wrapper.o
	$(CC) -o wrapper wrapper_test.c -I. $(CFLAGS) $(INCFLAGS) $(OBJS)
	
clean:
	rm -f $(OBJS) wrapper
