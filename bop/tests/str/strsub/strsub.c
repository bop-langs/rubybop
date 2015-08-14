#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "bop_api.h"

//6/20/2011
//parallel string subsitution

int avg_subs = 3;
int blocks = 2;
int step_size = 10000000;

#define read(x) BOP_use(&x, sizeof x)
#define write(x) BOP_promise(&x, sizeof x)

char* strsub_original(char* orig_begin, char* sub_begin, char* replace_begin) {

	char* orig = orig_begin; int sub_len = 0; bool sub_set = false;
	char* sub = sub_begin; int replace_len = 0; bool replace_set = false;
	char* replace = replace_begin; int orig_len = 0; bool orig_set = false;

	//sets orig_len, sub_len, and replace_len
	while (*orig || *replace || *sub) {
		if (!orig_set) {
			if (!*orig) {
				orig_set = true;
			} else {
				*orig++;
				orig_len++;
			}
		}
		if (!replace_set) {
			if (!*replace) {
				replace_set = true;
			} else {
				*replace++;
				replace_len++;
			}
		}
		if (!sub_set) {
			if (!*sub) {
				sub_set = true;
			} else {
				*sub++;
				sub_len++;
			}
		}
	}
	if (sub_len < 1) {
		return orig_begin;
	}

	//searches through the original string and replaces what it finds
	sub = sub_begin, replace = replace_begin;
	orig = malloc((orig_len + 1) * sizeof *orig);
	strcpy(orig, orig_begin);
	int i,j;
	int count = 0;
	for (i=0;i<orig_len;i++) {
		if (orig[i] == sub[count]) {
			count++;
		} else if (orig[i] == sub[count-1]) {
			count = 1;
		} else {
			count = 0;
		}
		if (count == sub_len) {
			if (sub_len == replace_len) {
				int itr_start = i - replace_len + 1;
				int itr_end = i+1;
				int replace_itr = 0;
				for (j=itr_start;j<itr_end;j++) {
					orig[j] = replace[replace_itr];
					replace_itr++;
				}
				i = itr_start-1;
			} else {
				int orig_temp_len = orig_len + replace_len - sub_len;
				char* orig_temp = malloc( (orig_temp_len + 1) * sizeof *orig_temp);
				int itr_start = i - sub_len + 1;
				int itr_end = itr_start + replace_len;
				for (j=0;j<itr_start;j++) {
					orig_temp[j] = orig[j];
				}
				int replace_itr=0;
				for (j=itr_start;j<itr_end;j++) {
					orig_temp[j] = replace[replace_itr];
					replace_itr++;
				}
				for (j=itr_end;j<orig_temp_len+1;j++) {
					orig_temp[j] = orig[j - replace_len + sub_len];
				}
				free(orig);
				orig = orig_temp;
				orig_len = orig_temp_len;
			}
		}
	}
	return orig;
}

char* BOP_strsub(char* orig_begin, char* sub_begin, char* replace_begin) {

	char* orig = orig_begin; int sub_len = 0; bool sub_set = false;
	char* sub = sub_begin; int replace_len = 0; bool replace_set = false;
	char* replace = replace_begin; int orig_len = 0; bool orig_set = false;

	//sets orig_len, sub_len, and replace_len
	//not done in parallel; would probably
	//create too much overhead
	while (*orig || *replace || *sub) {
		if (!orig_set) {
			if (!*orig) {
				orig_set = true;
			} else {
				*orig++;
				orig_len++;
			}
		}
		if (!replace_set) {
			if (!*replace) {
				replace_set = true;
			} else {
				*replace++;
				replace_len++;
			}
		}
		if (!sub_set) {
			if (!*sub) {
				sub_set = true;
			} else {
				*sub++;
				sub_len++;
			}
		}
	}
	if (sub_len < 1) {
		return orig_begin;
	}

	//searches through the original string and replaces what it finds
	sub = sub_begin, replace = replace_begin;
	orig = malloc((orig_len + 1) * sizeof *orig);
	strcpy(orig, orig_begin);

	int count = 0;
	int i,j,ii;
	int block_size = orig_len/blocks;

	for (ii=0;ii<orig_len;ii+=step_size) {
		BOP_ppr_begin(1);
		int itr_initial = ii;
		int itr_final = min(orig_len,ii+step_size);

		for (i=itr_initial;i<itr_final;i++) {
			if (orig[i] == sub[count]) {
				count++;
			} else if (orig[i] == sub[count-1]) {
				count = 1;
			} else {
				count = 0;
			}
			if (count != 0 && i == itr_final - 1)
				itr_final++;
			if (count == sub_len) {
				if (sub_len == replace_len) {
					int itr_start = i - replace_len + 1;
					int itr_end = i+1;
					int replace_itr = 0;
					for (j=itr_start;j<itr_end;j++) {
						read(orig[j]);
						write(orig[j]);
						orig[j] = replace[replace_itr];
						replace_itr++;
					}
					i = itr_start-1;
				} else {
					read(orig);
					write(orig);
					read(orig_len);
					write(orig_len);
					read(i);
					write(i);
					read(itr_final);
					write(itr_final);
					int orig_temp_len = orig_len + replace_len - sub_len;
					char* orig_temp = malloc( (orig_temp_len + 1) * sizeof *orig_temp);
					int itr_start = i - sub_len + 1;
					int itr_end = itr_start + replace_len;
					for (j=0;j<itr_start;j++) {
						orig_temp[j] = orig[j];
					}
					int replace_itr=0;
					for (j=itr_start;j<itr_end;j++) {
						orig_temp[j] = replace[replace_itr];
						replace_itr++;
					}
					for (j=itr_end;j<orig_temp_len+1;j++) {
						orig_temp[j] = orig[j - replace_len + sub_len];
					}
					free(orig);
					orig = orig_temp;
					orig_len = orig_temp_len;
					i = itr_start-1;
					itr_final = min(orig_len,ii+step_size);
				}
			}
		}
		BOP_ppr_end(1);
	}
	read(*orig);
	return orig;
}

int min(int a,int b) {
	if (a<b)
		return a;
	return b;
}

int find_int(int* haystack, int size, int needle) {
	int step = 1000;
	int i,j;
	for (i=0;i<size;i+=step) {
		BOP_ppr_begin(1);
			for (j=i;j<min(size,i+step);j++) {
				read(j);
				if (haystack[j] == needle) {
					write(j);
					return j;
				}
			}
		BOP_ppr_end(1);
	}
	return 0;
}

int main(int argc, char *argv[]) {
	printf("%s\n",BOP_strsub("my name is inigo montoya you killed my father prepare to die","father","mother"));
	printf("%s\n",BOP_strsub("a man a plan a canal panama","an","q"));
	printf("%s\n",BOP_strsub("a man a plan a canal panama","man","can"));
	printf("%s\n",BOP_strsub("a man a plan a canal panama","man","canama"));
	printf("%s\n",BOP_strsub("a man a plan a canal panama","an","qqqqqq"));

	/* open file for reading */
	FILE* fStream = fopen(argv[1], "r");
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
	//
	// //printf("%s\n",BOP_strsub(str,argv[2],argv[3]));
	// int a[10000];
	// int i;
	// for (i=0;i<10000;i++) {
	// 	a[i] = i;
	// }
	// printf("%d\n",find_int(a,10000,9500));
	return 0;
}
