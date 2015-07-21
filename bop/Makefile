#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC ?= gcc
ifeq ($(CC), cc)
  CC = gcc
endif
OBJS = malloc_wrapper.o dmmalloc.o ary_bitmap.o postwait.o bop_merge.o range_tree/dtree.o bop_ppr.o utils.o external/malloc.o bop_ppr_sync.o bop_io.o bop_ports.o bop_ordered.o
ALL = $(OBJS) $(TESTS)

CFLAGS_DEF = -Wall -fPIC -pthread -g3 -I. -Wno-unused-function $(PLATFORM) $(CUSTOMDEF)
CUSTOMDEF = -D USE_DL_PREFIX -D BOP
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS -U NDEBUG
LIB = inst.a
CFLAGS = $(CFLAGS_DEF) $(OPITIMIZEFLAGS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    PLATFORM += -D__LINUX__
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM += -D__OSX__
    CUSTOMDEF += -D_XOPEN_SOURCE
endif

library: $(LIB)

$(LIB): $(OBJS)
	ar r $(LIB) $(OBJS)
	ranlib $(LIB)

debug: CFLAG = $(CFLAGS_DEF)  $(DEFBUG_FLAGS)
debug: library

%_wrapper.o: %_wrapper.c #any _wrapper class needs the optimization filtering
		$(CC) -c -o $@ $^ $(CFLAGS_DEF)
%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

wrapper_test: $(OBJS)
malloc_test: $(OBJS)

clean:
	rm -f $(OBJS) wrapper_test malloc_test inst.a
