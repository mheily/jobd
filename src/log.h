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
#include <string.h>
#include <syslog.h>

/* Logging */
extern FILE *logfile;

#define _log_all(level, format,...) do {				\
	if (logfile != NULL) {						\
		fprintf(stdout, "%s(%s:%d): "format"\n",		\
			__func__, __FILE__, __LINE__, ## __VA_ARGS__);	\
	} else {							\
		syslog(level, "%s(%s:%d): "format"\n",			\
			__func__, __FILE__, __LINE__, ## __VA_ARGS__);	\
	}								\
} while (0)

#define log_error(format,...) _log_all(LOG_ERR, "**ERROR** "format, ## __VA_ARGS__)
#define log_warning(format,...) _log_all(LOG_WARNING, "WARNING: "format, ## __VA_ARGS__)
#define log_notice(format,...) _log_all(LOG_NOTICE, format, ## __VA_ARGS__)
#define log_info(format,...) _log_all(LOG_INFO, format, ## __VA_ARGS__)
#if !defined(NDEBUG)
#define log_debug(format,...) _log_all(LOG_DEBUG, format, ## __VA_ARGS__)
#else
#define log_debug(format,...) do { (void)format; } while (0)
#endif
#define log_errno(format,...) _log_all(LOG_ERR, format": errno=%d (%s)", ## __VA_ARGS__, errno, strerror(errno))

void log_freopen(FILE *new_logfile);

/* Emulate the <err.h> macros but use our own logging facility */
#define err(code, format, ...) do { \
	log_errno("**FATAL ERROR** "format, ## __VA_ARGS__); \
	exit((code)); \
} while (0)

#define errx(code, format, ...) do { \
	_log_all(LOG_ERR, "**FATAL ERROR** "format, ## __VA_ARGS__); \
	exit((code)); \
} while (0)

#endif /* LOG_H_ */
