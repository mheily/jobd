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
	if (asprintf(&dbpath, "%s/jmf/repository.db", compile_time_option.localstatedir) < 0)
		dbpath = NULL;
	if (!dbpath) {
		printlog(LOG_ERR, "dbpath: %s", strerror(errno));
		return (-1);
	}

	if (sqlite3_config(SQLITE_CONFIG_LOG, _db_log_callback, NULL) < 0) {
		printlog(LOG_ERR, "error setting log callback");
		return (-1);
	}

	return (0);
}

int
db_open(const char *path, bool readonly)
{
	int rv, flags;
	
	if (dbh) {
		printlog(LOG_ERR, "database is already open");
		return (-1);
	}

	if (!path)
		path = dbpath;
	
	if (readonly)
		flags = SQLITE_OPEN_READONLY;
	else
		flags = SQLITE_OPEN_READWRITE;

	rv = sqlite3_open_v2(path, &dbh, flags, NULL);
	if (rv != SQLITE_OK) {
		//FIXME:printlog(LOG_ERR, "Error opening %s: %s", path, sqlite3_errmsg(dbh));
		return (-1);
	}
	printlog(LOG_DEBUG, "opened %s", path);
	return (0);
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
		path = dbpath;
	
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

int 
db_exec_path(const char *path)
{
	char *sql;
	struct stat sb;
	int rv, fd;
	ssize_t bytes;
	char *errmsg;

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

	printlog(LOG_DEBUG, "Executing SQL: \n\n%s\n--", sql);

	rv = sqlite3_exec(dbh, sql, 0, 0, &errmsg);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "SQL error: %s", errmsg);
		sqlite3_free(errmsg);
	}
	free(sql);

	return (rv == SQLITE_OK ? 0 : -1);
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