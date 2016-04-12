/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
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

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "database.h"
#include "util.h"

static char * path_to_property(const char *property);

static char databasedir[PATH_MAX];

int database_init()
{
	if (getuid() == 0) {
		path_sprintf(&databasedir, "%s", DATABASEDIR);
	} else {
		path_sprintf(&databasedir, "%s/.launchd/cfg", getenv("HOME"));
	}
	mkdir_idempotent(databasedir, 0700);

	return 0;
}

int database_set(const char *property, const char *value)
{
	char *path;
	int fd;
	size_t value_len;
	ssize_t written;

	if (!property || !value) {
		log_error("null value passed in");
		return -1;
	}

	path = path_to_property(property);
	fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_EXLOCK, 0755);
	if (fd < 0) {
		log_errno("open(2) of %s", path);
		return -1;
	}

	value_len = strlen(value) + 1;
	written = write(fd, value, value_len);
	if (written < value_len) {
		log_errno("write(2) of %s", path);
	}

	(void) close(fd);

	return 0;
}

int database_get(char **value, const char *property)
{
	char *path;
	int fd;
	ssize_t bytes;
     	struct stat sb;

	if (!property || !value) {
		log_error("null value passed in");
		return -1;
	}

	path = path_to_property(property);
	fd = open(path, O_RDONLY | O_EXLOCK);
	if (fd < 0) {
		log_errno("open(2) of %s", path);
		return -1;
	}

     	if (fstat(fd, &sb) < 0) {
		log_errno("fstat(2) of %s", path);
		(void) close(fd);
		return -1;
	}

     	if (sb.st_size == 0 || sb.st_size >= INT_MAX) {
		log_error("invalid size of %s", path);
		(void) close(fd);
		return -1;
	}

	*value = malloc(sb.st_size);
	if (!value) {
		log_errno("malloc(3)");
		(void) close(fd);
		return -1;
	}

	bytes = read(fd, *value, sb.st_size);
	if (bytes < sb.st_size) {
		log_errno("read(2) of %s", path);
		(void) close(fd);
		return -1;
	}

	(void) close(fd);

	return 0;
}

int database_subscribe(const char *property)
{
	/* TODO */
	return -1;
}

static char * path_to_property(const char *property) {
	static char buf[PATH_MAX]; /* Not threadsafe */
	path_sprintf(&buf, "%s/%s", databasedir, property);
	return ((char *) &buf);
}
