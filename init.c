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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/uio.h>
#endif

static int
mount_devfs(void)
{
#ifdef __FreeBSD__
    char errmsg[255];

    struct iovec iov[6] = {
            {.iov_base = "fstype", .iov_len = sizeof("fstype")},
            {.iov_base = "devfs", .iov_len = sizeof("devfs")},
            {.iov_base = "fspath", .iov_len = sizeof("fspath")},
            {.iov_base = "/dev", .iov_len = sizeof("/dev")},
            {.iov_base = "errmsg", .iov_len = sizeof("errmsg")},
            {.iov_base = &errmsg, .iov_len = sizeof(errmsg)}
    };

    if (nmount(iov, 6, 0) < 0) {
            syslog(LOG_ERR, "nmount(2) of /dev: %s: %s", strerror(errno), (char*)errmsg);
            return (-1);
    }
#endif
    return (0);
}

static int
mount_tmpfs_rundir(void)
{
#ifdef __FreeBSD__
    char errmsg[255];

    struct iovec iov[8] = {
            {.iov_base = "fstype", .iov_len = sizeof("fstype")},
            {.iov_base = "tmpfs", .iov_len = sizeof("tmpfs")},
            {.iov_base = "from", .iov_len = sizeof("from")},
            {.iov_base = "tmpfs", .iov_len = sizeof("tmpfs")},
            {.iov_base = "errmsg", .iov_len = sizeof("errmsg")},
            {.iov_base = &errmsg, .iov_len = sizeof(errmsg)},
            {.iov_base = "fspath", .iov_len = sizeof("fspath")},
            {.iov_base = "/run", .iov_len = sizeof("/run")}};

    if (kldload("/boot/kernel/tmpfs.ko") < 0) {
            if (errno != EEXIST) {
                    syslog(LOG_ERR, "kldload(2): %s", strerror(errno));
                    return (-1);
            }
    }

    if (nmount(iov, 8, 0) < 0) {
            syslog(LOG_ERR, "nmount(2) of /run: %s: %s", strerror(errno), (char*)errmsg);
            return (-1);
    }

#else
    //TODO: Linux stuff
#endif

    (void) mkdir("/run/jobd", 0755);

    return (0);
}

int
main(int argc, char *argv[])
{
    /* TODO: process arguments */
    (void) argc;
    (void) argv;

    openlog("init", LOG_CONS, LOG_AUTH);
    if (setsid() < 0)
        syslog(LOG_WARNING, "setsid: %s", strerror(errno));
#ifdef setlogin
    if (setlogin("root") < 0)
           syslog(LOG_WARNING, "setlogin: %s", strerror(errno));
#endif

    if (mount_devfs() < 0)
        syslog(LOG_CRIT, "unable to mount /dev");

    if (mount_tmpfs_rundir() < 0)
        syslog(LOG_CRIT, "unable to mount /run");

    if (execl("/sbin/jobd", "/sbin/jobd", NULL) < 0)
        syslog(LOG_CRIT, "execl: %s", strerror(errno));

    exit(EXIT_FAILURE);
}

