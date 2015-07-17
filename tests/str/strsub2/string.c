#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bop_api.h"

#define STEP 8000000
#define min(a,b)  ((a < b) ? a : b)
#define read(x) BOP_use(&x, sizeof x)
#define write(x) BOP_promise(&x, sizeof x)

#define true 1
#define false 0

/* A BOP_String contains an array of strings (char pointers) */
#define SUBSTRING_SIZE 100000
typedef struct
{
    char ** string;
} BOP_string;

/* creates a BOP_string struct from a char array
 * the BOP_string contains an array of char pointers
 * the last pointer is a null pointer, the rest of
 * the pointers contain substrings that make up the string */
BOP_string* BOP_create_string(char* str) {
	BOP_string* ret = malloc(sizeof *ret);
	size_t len = strlen(str);
	/* If empty string, return an array with one element:
	 * a null pointer */
	if (len == 0) {
		ret->string = malloc(sizeof(char*));
		ret->string[0] = NULL;
	}
	/* Otherwise create an array of char pointers
	 * and loop through the string giving each pointer
	 * SUBSTRING_SIZE chars */
	int num = len/SUBSTRING_SIZE + 2;
	ret->string = malloc(num * sizeof(char*));
	/* Set the end to NULL */
	ret->string[num-1] = NULL;
	/* Allocate the first substring */
	ret->string[0] = malloc((SUBSTRING_SIZE + 1) * sizeof(char));
	int count = 0;
	int substring = 0;
	/* Copy chars from str to substring/allocate new substrings
	 * when requried */
	do {
		if (count == SUBSTRING_SIZE) {
			ret->string[substring++][count] = '\0';
			count=0;
			ret->string[substring] = malloc((SUBSTRING_SIZE + 1) * sizeof(char));
		}
		ret->string[substring][count] = *str;
		count++;
	} while (*(str++) != '\0');
	ret->string[substring][count] = '\0';
	return ret;
}

/* Prints a BOP_string to stdout */
void BOP_print_string(BOP_string* str) {
	int i = 0;
	while (str->string[i] != NULL) {
		printf("%s",str->string[i]);
		i++;
	}
	printf("\n");
}

