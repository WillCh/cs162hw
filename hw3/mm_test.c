/**
 * A simple test harness for memory alloction. You should augment this with your
 * own tests.
 */

#include <stdio.h>
#include "mm_alloc.h"

void print_array(int *array, int size) {
	int i = 0;
	for (i = 0; i < size; i++) {
		printf("%d  - ", array[i]);
	}
	printf("\n");
}

int main(int argc, char **argv) {
    int *data;
    data = (int *) mm_malloc(4);
    printf("mm_malloc(4)\n");
    debug_print_list();
    data[0] = 1;
    printf("mm_free\n");
    mm_free(data);
    debug_print_list();
    printf("malloc() basic test passed!\n");
    data = (int *) mm_malloc(sizeof(int) * 3);
    data[0] = 1;
    data[1] = 2;
    data[2] = 3;
    print_array(data, 3);
    debug_print_list();
    data = (int *) mm_realloc(data, sizeof(int) * 5);
    data[3] = 4;
    data[4] = 5;
    print_array(data, 5);
    debug_print_list();
    data = (int *) mm_realloc(data, sizeof(int) * 2);
    print_array(data, 2);
    debug_print_list();
    return 0;
}
