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

#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>
#include <syslog.h>

#define printlog(level, format,...) do {				\
	if (logger_verbose || level != LOG_DEBUG) {			\
		fprintf(logger_fh, "%d %s(%s:%d): "format"\n",	\
level, __func__, __FILE__, __LINE__, ## __VA_ARGS__); 	\
		fflush(logger_fh);								\
	}													\
} while (0)

extern FILE *logger_fh;

int logger_verbose;
FILE *logger_fh;

int logger_init(void);
int logger_open(const char *);
void logger_set_verbose(int);

#endif /* _LOGGER_H */
