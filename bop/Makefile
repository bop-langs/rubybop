#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC ?= gcc
ifeq ($(CC), cc)
  CC = gcc
endif
OBJS = malloc_wrapper.o dmmalloc.o ary_bitmap.o postwait.o bop_merge.o range_tree/dtree.o bop_ppr.o utils.o external/malloc.o bop_ppr_sync.o bop_io.o bop_ports.o bop_ordered.o

CFLAGS_DEF = -Wall -fPIC -pthread -g3 -I. -Wno-unused-function $(PLATFORM) $(CUSTOMDEF)
CUSTOMDEF = -D USE_DL_PREFIX -D BOP
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS -U NDEBUG
LIB = inst.a
CFLAGS = $(CFLAGS_DEF) $(OPITIMIZEFLAGS)
CC = gcc
_OBJS = malloc_wrapper.o dmmalloc.o ary_bitmap.o postwait.o bop_merge.o range_tree/dtree.o bop_ppr.o utils.o external/malloc.o bop_ppr_sync.o bop_io.o bop_ports.o bop_ordered.o

CFLAGS = -Wall -fPIC -pthread -I. $(DEBUG_FLAGS)  -Wno-unused-function $(CUSTOMDEF)
CUSTOMDEF = -D USE_DL_PREFIX -D BOP -D__LINUX__
LDFLAGS = -Wl,--no-as-needed -ldl
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -U NDEBUG
BUILD_DIR = ../build/bop
LIB_SO = $(BUILD_DIR)/inst.a

OBJS = $(patsubst %,$(BUILD_DIR)/%,$(_OBJS))

library: $(LIB_SO)

$(LIB_SO): $(OBJS)
	ar r $(LIB_SO) $(OBJS)
	ranlib $(LIB_SO)

debug: CFLAG = $(CFLAGS_DEF)  $(DEFBUG_FLAGS)
debug: library

$(BUILD_DIR)/%_wrapper.o: %_wrapper.c #any _wrapper class needs the optimization filtering
		@mkdir -p $(@D)
		$(CC) -c -o $@ $^ $(CFLAGS_DEF)
all: $(OBJS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $^ $(CFLAGS)

clean:
	rm -f $(OBJS) $(LIB_SO)
