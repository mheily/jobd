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

#include <assert.h>
#include <errno.h>
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
FILE *stderr_fh;

struct {
    int initialized:1;
    int verbose:1;
    int syslog_appender:1;
    int file_appender:1;
    int stderr_appender:1;
} status_flags;

/* This is called when there is an error setting up logging. */
static int __attribute__((format(printf, 2, 3)))
_fallback_printlog(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fflush(stderr);
    va_end(args);
    if (level <= LOG_ERR)
        return -1;
    else
        return 0;
}

int
logger_open(const char *path)
{
    int fd;
    FILE *nfh;

    fd = open(path, O_WRONLY|O_CREAT, 0600);
    if (fd < 0)
        return _fallback_printlog(LOG_ERR, "open(2) of %s: %s",
                path, strerror(errno));
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
        return _fallback_printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));
    nfh = fdopen(fd, "a");
    if (!nfh) {
        (void) close(fd);
        return _fallback_printlog(LOG_ERR, "fdopen(3): %s", strerror(errno));
    }
    if (logger_fh)
        fclose(logger_fh);
    logger_fh = nfh;
    return 0;
}

int
logger_add_syslog_appender(const char *ident, int option, int facility)
{
    assert(status_flags.initialized);
    assert(!status_flags.syslog_appender);
    openlog(ident, option, facility);
    status_flags.syslog_appender = 1;
    return 0;
}

int
logger_add_file_appender(const char *path)
{
    assert(status_flags.initialized);
    assert(!status_flags.file_appender);
    if (logger_open(path) < 0)
        return _fallback_printlog(LOG_ERR, "error opening logfile");
    status_flags.file_appender = 1;
    return 0;
}

int
logger_add_stderr_appender(void)
{
    assert(status_flags.initialized);
    assert(!status_flags.stderr_appender);
    int fd = dup(STDERR_FILENO);
    if (fd < 0)
        return _fallback_printlog(LOG_ERR, "dup(2): %s", strerror(errno));
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
        return _fallback_printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));
    stderr_fh = fdopen(fd, "a");
    if (!stderr_fh) {
        close(fd);
        return _fallback_printlog(LOG_ERR, "fdopen(2): %s", strerror(errno));
    }
    status_flags.stderr_appender = 1;
    return 0;
}

int
logger_redirect_file_descriptor(int oldfd, const char *path, int flags, mode_t mode)
{
    int newfd;

    newfd = open(path, flags, mode);
    if (newfd < 0)
        return printlog(LOG_ERR, "open(2) of %s: %s", path, strerror(errno));

    if (dup2(newfd, oldfd) < 0) {
        printlog(LOG_ERR, "dup2(2): %s", strerror(errno));
        (void) close(newfd);
        return -1;
    }
    if (close(newfd) < 0)
        return printlog(LOG_ERR, "close(2): %s", strerror(errno));

    return 0;
}

int
logger_init(void)
{
    if (status_flags.initialized) {
        memset(&status_flags, 0, sizeof(status_flags));
        (void)fclose(stderr_fh);
        stderr_fh = NULL;
        (void)fclose(logger_fh);
        logger_fh = NULL;
    }
    status_flags.initialized = 1;
    return 0;
}

void logger_shutdown(void)
{
    if (status_flags.initialized) {
        if (status_flags.stderr_appender) {
            fclose(stderr_fh);
        }
        status_flags.initialized = 0;
    }
}

void
logger_set_verbose(int flag)
{
    status_flags.verbose = flag;
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
    assert(status_flags.initialized);
    if (status_flags.syslog_appender) {
        va_list syslog_args;
        va_copy(syslog_args, args);
        vsyslog(level, format, syslog_args);
        va_end(syslog_args);
    }
    if (status_flags.verbose || level != LOG_DEBUG) {
        if (status_flags.stderr_appender) {
            va_list args2;
            va_copy(args2, args);
            vfprintf(stderr_fh, format, args2);
            fflush(stderr_fh);
            va_end(args2);
        }
        if (status_flags.file_appender) {
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
    }
    va_end(args);
    if (level <= LOG_ERR)
        return -1;
    else
        return 0;
}
