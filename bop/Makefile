#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o
TESTS = wrapper_test malloc_test
ALL = $(OBJS) $(TESTS)

CFLAGS = -Wall -fPIC -ggdb3 -g3 -I. $(OPITIMIZEFLAGS)
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O3
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS -U NDEBUG

library: $(OBJS)
all: $(ALL)
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(ALL)

malloc_wrapper.o: malloc_wrapper.c
		$(CC) -c -o $@ $^ $(filter-out $(OPITIMIZEFLAGS), $(CFLAGS))

%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

test: debug library wrapper_test malloc_test
	./wrapper_test
	./malloc_test

wrapper_test: $(OBJS)
malloc_test: $(OBJS)

clean:
	rm -f $(OBJS) wrapper_test malloc_test
