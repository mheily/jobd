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

#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <memory.h>

#include "logger.h"

FILE *logger_fh;
int logger_use_syslog;
int logger_verbose;

int
logger_open(const char *path)
{
	int fd;
	FILE *nfh;

	fd = open(path, O_WRONLY|O_CREAT, 0600);
	if (fd < 0)
		return (-1);
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
		return (-2);
	nfh = fdopen(fd, "a");
	if (!nfh) {
		close(fd);
		return (-3);
	}
	if (logger_fh)
		fclose(logger_fh);
	logger_fh = nfh;
	return (0);
}

int
logger_init(const char *logfile)
{
	int fd;

	/* TODO: add support for syslog */
	logger_use_syslog = 1;
	openlog("jobd", LOG_CONS, LOG_AUTH);
	//logger_use_syslog = 0;

	if (logfile) {
	    return logger_open(logfile);
	} else {
		fd = dup(STDERR_FILENO);
		if (fd < 0)
			return (-1);
		if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
			//TODO: printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));
			return (-1);
		}
		logger_fh = fdopen(fd, "w");
		if (!logger_fh) {
			close(fd);
			return (-1);
		}
	}

	return (0);
}

void
logger_set_verbose(int flag)
{
	logger_verbose = flag;
}

static inline char
_level_code(int level)
{
    switch (level) {
        case LOG_ERR: return 'E';
        case LOG_WARNING: return 'W';
        case LOG_INFO: return 'I';
        case LOG_DEBUG: return 'D';
        default: return 'U'; /* Unknown */
    }
}

int __attribute__((format(printf, 2, 3)))
logger_append(int level, const char *format, ...)
{
	va_list args;
    va_start(args, format);
	if (logger_use_syslog) {
	    va_list syslog_args;
	    va_copy(syslog_args, args);
		vsyslog(level, format, syslog_args);
		va_end(syslog_args);
	}
    if (logger_verbose || level != LOG_DEBUG) {
        /* Generate our own timestamp */
        time_t t;
        struct tm *tms;
        char tbuf[32];
        t = time(NULL);
        tms = localtime(&t);
        if (strftime(tbuf, sizeof(tbuf), "%a, %d %b %Y %T %z", tms) == 0)
            strncpy(tbuf, "Unknown timeval", sizeof(tbuf));

        /* Write to the log */
        const char *term = getenv("TERM");
        if (term && level == LOG_ERR)
            fprintf(logger_fh, "\033[0;31m");
        fprintf(logger_fh, "%c %s %d ", _level_code(level), tbuf, getpid());
        vfprintf(logger_fh, format, args);
        if (term && level == LOG_ERR)
            fprintf(logger_fh, "\033[0m");
        fflush(logger_fh);
    }
	va_end(args);
	if (level <= LOG_ERR)
		return -1;
	else
		return 0;
}
