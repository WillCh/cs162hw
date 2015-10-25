/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

slot *head, *tail;
bool isInit = false;
void init_malloc() {
	assert(isInit == false);
	head = sbrk(sizeof(slot));
	head->prev = NULL;
	head->next = NULL;
	head->size = 0;
	head->isFree = false;
	isInit = true;
	tail = head;
}

// If not found, return NULL
slot* find_list_elem(size_t size) {
	assert(head != NULL);
	slot* iter = head;
	while (iter != NULL) {
		if (iter->isFree) {
			if (iter->size >= size) {
				return iter;
			}
		}
		iter = iter->next;
	}
	return NULL;
}

void split_slot(slot *old_slot, size_t size) {
	size_t min_new_size = sizeof(slot) + 4;
	if (old_slot->size >= size + min_new_size) {
		slot *new_slot = (slot*) (old_slot + sizeof(slot) + size);
		new_slot->prev = old_slot;
		new_slot->next = old_slot->next;
		new_slot->isFree = true;
		new_slot->size = old_slot->size - size - sizeof(slot);
		old_slot->next = new_slot;
		if (new_slot->next != NULL) {
			new_slot->next->prev = new_slot;
		}
		old_slot->size = size;
		if (old_slot == tail) {
			tail = new_slot;
		}
	}
}

slot* find_list_elem_data_location(void *data_pnt) {
	slot *iter = head;
	while (iter != NULL) {
		if (iter + sizeof(slot) == data_pnt) {
			return iter;
		}
		iter = iter->next;
	}
	return NULL;
}

void merge_free_space(slot* slot_pnt) {
	// merge to the following slot
	slot* iter = slot_pnt->next;
	slot* last = slot_pnt;
	while (iter != NULL && (iter->isFree)) {
		slot_pnt->size += iter->size + sizeof(slot);
		last = iter;
		iter = iter->next;
	}
	if (last != slot_pnt) {
		slot_pnt->next = last->next;
		if (last->next == NULL) {
			tail = slot_pnt;
		} else {
			last->next->prev = slot_pnt;
		}
	}
	// set them to be zero
	memset(slot_pnt + sizeof(slot), 0, slot_pnt->size);
	// merge to the before slot
	iter = slot_pnt->prev;
	slot *tmp_head = slot_pnt;
	while (iter != NULL && (iter->isFree)) {
		iter->size += iter->next->size + sizeof(slot);
		tmp_head = iter;
		iter = iter->prev;
	}
	if (tmp_head != slot_pnt) {
		tmp_head->next = slot_pnt->next;
		if (slot_pnt->next != NULL) {
			slot_pnt->next->prev = tmp_head;
		}
	}
	if (slot_pnt == tail) {
		tail = tmp_head;
	}
	memset(tmp_head + sizeof(slot), 0, tmp_head->size);

}

void *mm_malloc(size_t size) {
    /* YOUR CODE HERE */
    if (!isInit) {
    	init_malloc();
    }
    if (size == 0) return NULL;
    slot* possible_slot = find_list_elem(size);
    if (possible_slot != NULL) {
    	// need to check whether need to split the space
    	split_slot(possible_slot, size);
    	possible_slot->isFree = false;
    	return (possible_slot + sizeof(slot));
    } else {
    	// need to move the brk
    	slot *new_slot = sbrk(sizeof(slot) + size);
    	if (new_slot == -1) return NULL;
    	memset(new_slot, 0, sizeof(slot) + size);
    	new_slot->next = NULL;
    	new_slot->prev = tail;
    	new_slot->size = size;
    	new_slot->isFree = false;
    	tail->next = new_slot;
    	tail = new_slot;
    	return (new_slot + sizeof(slot));
    }
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    assert (size >= 0);
    if (size == 0) return ptr;
    size_t min_new_size = sizeof(slot) + 4;
    slot *cur_slot = find_list_elem_data_location(ptr);
    if (cur_slot->size >= size + min_new_size) {
    	// we can insert a new slot and use the orgianl space
    	slot *new_slot = cur_slot + sizeof(slot) + size;
    	memset(new_slot, 0, cur_slot->size - size);
    	new_slot->prev = cur_slot;
    	new_slot->next = cur_slot->next;
    	new_slot->isFree = true;
    	new_slot->size = cur_slot->size - size;
    	cur_slot->size = size;
    	cur_slot->next = new_slot;
    	if (new_slot->next == NULL) {
    		tail = new_slot;
    	} else {
    		new_slot->next->prev = new_slot;
    	}
    	merge_free_space(new_slot);
    	return ptr;
    }
    void *new_ptr = mm_malloc(size);
    memcpy(new_ptr, ptr, cur_slot->size);
    mm_free(ptr);
    return new_ptr;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
    if (ptr != NULL) {
    	slot *slot_pnt = find_list_elem_data_location(ptr);
    	// free the space
    	memset(ptr, 0, slot_pnt->size);
    	slot_pnt->isFree = true;
    	merge_free_space(slot_pnt);
    }
}

void debug_print_list() {
	slot *iter = head;
	printf("the size of head is %zu\n", sizeof(slot));
	printf("print the list\n");
	while (iter != NULL) {
		printf("prev: %p, next: %p, isFree: %d, size: %zu\n",
				iter->prev, iter->next, iter->isFree, iter->size);
		iter = iter->next;
	}
}