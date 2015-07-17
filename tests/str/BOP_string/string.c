#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bop_api.h"

#define STEP 20000000
#define min(a,b)  ((a < b) ? a : b)
#define read(x) BOP_use(&x, sizeof x)
#define write(x) BOP_promise(&x, sizeof x)

typedef struct
{
    size_t size;
    char * string;
} BOP_string;

/* creates a BOP_string struct from a char array */
BOP_string* BOP_create_string(char* str) {
	BOP_string* ret = malloc(sizeof *ret);
	size_t len = strlen(str);
	/* have to add one for the terminating character */
	ret->string = malloc((len+1) * sizeof *(ret->string));
	strcpy(ret->string,str);
	ret->size = len;
	return ret;
}

/* returns a copy of the string as a char array */
char* BOP_get_string(BOP_string* bstr) {
	/* have to add one for the terminating character */
	char* ret = malloc(bstr->size * sizeof *ret);
	strcpy(ret, (const char*) ret);
	return ret;
}

/* compares two BOP_strings for equality */
/* Compare bs1 and bs2, returning less than, equal to or
   greater than zero if bs1 is lexicographically less than,
   equal to or greater than bs2.  */
int BOP_strcmp2(BOP_string *bs1, BOP_string *bs2) {
	char* s1 = bs1->string;
	char* s2 = bs2->string;
	size_t min_size = min(bs1->size,bs2->size);
	if (min_size < STEP) {
		int i;
		for (i=0;i<min_size;i++) {
			if (s1[i] != s2[i]) {
				return ((unsigned char) s1[i]) - ((unsigned char) s2[i]);
			}
		}
		return 0;
	}
	/* performs string comparison in parallel using BOP */
	int i;
	int index = min_size+1;
	int ret = 0;
	for (i=0;i<min_size;i+=STEP) {
		if (i > index) {
			return ret;
		}
		BOP_ppr_begin(1);
			int j;
			read(index);
			for (j=i;j<min(i+STEP,min_size);j++) {
				if (s1[j] != s2[j]) {
					//BOP_ordered_begin();
						if (j < index) {
							write(index);
							index = j;
							write(ret);
							ret = ((unsigned char) s1[j]) - ((unsigned char) s2[j]);
						}
						break;
					//BOP_ordered_end();
				}
			}
		BOP_ppr_end(1);
	}
	return ret;
}

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
	int visited = 0;
	long int ub = 0;

	do {

		s1 = p1 + ub;
		s2 = p2 + ub;
		unsigned const register char* itr = s1 + STEP;
		BOP_ppr_begin(1);
			do {
				c1 = (unsigned char) *s1++;
				c2 = (unsigned char) *s2++;
				if (c1 == '\0') {
					write(ret);
					write(visited);
					ret = c1 - c2;
					visited = 1;
					break;
				}
			} while ((c1 == c2) && (s1 < itr));
                        if (c1 != c2) {
                                write(ret);
                                write(visited);
                                ret = c1 - c2;
                                visited = 1;
                        }

		bop_msg(1,"Distance from base pointer: %d",(long int) s1 - (long int) p1);
		BOP_ppr_end(1);
	        read(visited);
		if (visited) {
                        read(ret);
			return ret;
		}

		ub += STEP;

	} while (1);

	BOP_abort_spec("end of loop");

	return c1 - c2;
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
		printf("%d\n",BOP_strcmp(a,b));
	} else {
		printf("%d\n",strcmp(a,b));
	}
  return 0;
}
