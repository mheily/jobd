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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "array.h"
#include "config.h"
#include "logger.h"
#include "database.h"

static struct {
	char *dbpath;
	char *schemapath;
} db_default;

static char *dbpath;
sqlite3 *dbh = NULL;

static void
_db_log_callback(void *unused, int error_code, const char *msg)
{
	(void) unused;
	printlog(LOG_ERR, "sqlite3 error %d: %s", error_code, msg);
}

int
db_init(void)
{
	memset(&db_default, 0, sizeof(db_default));

	if (asprintf(&db_default.dbpath, "%s/repository.db", compile_time_option.localstatedir) < 0)
		db_default.dbpath = NULL;
	if (!db_default.dbpath) {
		printlog(LOG_ERR, "asprintf: %s", strerror(errno));
		return (-1);
	}

	if (asprintf(&db_default.schemapath, "%s/schema.sql", compile_time_option.datarootdir) < 0)
		db_default.schemapath = NULL;
	if (!db_default.schemapath) {
		printlog(LOG_ERR, "asprintf: %s", strerror(errno));
		return (-1);
	}

	if (sqlite3_config(SQLITE_CONFIG_LOG, _db_log_callback, NULL) < 0) {
		printlog(LOG_ERR, "error setting log callback");
		return (-1);
	}

	return (0);
}

int
db_open(const char *path, int flags __attribute__((unused)))
{
    sqlite3 *conn = NULL;

    if (dbh)
        return printlog(LOG_ERR, "database is already open");
    if (!path)
        path = db_default.dbpath;

    int sqlite_flags = SQLITE_OPEN_READWRITE;
    if (sqlite3_open_v2(path, &conn, sqlite_flags, NULL) != SQLITE_OK)
        return printlog(LOG_ERR, "Error opening %s: %s", path, sqlite3_errmsg(dbh));

    dbh = conn;

    return printlog(LOG_DEBUG, "opened %s with flags=%d", path, sqlite_flags);
}

int
db_reopen(void)
{
	int rv;
	if (!dbh) {
		printlog(LOG_ERR, "database is not open");
		return (-1);
	}
	rv = sqlite3_close(dbh);
	if (rv == SQLITE_OK) {
		dbh = NULL;
		return (db_open(dbpath, 0));
	} else {
		return printlog(LOG_ERR, "database error: %s", sqlite3_errstr(rv));
	}
}

int db_close(sqlite3 *conn)
{
    if (conn == NULL)
        conn = dbh;
    if (sqlite3_close(conn) != SQLITE_OK)
        return db_error;
    if (conn == dbh)
        dbh = NULL;
    return 0;
}

int db_checkpoint(sqlite3 *conn)
{
    int x, y;
    if (sqlite3_wal_checkpoint_v2(conn, "main", SQLITE_CHECKPOINT_TRUNCATE, &x, &y) < 0)
        return printlog(LOG_ERR, "unable to checkpoint database");
    return 0;
}

int
db_create(const char *path, const char *schemapath)
{
    sqlite3 *conn = NULL;

    if (!path)
        path = db_default.dbpath;
    if (!schemapath)
        schemapath = db_default.schemapath;

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(path, &conn, flags, NULL) != SQLITE_OK)
        return printlog(LOG_ERR, "Error creating %s: %s", path, sqlite3_errmsg(dbh));

    if (db_exec(conn, "PRAGMA journal_mode=WAL") < 0)
        printlog(LOG_WARNING, "failed to enable WAL; expect bad performance");

    if (db_exec_path(conn, schemapath) < 0) {
        (void) sqlite3_close(conn);
        (void) unlink(path);
        return printlog(LOG_ERR, "Error executing SQL from %s", schemapath);
    }

    if (db_checkpoint(conn) < 0)
        printlog(LOG_WARNING, "unable to checkpoint database");

    return printlog(LOG_INFO, "created an empty repository.db at %s", path);
}

bool
db_exists(void)
{
	if (access(db_default.dbpath, F_OK) < 0)
		return (false);
	else
		return (true);
}

int
db_exec(sqlite3 *conn, const char *sql)
{
	int rv;
	char *errmsg;

	printlog(LOG_DEBUG, "Executing SQL: \n\n%s\n--", sql);

	rv = sqlite3_exec(conn, sql, 0, 0, &errmsg);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "SQL error: %s", errmsg);
		sqlite3_free(errmsg);
	}

	return (rv == SQLITE_OK ? 0 : -1);
}

