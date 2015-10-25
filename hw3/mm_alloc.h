/*
 * mm_alloc.h
 *
 * Exports a clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
 #include <stdbool.h>
typedef struct slot slot;
struct slot
{
	slot *prev;
	slot *next;
	bool isFree;
	size_t size;
};
void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
void debug_print_list();
