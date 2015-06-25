#include "malloc_wrapper.h"
#include <stdio.h>
#include <stddef.h>

int main(){
	printf("start\n");
	int k;
	int nums = 50;
	int * ints;
	for(k = 0; k < 5; k++){
	//	switch (k % 2){
		//	case 0:
				ints = calloc(nums, sizeof(int));
		//	default:
			//	ints = malloc(nums * sizeof(int));
		//}
		int i;
		int count = 0;
		for(i = 0; i < nums; i++)
			ints[i] = i*(k+1);
		for(i = 0; i < nums; i++)
			count += ints[i];
		free(ints);
		printf("sum %d\n", count);
	}
	ints = calloc(nums, sizeof(int));
	
	return 0;
}
