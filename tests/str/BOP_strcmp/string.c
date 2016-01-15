#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bop_api.h"

#define STEP 20000000
#define min(a,b)  ((a < b) ? a : b)
#define read(x) BOP_use(&x, sizeof x)
#define write(x) BOP_promise(&x, sizeof x)

/* compares two strings for equality */
/* Compare bs1 and bs2, returning less than, equal to or
   greater than zero if bs1 is lexicographically less than,
   equal to or greater than bs2.  */
int BOP_strcmp(char* p1, char* p2) {

	register const unsigned char *s1 = (const unsigned char *) p1;
	register const unsigned char *s2 = (const unsigned char *) p2;
	unsigned register char c1, c2;

	int equal = 1;
	int ret;
	int done = 0;
	long int ub = 0;

	do {
		s1 = p1 + ub;
		s2 = p2 + ub;
		unsigned const register char* itr = s1 + STEP;
		BOP_ppr_begin(1);
			do {
				c1 = (unsigned char) *s1++;
				c2 = (unsigned char) *s2++;
				if (c1 == '\0') break;
			} while ((c1 == c2) && (s1 < itr));
                        if (c1 == '\0' || c1 != c2) {
                                write(done);
                                ret = ((c1) - (c2));
                                done = 1;
                        }
		bop_msg(1,"Distance from base pointer: %d",(long int) s1 - (long int) p1);
		BOP_ppr_end(1);

		ub += STEP;
		read(done);
	} while (!done);

	return ret;
}

/* reads a string in from a file */
char* read_string(char* file_name) {
	/* open file for reading */
	FILE* fStream = fopen(file_name, "r");
	if(fStream == NULL){fputs("File open error\n",stderr); exit(1);}

	/* get file size */
	fseek(fStream, 0, SEEK_END);
	int fSize = ftell(fStream);
	rewind(fStream);

	/* allocate memory */
	char* str = malloc(fSize * sizeof *str);
	if(str == NULL){fputs("Memory error\n", stderr); exit(2);}

	/* read sequence into buffer */
	size_t result = fread(str, 1, fSize - 1, fStream);
	if(result != fSize - 1){fputs("Reading error\n", stderr); exit(3);}

	/* terminate */
	fclose(fStream);

	return str;
}

/* test the functions defined in this file */
int main(int argc, char *argv[]) {
	char* a = read_string(argv[1]);
	char* b = read_string(argv[2]);

	bop_msg(1,"before string compare",0);

	if (argv[3][0] == 'b') {
		printf("Compaing...\n");
		printf("The Result: %d\n",BOP_strcmp(a,b));
	} else {
		printf("%d\n",strcmp(a,b));
	}
	return 0;
}
