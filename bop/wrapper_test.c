#include "malloc_wrapper.h"
#include <stdio.h>
#include <stddef.h>

int main(){
	printf("start\n");
	int * ints = malloc(300*sizeof(int));
	int i;
	int count = 0;
	for(i = 0; i < 300; i++)
		ints[i] = i;
	for(i = 0; i < 300; i++)
		count += ints[i];
	printf("sum %d\n", count);
	return 1;
}
