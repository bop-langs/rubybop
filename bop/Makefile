#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o

all: $(OBJS) wrapper_test
#
CFLAGS = -Wall -fPIC -ggdb3 -g3 -I. $(OPITIMIZEFLAGS)
OPITIMIZEFLAGS = -O3
LFLAGS = -ldl

malloc_wrapper.o: malloc_wrapper.c
		$(CC) -c -o $@ $^ $(filter-out $(OPITIMIZEFLAGS), $(CFLAGS))

%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

library: malloc_wrapper.o dmmalloc.o

test: all
	./wrapper_test

clean:
	rm -f $(OBJS) wrapper_test
