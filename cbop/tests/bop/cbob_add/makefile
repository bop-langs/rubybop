BOP_DIR = ~/orig_rubybop/cbop/INST
CC = gcc
flags = g3
target = add

default: 
	cd $(BOP_DIR); rake;
	make par 
	make seq

par:
	$(CC) -$(flags) -I $(BOP_DIR) $(target).c -lm $(BOP_DIR)/inst.a -o $(target)_par

seq:
	$(CC) -$(flags) -I $(BOP_DIR) $(target).c -lm -o $(target)_seq

run:
	time ./$(target)_par 100 2
	time ./$(target)_seq 100 2

clean:
	rm *_par *_seq *~
