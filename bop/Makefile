#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC = gcc
_OBJS = malloc_wrapper.o dmmalloc.o ary_bitmap.o postwait.o bop_merge.o range_tree/dtree.o bop_ppr.o utils.o external/malloc.o bop_ppr_sync.o bop_io.o bop_ports.o bop_ordered.o

CFLAGS = -Wall -fPIC -pthread -I. $(OPITIMIZEFLAGS)  -Wno-unused-function $(CUSTOMDEF)
CUSTOMDEF = -D USE_DL_PREFIX -D BOP -D__LINUX__
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS -U NDEBUG
BUILD_DIR = ../build/bop
LIB_SO = $(BUILD_DIR)/inst.a

OBJS = $(patsubst %,$(BUILD_DIR)/%,$(_OBJS))

library: $(LIB_SO)

$(LIB_SO): $(OBJS)
	ar r $(LIB_SO) $(OBJS)
	ranlib $(LIB_SO)

all: $(OBJS)
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(OBJS)

$(BUILD_DIR)/malloc_wrapper.o: malloc_wrapper.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $^ $(filter-out $(OPITIMIZEFLAGS), $(CFLAGS))

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $^ $(CFLAGS)

clean:
	rm -f $(OBJS) $(LIB_SO)
