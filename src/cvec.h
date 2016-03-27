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

#ifndef __CVEC_H
#define __CVEC_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* A simple vector of strings that can dynamically grow */
struct cvec {
	size_t 	length;   	/* Number of elements actually used */
	size_t 	allocated; 	/* Number of elements malloc'd */
	char	**items;
};
typedef struct cvec *cvec_t;

static inline cvec_t cvec_new()
{
	cvec_t cv;

	cv = calloc(1, sizeof(*cv));
	if (cv == NULL) return NULL;
	return cv;
}

static inline int cvec_resize(cvec_t cv, const size_t new_size)
{
	char **new_items;

	if (cv->allocated == cv->length) {
		new_items = realloc(cv->items, cv->allocated + (50 * sizeof(char *)));
		if (new_items == NULL) {
			return (-1);
		} else {
			cv->items = new_items;
			cv->allocated += 50;
		}
	}

	return (0);
}

static inline int cvec_push(cvec_t cv, const char *item) {
	if (cv->length == SIZE_MAX) return (-1);
	if (cv->length + 1 >= cv->allocated) {
		if (cvec_resize(cv, cv->allocated + 50) < 0) return (-1);
	}
	cv->items[cv->length] = strdup(item);
	if (cv->items[cv->length] == NULL) {
		return (-1);
	} else {
		cv->items[cv->length + 1] = NULL;
		cv->length++;
		return (0);
	}
}

static inline char * cvec_get(cvec_t cv, size_t index) {
	return cv->items[index];
}

static inline int cvec_set(cvec_t cv, size_t index, char *val) {
	char *val2;
	if (index < cv->length) return -1;
	val2 = strdup(val);
	if (!val2) return -1;
	if (cv->items[index]) free(cv->items[index]);
	cv->items[index] = val2;
	return (0);
}

static inline void cvec_free(cvec_t cv) {
	int i;

	if (cv == NULL) return;

	for (i = 0; i < cv->length; i++) {
		free(cv->items[i]);
	}
	free(cv->items);
	free(cv);
}

static inline char ** cvec_to_array(cvec_t cv) {
	return (cv->items);
}

static inline void cvec_debug(cvec_t cv) {
	int i;

	fprintf(stderr, "items: %zu\n", cv->length);
	for (i = 0; i < cv->length; i++) {
		fprintf(stderr, "  %d = %s\n", i, cv->items[i]);
	}
}

static inline size_t cvec_length(cvec_t const cv) {
	return cv->length;
}

static inline cvec_t cvec_dup(cvec_t cv)
{
	cvec_t cv2;

	cv2 = cvec_new();
	if (cv2 == NULL) return NULL;
	for (int i = 0; i < cv->length; i++) {
		if (cvec_push(cv2, cv->items[i]) < 0) {
			cvec_free(cv2);
			return NULL;
		}
	}
	return cv2;
}

#endif /* __CVEC_H */
