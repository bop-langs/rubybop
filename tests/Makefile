#NOTE: for malloc_wrapper to build correctly it cannot be compiled with optimizaitons (ie don't specify -O option). All other seperate files can be compiled with optimizations as normal and still build correctly.

CC = gcc
OBJS = malloc_test.o wrapper_test.o $(SLEEP_OBJS) $(ADD_OBJS) $(STR_CMP_OBJS) $(STR_OBJS) $(STR_SUB_OBJS) $(STR_SUB_OBJS2)
TESTS = malloc_test wrapper_test $(ADD_TESTS) $(SLEEP_TESTS) $(STR_CMP_TESTS) $(STR_TESTS) $(STR_SUB_TESTS) $(STR_SUB_TESTS2)
ALL = $(OBJS) $(TESTS)
BOP_DIR = -I../bop/
BOP_LIB = ../bop/inst.a

ADD_OBJS = add/add_bop.o add/add_orig.o add/add_ppr_only_bop.o add/add_ppr_only_orig.o
ADD_TESTS = add/add_bop add/add_orig add/add_ppr_only_bop add/add_ppr_only_orig

SLEEP_OBJS = sleep/sleep_orig.o sleep/sleep_bop.o
SLEEP_TESTS = sleep/sleep_orig sleep/sleep_bop

STR_CMP_OBJS = str/BOP_strcmp/string_orig.o str/BOP_strcmp/string_bop.o
STR_CMP_TESTS = str/BOP_strcmp/string_bop str/BOP_strcmp/string_orig

STR_OBJS = str/BOP_string/string_orig.o str/BOP_string/string_bop.o
STR_TESTS = str/BOP_string/string_orig str/BOP_string/string_bop

STR_SUB_OBJS = str/strsub/strsub_orig.o str/strsub/strsub_bop.o
STR_SUB_TESTS= str/strsub/strsub_orig str/strsub/strsub_bop

STR_SUB_OBJS2 = str/strsub2/string_orig.o str/strsub2/string_bop.o
STR_SUB_TESTS2 = str/strsub2/string_orig str/strsub2/string_bop



CFLAGS = -fPIC $(OPITIMIZEFLAGS)  -Wno-unused-function $(CUSTOMDEF) $(INCFLAGS)
CUSTOMDEF = -DUSE_DL_PREFIX -D__LINUX__ -DBOP
LDFLAGS = -Wl,--no-as-needed -ldl -lm $(BOP_LIB) $(INCFLAGS)
INCFLAGS = $(BOP_DIR) $(OPITIMIZEFLAGS)
OPITIMIZEFLAGS = -O2
DEBUG_FLAGS = -ggdb3 -g3 -pg -D CHECK_COUNTS -U NDEBUG

all: $(ALL)
	@echo 'BOP MAKE: all objects built'

test: all #NOTE: for now, it just builds


malloc_test: malloc_test.o

wrapper_test: wrapper_test.o

test_sleep: $(SLEEP_OBJS) $(SLEEP_TESTS)
	./sleep/sleep_orig
	./sleep/sleep_bop

test_add: $(ADD_OBJS) $(ADD_TESTS)
	./add/add_bop 20 4
	./add/add_orig 20 4
	./add/add_ppr_only_bop 20 4
	./add/add_ppr_only_orig 20 4

%:%.o #executable
	$(CC) -o $@ $^ $(LDFLAGS) $(INCFLAGS)
	#./$@

%_orig.o : %.c
	$(CC) -c -o $@ $^ $(CFLAGS)
%_bop.o : %.c
	$(CC) -c -o $@ $^ $(CFLAGS) -DBOP
%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

clean:
	rm -f $(ALL)
