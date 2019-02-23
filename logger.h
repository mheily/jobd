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

#define printlog(level, format,...) logger_append(level, "%s(%s:%d): "format"\n", __func__, __FILE__, __LINE__, ## __VA_ARGS__)

extern FILE *logger_fh;
extern int logger_use_syslog;

FILE *logger_fh;

void logger_enable_syslog(const char *ident, int option, int facility);
int logger_init(const char *logfile);
int logger_open(const char *);
int __attribute__((format(printf, 2, 3))) logger_append(int level, const char *format, ...);
void logger_set_verbose(int);

#endif /* _LOGGER_H */
