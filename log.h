/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
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

#ifndef LOG_H_
#define LOG_H_

#include <errno.h>
#include <stdio.h>
#include <syslog.h>

/* Logging */

extern FILE *logfile;

int log_open(const char *path);

/* Kludgy because this doesn't use syslog(3) yet */
#define _log_all(level, format,...) do { \
	(void)level; \
	fprintf(logfile,                       \
   "%s(%s:%d): "format"\n",                                             \
   __func__, __FILE__, __LINE__, ## __VA_ARGS__); \
   fflush(logfile); \
} while (0)

#define log_error(format,...) _log_all(LOG_ERR, "**ERROR** "format, ## __VA_ARGS__)
#define log_warning(format,...) _log_all(LOG_WARNING, "WARNING: "format, ## __VA_ARGS__)
#define log_notice(format,...) _log_all(LOG_NOTICE, format, ## __VA_ARGS__)
#define log_info(format,...) _log_all(LOG_INFO, format, ## __VA_ARGS__)
#ifdef DEBUG
#define log_debug(format,...) _log_all(LOG_DEBUG, format, ## __VA_ARGS__)
#else
#define log_debug(format,...) do { (void)format; } while (0)
#endif
#define log_errno(format,...) _log_all(LOG_ERR, format": %s", ## __VA_ARGS__, strerror(errno))



#endif /* LOG_H_ */
