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

static int
_db_setup_volatile(void)
{
	char path[PATH_MAX];
	int rv;

	rv = snprintf((char *)&path, sizeof(path),  "%s/volatile.sql", compile_time_option.datarootdir);
	if (rv >= (int)sizeof(path) || rv < 0) {
			printlog(LOG_ERR, "snprintf failed");
			return (-1);
	}

	rv = db_exec_path(path);
	if (rv < 0) {
		printlog(LOG_ERR, "Error executing SQL from %s", path);
		return (-1);
	}

	return (0);
}

static int
_db_create_volatile(const char *dbfile)
{
	sqlite3 *tmpconn;
	int rv;

	rv = sqlite3_open_v2(dbfile, &tmpconn, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rv != SQLITE_OK) {
		//FIXME:printlog(LOG_ERR, "Error opening %s: %s", path, sqlite3_errmsg(dbh));
		return (-1);
	}
	if (sqlite3_close(tmpconn) != SQLITE_OK)
		return (-1);

	printlog(LOG_DEBUG, "created an empty %s", dbfile);
	return (0);
}

static int
_db_attach_volatile(const char *path)
{
	char sql[PATH_MAX+1024];
	int rv;

	rv = snprintf((char *)&sql, sizeof(sql), 
		"ATTACH DATABASE '%s' AS 'volatile';"
		"PRAGMA volatile.synchronous = OFF;",
		 path); //WARN: injection
	if (rv >= (int)sizeof(sql) || rv < 0) {
			printlog(LOG_ERR, "snprintf failed");
			return (-1);
	}

	if (db_exec(dbh, sql) < 0) {
		printlog(LOG_ERR, "Error attaching volatile database");
		return (-1);
	}

	printlog(LOG_DEBUG, "attached %s as volatile", path);
	return (0);
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
db_open(const char *path, int flags)
{
	char volatile_dbpath[PATH_MAX];
	int rv, sqlite_flags;

	rv = snprintf((char *)&volatile_dbpath, sizeof(volatile_dbpath),  "%s/jobd/volatile.db",
			compile_time_option.rundir);
	if (rv >= (int)sizeof(volatile_dbpath) || rv < 0) {
			printlog(LOG_ERR, "snprintf failed");
			return (-1);
	}

	if (dbh) {
		printlog(LOG_ERR, "database is already open");
		return (-1);
	}

	if (!path)
		path = db_default.dbpath;
	
	sqlite_flags = SQLITE_OPEN_READWRITE;

	rv = sqlite3_open_v2(path, &dbh, sqlite_flags, NULL);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "Error opening %s: %s", path, sqlite3_errmsg(dbh));
		return (-1);
	}
	printlog(LOG_DEBUG, "opened %s with flags %d", path, sqlite_flags);

	//KLUDGE: should refactor volatile out of db_open() entirely
	if (flags & DB_OPEN_NO_VOLATILE) {
		/* NOOP */
	} else if (access(volatile_dbpath, F_OK) == 0) {
		if (_db_attach_volatile(volatile_dbpath) < 0)
			return (-1);
	} else if (flags & DB_OPEN_CREATE_VOLATILE) {
		if (_db_create_volatile(volatile_dbpath) < 0) {
			unlink(volatile_dbpath);
			return (-1);
		}

		if (_db_attach_volatile(volatile_dbpath) < 0)
			return (-1);
	
		if (_db_setup_volatile() < 0)
			return (-1);
	} else {
		printlog(LOG_ERR, "unable to open volatile.db");
		return (-1);
	}

	dbpath = strdup(path);
	if (!dbpath) abort(); //FIXME

	return (0);
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
		db_log_error(rv);
		return (-1);
	}
}

int
db_create(const char *path, const char *schemapath)
{
	int rv, flags;
	
	if (dbh) {
		printlog(LOG_ERR, "database is already open");
		return (-1);
	}

	if (!path)
		path = db_default.dbpath;
	if (!schemapath)
		schemapath = db_default.schemapath;
	
	flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

	rv = sqlite3_open_v2(path, &dbh, flags, NULL);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "Error creating %s: %s", path, sqlite3_errmsg(dbh));
		dbh = NULL;
		return (-1);
	}
	printlog(LOG_INFO, "opened %s", path);

	rv = db_exec_path(schemapath);
	if (rv < 0) {
		printlog(LOG_ERR, "Error executing SQL from %s", schemapath);
		return (-1);
	}

	return (0);
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
db_exec_path(const char *path)
{
	char *sql;
	struct stat sb;
	int rv, fd;
	ssize_t bytes;

	if (!dbh) {
		printlog(LOG_ERR, "database is not open");
		return (-1);
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		printlog(LOG_ERR, "open of %s: %s", path, strerror(errno));
		return (-1);
	}

	rv = fstat(fd, &sb);
	if (rv < 0) {
		printlog(LOG_ERR, "fstat: %s", strerror(errno));
		close(fd);
		return (-1);
	}

	sql = malloc(sb.st_size + 1);
	if (!sql) {
		printlog(LOG_ERR, "malloc: %s", strerror(errno));
		close(fd);
		return (-1);
	}

	bytes = read(fd, sql, sb.st_size);
	if (bytes < 0 || bytes != sb.st_size) {
		printlog(LOG_ERR, "read: %s", strerror(errno));
		close(fd);
		free(sql);
		return (-1);	
	}
	sql[sb.st_size] = '\0';

	close(fd);

	rv = db_exec(dbh, sql);
	free(sql);

	return (rv);
}

int
db_select_into_string_array(struct string_array *strarr, sqlite3_stmt *stmt)
{
	int rv;
	char *val;

	for (;;) {
		rv = sqlite3_step(stmt);
		if (rv == SQLITE_DONE)
			break;
		if (rv != SQLITE_ROW)
			goto db_err;

		val = strdup((char *)sqlite3_column_text(stmt, 0));
		if (!val)
			goto os_err;

		if (string_array_push_back(strarr, val) < 0)
			goto err_out;
	}
	return (0);

os_err:
	printlog(LOG_ERR, "OS error: %s", strerror(errno));
	goto err_out;

db_err:
	db_log_error(rv);

err_out:
	sqlite3_finalize(stmt);
	return (-1);
}

int
db_get_id(int64_t *result, const char *sql, const char *fmt, ...)
{
	va_list args;
	sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK) {
		printlog(LOG_ERR, "prepare failed");
		stmt = NULL;
		goto err_out;
	}

	va_start(args, fmt);
	if (db_statement_bind(stmt, fmt, args) < 0) {
		printlog(LOG_ERR, "error binding statement");
		va_end(args);
		goto err_out;
	}
	va_end(args);

	int rv = sqlite3_step(stmt);
	if (rv == SQLITE_ROW) {
		*result = sqlite3_column_int64(stmt, 0);
	} else if (rv == SQLITE_DONE) {
		*result = INVALID_ROW_ID;
	} else {
		goto err_out;
	}

	sqlite3_finalize(stmt);
	return (0);

err_out:
	*result = INVALID_ROW_ID;
	sqlite3_finalize(stmt);
	return (-1);
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
