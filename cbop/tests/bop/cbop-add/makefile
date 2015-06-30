BOP_DIR = ~/orig_rubybop/cbop/INST
CC = gcc
flags = -g3
target = add
both = both

default: 
	cd $(BOP_DIR); rake;
	make par 
	make seq
	make omp
	make ext
	make both
both:
	$(CC) $(flags) -I $(BOP_DIR) $(both).c -lm $(BOP_DIR)/inst.a -o $(both)
	./$(both) 20 4
omp:
	$(CC) -g3 -fopenmp add_omp.c -lm -o add_omp
par:
	$(CC) $(flags) -I $(BOP_DIR) $(target).c -lm $(BOP_DIR)/inst.a -o $(target)_par

seq:
	$(CC) $(flags) -I $(BOP_DIR) $(target).c -lm -o $(target)_seq

ext:
	cd Extension; ruby extconf.rb; make;

run:
	time ./$(target)_par 100 2
	time ./$(target)_seq 100 2

clean:
	rm -f *_par *_seq *~ add_omp both

