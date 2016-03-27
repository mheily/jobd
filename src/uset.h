/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __USET_H
#define __USET_H

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

/* An unordered set of things that can dynamically grow */
struct uset {
	size_t 	length;   	/* Number of elements actually used */
	size_t 	allocated; 	/* Number of elements malloc'd */
	void   (*destructor)(void *);	/* Called when an element is removed */
	void	**items;
};
typedef struct uset *uset_t;

static inline uset_t uset_new(void (*destructor)(void *)) {
	uset_t us;

	us = calloc(1, sizeof(struct uset));
	if (us == NULL) return NULL;
	us->destructor = destructor;
	us->allocated = 50;
	us->items = calloc(us->allocated, sizeof(void *));
	us->length = 0;
	return us;
}

static inline int uset_add(uset_t us, void **item) {
	int i;
	void **new_items;

	/* Find the first empty slot */
	for (i = 0; i < us->allocated; i++) {
		if (us->items[i] == NULL) {
			us->items[i] = *item;
			if (us->length <= i) us->length = i + 1;
			printf("added item %p to slot %d, length now %zu\n", item, i, us->length);
			return (0);
		}
	}

	/* Otherwise, add more slots */
	new_items = realloc(us->items, us->allocated + 50);
	if (new_items == NULL) {
		return (-1);
	} else {
		us->items = new_items;
		us->allocated += 50;
	}
	us->items[us->length] = item;
	us->length++;
	return (0);
}

static inline void *uset_get(uset_t us, size_t index) {
	if (index > us->length) {
		return NULL;
	} else {
		printf("returning item %p from slot %zu\n", us->items[index], index);
		return us->items[index];
	}
}

static inline void uset_free(uset_t us) {
	int i;

	if (us == NULL) return;

	for (i = 0; i < us->length; i++) {
		us->destructor(us->items[i]);
	}
	free(us->items);
	free(us);
}

static inline void uset_dump(uset_t us) {
	printf("uset stats: length=%zu, allocated=%zu\n", us->length, us->allocated);
}

#endif /* __USET_H */
