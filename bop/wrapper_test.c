#include "malloc_wrapper.h"
#include <stdio.h>
#include <stddef.h>

int main(){
	printf("start\n");
	int k;
	for(k = 0; k < 5; k++){
		int * ints = calloc(300, sizeof(int));
		int i;
		int count = 0;
		for(i = 0; i < 300; i++)
			ints[i] = i*(k+1);
		for(i = 0; i < 300; i++)
			count += ints[i];
		free(ints);
		printf("sum %d\n", count);
	}
	return 1;
}
