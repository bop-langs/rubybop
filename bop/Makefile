CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o
#
CFLAGS = -Wall -fPIC -ggdb3 -g3 -I.
LFLAGS = -ldl

%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)
	
library: malloc_wrapper.o dmmalloc.o
	
test: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LFLAGS) -o wrapper wrapper_test.c
	./wrapper

clean:
	rm -f $(OBJS) wrapper
