#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC = gcc
OBJS = malloc_wrapper.o dmmalloc.o ary_bitmap.o postwait.o bop_merge.o range_tree/dtree.o bop_ppr.o utils.o external/malloc.o bop_ppr_sync.o bop_io.o bop_ports.o bop_ordered.o sys_wrapper.o
ALL = $(OBJS) $(TESTS)

CFLAGS = -Wall -fPIC -pthread -I. $(OPITIMIZEFLAGS)  -Wno-unused-function $(CUSTOMDEF)
CUSTOMDEF = -D USE_DL_PREFIX -DBOP -D__LINUX__ -U NDEBUG
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS
ARCHIVE = inst.a

library: $(ARCHIVE)

$(ARCHIVE): $(OBJS)
	ar r $(ARCHIVE) $(OBJS)
	ranlib $(ARCHIVE)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: library

%_wrapper.o: %_wrapper.c #wrappers are assumed to use dlsym, which breaks with optimizations
	$(CC) -c -o $@ $^ $(filter-out $(OPITIMIZEFLAGS), $(CFLAGS))

%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

wrapper_test: $(OBJS)
malloc_test: $(OBJS)

clean:
	rm -f $(OBJS) wrapper_test malloc_test inst.a
