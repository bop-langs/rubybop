CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o

CFLAGS = -W -ggdb -pg
LFLAGS = -ldl

DEPS = dmmalloc.h malloc_wrapper.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
	
wrapper: dmmalloc.o malloc_wrapper.o
	$(CC) $(CFLAGS) $(OBJS) $(LFLAGS) -o wrapper wrapper_test.c

clean:
	rm -f $(OBJS) wrapper
