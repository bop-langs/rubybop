CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o
#
CFLAGS = -Wall -fPIC -ggdb3 -g3
LFLAGS = -ldl

DEPS = dlmalloc.h malloc_wrapper.h dmmalloc.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
	
library: malloc_wrapper.o dmmalloc.o
	
test: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LFLAGS) -o wrapper wrapper_test.c

clean:
	rm -f $(OBJS) wrapper
