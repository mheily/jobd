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

char databasedir[PATH_MAX];

int database_init()
{
	char path[PATH_MAX];

	if (getuid() == 0) {
		path_sprintf(&databasedir, "%s", DATABASEDIR);
	} else {
		path_sprintf(&databasedir, "%s/.launchd/cfg", getenv("HOME"));
	}
	mkdir_idempotent(path, 0700);

	return 0;
}

int database_set(const char *property, const char *value)
{
	return -1;
}

int database_get(char **value, const char *property)
{
	return -1;
}

int database_subscribe(const char *property)
{
	return -1;
}
