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

#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdbool.h>
#include <sqlite3.h>

#define DB_OPEN_READONLY 0x01
#define DB_OPEN_CREATE_VOLATILE 0x10

#define db_check_result(_rv, _stmt) \
	if ((_rv) != SQLITE_OK) { \
		printlog(LOG_ERR, "Database error: %s", sqlite3_errmsg(dbh)); \
		sqlite3_finalize(_stmt); \
		(_stmt) = NULL; \
		return (-1); \
	}

#define db_log_error(_rv) printlog(LOG_ERR, "database error: %d", _rv)

struct string_array;

extern sqlite3 *dbh;

int db_init(void);
int db_create(const char *, const char *);
int db_open(const char *, int);
bool db_exists(void);
int db_exec(sqlite3 *, const char *sql);
int db_exec_path(const char *);

int db_select_into_string_array(struct string_array *strarr, sqlite3_stmt *stmt);

#endif /* _DATABASE_H */