/* Finds all instances of find and replaces them with replace */
BOP_string* BOP_replace(BOP_string* original, char* find, char* replace) {
	int i = 0;
	int len = strlen(find);
	int len_replace = strlen(replace);
	while (original->string[i] != NULL) {
		BOP_ppr_begin(1);
			int j = 0;
			/* count is the number of characters that match */
			int count = 0;
			int num_matches = 0;
			/* size is the number of chars in the array */
			int size = 0;
			while (original->string[i][j] != '\0') {
				size++;
				if (original->string[i][j] == find[count]) {
					count++;
					if (count == len) {
						count = 0;
						if (len == len_replace) {
							int k;
							int h = 0;
							for (k=j-len+1;k<j+1;k++) {
								original->string[i][k] = replace[h];
								h++;
							}
						} else {
							num_matches++;
						}
					}
				} else {
					count = 0;
				}
				j++;
			}
			BOP_use(original->string[i], (size + 1) * sizeof(char));
			/* If at least one instance of find was found in the substring */
			if (num_matches != 0 || count != 0) {
				/* First, we have to check to see if there is a match that is
				 * split between two substrings */
				int first_copy = false;
				if (count != 0) {
					int f_count = count;
					if (original->string[i+1] != NULL) {
						int ii = 0;
						int leftover = 0;
						while (original->string[i+1][ii] == find[f_count]) {
							f_count++;
							ii++;
							if (f_count == 0) {
								leftover = 0;
								break;
							} else if (f_count == len) {
								leftover = ii;
								break;
							}
						}
						if (leftover > 0) {
							size += len_replace - count;
							first_copy = true;
							int jj = 0;
							/* Shifts the second substring over by leftover */
							BOP_promise(original->string[i+1],strlen(original->string[i+1]+1) * sizeof(char));
							while ((original->string[i+1][jj] = original->string[i+1][jj+leftover]) != '\0') {
								jj++;
							}
						}
					}
				}
				/* This piece of code reallocates the block if the size increased */
				int jump_size = len_replace - len;
				int new_size = size + (num_matches * jump_size);
				int old_substring_size = size/SUBSTRING_SIZE + 1;
				int new_substring_size = new_size/SUBSTRING_SIZE + 1;
				char* ret = original->string[i];
				if (new_substring_size > old_substring_size) {
					ret = malloc((new_substring_size * SUBSTRING_SIZE + 1) * sizeof(char));
				}

				/* If a match was found between substrings, it gets added to the tail of
				 * the first substring */
				if (first_copy) {
					int ii;
					int jj = size-1;
					ret[size] = '\0';
					for (ii = len_replace-1;ii>=0;ii--) {
						ret[jj] = replace[ii];
						jj--;
					}
				}

				/* Next, the substring is iterated through and find/replace is performed.
				 * When find/replace occurs in this code, it ends up iterating through the
				 * string twice, once to count the number of finds, and once to actually do
				 * the replaces. If this were not the case, then some kind of data structure
				 * would be needed to keep track of these finds. But then, all characters after
				 * a replace would still need to be iterated over. So overall, I don't think
				 * any speed would be gained by doing that and it would add some complexity */

				if (jump_size > 0 && num_matches != 0) {
					int r_count = len;
					int jump = num_matches * jump_size;
					/* The search is performed backwards if new the substring is larger than the
					 * original substring. This makes it eacier and more efficient */
					for (;j>=0;j--) {
						if (original->string[i][j] == find[r_count-1]) {
							r_count--;
							if (r_count == 0) {
								int h;
								int k = j+len+jump-1;
								for (h = len_replace-1;h>=0;h--) {
									ret[k] = replace[h];
									k--;
								}
								jump -= jump_size;
								r_count = len;
								if (jump == 0 && new_substring_size < old_substring_size) {
									break;
								}
							} else {
								ret[j+jump] = original->string[i][j];
							}
						} else {
							r_count = len;
							ret[j+jump] = original->string[i][j];
						}
					}
					if (new_substring_size > old_substring_size) {
						free(original->string[i]);
						original->string[i] = ret;
					}
				} else if (jump_size < 0 && num_matches != 0) {
					/* The search is performed forwards otherwise */
					int jump = 0;
					char current = original->string[i][0];
					int f_count = 0;
					for (j=0;;j++) {
						ret[j] = ret[j-jump];
						if (ret[j] == '\0') {
							break;
						}
						if (ret[j] == find[f_count]) {
							f_count++;
							if (f_count == len) {
								int k = j - len + 1;
								int h;
								for (h=0;h<len_replace;h++) {
									ret[k] = replace[h];
									k++;
								}
								jump += jump_size;
								f_count = 0;
								j = j - len + 1;
							}
						} else {
							f_count = 0;
						}
					}
				}
				write(original->string[i]);
				BOP_promise(original->string[i], ((new_substring_size * SUBSTRING_SIZE + 1) * sizeof(char)));
				num_matches = 0;
			}
		BOP_ppr_end(1);
		i++;
	}
	return original;
}

/* returns a copy of the string as a char array */
char* BOP_get_string(BOP_string* bstr) {
	return NULL;
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

	BOP_string* a2 = BOP_create_string(a);

	BOP_replace(a2,argv[2],argv[3]);
	BOP_print_string(a2);
return 0;
	/*if (argv[3][0] == 'b') {
		printf("%d\n",BOP_strcmp(a,b));
	} else {
		printf("%d\n",strcmp(a,b));
	}*/

	//BOP_string* a = BOP_create_string(read_string(argv[1]));
	//BOP_print_string(a);

	/*BOP_string* b = BOP_create_string("My name is Inigo Montoya. You killed my father. Prepare to name.");
	BOP_print_string(b);
	BOP_replace(b,"name","q");
	BOP_print_string(b);*/
}
