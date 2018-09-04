/*
 * Copyright (c) 2018 Mark Heily <mark@heily.com>
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

#ifndef _ARRAY_H
#define _ARRAY_H

#include <string.h>
#include <inttypes.h>

#define string_array_data(_strarr) ((_strarr)->strp)
#define string_array_len(_strarr) ((_strarr)->pos)

struct string_array {
	uint32_t size, pos;
	char **strp;
};

static inline struct string_array *
string_array_new(void)
{
	struct string_array *strarr = calloc(1, sizeof(*strarr));
	if (!strarr)
		return (NULL);
	strarr->strp = calloc(16, sizeof(char *));
	strarr->size = 16;
	return (strarr);
} 

static inline void
string_array_free(struct string_array *strarr)
{
	uint32_t i;

	if (!strarr)
		return;

	for (i = 0; i < strarr->pos; i++)
		free(strarr->strp[i]);
	free(strarr->strp);
	free(strarr);
} 

static inline int
string_array_get(char **result, struct string_array *strarr, uint32_t index)
{
	if (!strarr || index >= strarr->pos) {
		*result = NULL;
		return (-1);
	} else {
		*result = strarr->strp[index];
		return (0);
	}
}

static inline int
string_array_push_back(struct string_array *strarr, char *item)
{
	if (item == NULL)
		return (-1);
	if (!strarr->size || strarr->pos == (strarr->size - 1)) {
		if (strarr->size > 8096)
			return (-1); // KLUDGE
		uint32_t new_size = strarr->size + 16;
		char **newbuf = calloc(new_size, sizeof(char *));
		if (!newbuf)
			return (-1);
		if (strarr->strp) {
			memcpy(newbuf, strarr->strp, strarr->size);
			free(strarr->strp);
		}
		strarr->strp = newbuf;
		strarr->size = new_size;
	}
	strarr->strp[strarr->pos++] = item;
	return (0);
}

static inline int
string_array_contains(struct string_array *haystack, const char *needle)
{
	uint32_t i;
	if (!haystack || !needle)
		return (0);
	for (i = 0; i < haystack->pos; i++) {
		if (!strcmp(haystack->strp[i], needle))
			return (1);
	}
	return (0);
}

#endif /* _ARRAY_H */