int 
db_exec_path(sqlite3 *conn, const char *path)
{
	char *sql;
	struct stat sb;
	int rv, fd;
	ssize_t bytes;

	if (!conn)
		return printlog(LOG_ERR, "database is not open");

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return printlog(LOG_ERR, "open of %s: %s", path, strerror(errno));

	rv = fstat(fd, &sb);
	if (rv < 0) {
		close(fd);
		return printlog(LOG_ERR, "fstat: %s", strerror(errno));
	}

	sql = malloc(sb.st_size + 1);
	if (!sql) {
		close(fd);
		return printlog(LOG_ERR, "malloc: %s", strerror(errno));
	}

	bytes = read(fd, sql, sb.st_size);
	if (bytes < 0 || bytes != sb.st_size) {
		close(fd);
		free(sql);
		return printlog(LOG_ERR, "read: %s", strerror(errno));
	}
	sql[sb.st_size] = '\0';

	close(fd);

	rv = db_exec(conn, sql);
	free(sql);

	return rv;
}

int
db_get_id(int64_t *result, const char *sql, const char *fmt, ...)
{
	va_list args;
	sqlite3_stmt CLEANUP_STMT *stmt = NULL;

	*result = INVALID_ROW_ID;

	if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
		return printlog(LOG_ERR, "prepare failed");

	va_start(args, fmt);
	if (db_statement_bind(stmt, fmt, args) < 0) {
		va_end(args);
		return printlog(LOG_ERR, "error binding statement");
	}
	va_end(args);

	switch (sqlite3_step(stmt)) {
		case SQLITE_ROW:
			*result = sqlite3_column_int64(stmt, 0);
			break;
		case SQLITE_DONE:
			*result = INVALID_ROW_ID;
			break;
		default:
			return db_error;
	}

	return 0;
}

int db_statement_bind(sqlite3_stmt *stmt, const char *fmt, va_list args)
{
	int col;
	char *c = (char *)fmt;

	for (col = 1; *c != '\0'; c++) {
		if (*c == 'i') {
			int64_t i = va_arg(args, int64_t);
			if (sqlite3_bind_int64(stmt, col++, i) != SQLITE_OK) {
				printlog(LOG_ERR, "bind_int64 failed");
				return (-1);
			}
		} else if (*c == 's') {
			char *s = va_arg(args, char *);
			if (sqlite3_bind_text(stmt, col++, s, -1, SQLITE_STATIC) != SQLITE_OK) {
				printlog(LOG_ERR, "bind_text failed");
				return (-1);
			}
		} else {
			printlog(LOG_ERR, "invalid format specifier: %c in %s", *c, fmt);
			return (-1);
		}
		++fmt;
	}
	return (0);
}

/* Caller must free result */
int db_query(sqlite3_stmt **result, const char *sql, const char *fmt, ...)
{
	va_list args;
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK) {
		printlog(LOG_ERR, "prepare failed: sql=%s", sql);
		stmt = NULL;
		goto err_out;
	}

	va_start(args, fmt);
	if (db_statement_bind(stmt, fmt, args) < 0) {
		printlog(LOG_ERR, "error binding statement: sql=%s", sql);
		va_end(args);
		goto err_out;
	}
	va_end(args);

	*result = stmt;
	return (0);

err_out:
	*result = NULL;
	sqlite3_finalize(stmt);
	return (-1);
}

void db_statement_free(sqlite3_stmt **stmt)
{
    sqlite3_finalize(*stmt);
    *stmt = NULL;
}

static int
db_trace_callback(unsigned int reason, void *ctx __attribute__((unused)), void *p, void *x)
{
	if (reason == SQLITE_TRACE_STMT) {
		char *xstr = (char *) x;
		if (xstr[0] == '-' && xstr[1] == '-') {
			printlog(LOG_DEBUG, "statement: %s", xstr);
		} else {
			char *expanded = sqlite3_expanded_sql((sqlite3_stmt *) p);
			printlog(LOG_DEBUG, "statement: %s", expanded);
			sqlite3_free(expanded);
		}
	} else if (reason == SQLITE_TRACE_ROW) {
		printlog(LOG_DEBUG, "row returned");
	} else {
		return -1;
	}
	return 0;
}

int db_enable_tracing(void)
{
	if (sqlite3_trace_v2(dbh, SQLITE_TRACE_STMT | SQLITE_TRACE_ROW, db_trace_callback, NULL) != SQLITE_OK)
		return db_error;
	else
		return 0;
}
