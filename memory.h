/*
 * Copyright (c) 2019 Mark Heily <mark@heily.com>
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

#ifndef _JOBD_MEMORY_H
#define _JOBD_MEMORY_H

#include <stdlib.h>

#define CLEANUP_STR __attribute__((__cleanup__(string_free)))

static inline void string_free(char **strp) {
	free(*strp);
	*strp = NULL;
}

struct sqlite3_stmt;

#define CLEANUP_STMT __attribute__((__cleanup__(db_statement_free)))
void db_statement_free(struct sqlite3_stmt **stmt);

#endif
