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

#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include "../vendor/FreeBSD/sys/queue.h"
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include "config.h"
#include "calendar.h"
#include "log.h"
#include "manager.h"
#include "manifest.h"
#include "options.h"
#include "job.h"
#include "pidfile.h"
#include "socket.h"
#include "timer.h"
#include "uset.h"

FILE *logfile;

struct launchd_options options;

static struct {
	int 	kq;				/* kqueue(2) descriptor */
	struct pidfh *pfh;
} state;

void usage() 
{
	printf("todo: usage\n");
}

static void setup_job_dirs()
{
	char *buf;

	if (asprintf(&options.watchdir, "%s/new", options.pkgstatedir) < 0) abort();
	if (asprintf(&options.activedir, "%s/cur", options.pkgstatedir) < 0) abort();

	if (asprintf(&buf, "/bin/mkdir -p %s %s",
			options.watchdir, options.activedir) < 0) abort();
	if (system(buf) < 0) abort();
	free(buf);

	if (getuid() != 0 && getenv("HOME")) {
		if (asprintf(&buf, "/bin/mkdir -p %s/.launchd/agents", getenv("HOME")) < 0) abort();
		if (system(buf) < 0) abort();
		free(buf);
	}

	/* Clear any record of active jobs that may be leftover from a previous program crash */
	if (asprintf(&buf, "/bin/rm -f %s/* %s/*", options.activedir, options.watchdir) < 0) abort();
	if (system(buf) < 0) abort();
	free(buf);
}

static void do_shutdown()
{
	pidfile_remove(state.pfh);
	free(options.pkgstatedir);
	free(options.pidfile);
}

static void setup_signal_handlers()
{
	const int signals[] = {SIGHUP, SIGUSR1, SIGCHLD, SIGINT, SIGTERM, 0};
	int i;
	struct kevent kev;

	for (i = 0; signals[i] != 0; i++) {
		if (signal(signals[i], SIG_IGN) == SIG_ERR)
			abort();
		EV_SET(&kev, signals[i], EVFILT_SIGNAL, EV_ADD, 0, 0,
				&setup_signal_handlers);
		if (kevent(state.kq, &kev, 1, NULL, 0, NULL) < 0)
			abort();
	}
}

static void main_loop()
{
	struct kevent kev;

	for (;;) {
		if (kevent(state.kq, NULL, 0, &kev, 1, NULL) < 1) {
			if (errno == EINTR) {
				continue;
			} else {
				log_errno("kevent");
				abort();
			}
		}
		if ((void *)kev.udata == &setup_signal_handlers) {
			switch (kev.ident) {
			case SIGHUP:
				manager_update_jobs();
				break;
			case SIGUSR1:
				manager_write_status_file();
				break;
			case SIGCHLD:
				manager_reap_child(kev.ident, kev.data);
				break;
			case SIGINT:
			case SIGTERM:
				log_notice("caught signal %u, exiting", (unsigned int)kev.ident);
				do_shutdown();
				exit(0);
				break;
			default:
				log_error("caught unexpected signal");
			}
		} else if (kev.filter == EVFILT_PROC) {
			(void) manager_reap_child(kev.ident, kev.data);
		} else if ((void *)kev.udata == &setup_socket_activation) {
			if (socket_activation_handler() < 0) abort();
		} else if ((void *)kev.udata == &setup_timers) {
			if (timer_handler() < 0) abort();
		} else if ((void *)kev.udata == &calendar_init) {
			if (calendar_handler() < 0) abort();
		} else {
			log_warning("spurious wakeup, no known handlers");
		}
	}
}

static inline void setup_logging()
{
       char path[PATH_MAX + 1];
       int rv;

       if (options.daemon) {
	       if (getuid() == 0) {
                rv = snprintf(path, sizeof(path), "/var/log/launchd.log");
	       } else {
                rv = snprintf(path, sizeof(path), 
                        "%s/.launchd/launchd.log", getenv("HOME"));
	       }
       } else {
           rv = snprintf(path, sizeof(path), "/dev/stdout");
       }
       if (rv < 0) {
           log_errno("snprintf(3)");
           abort();
       }
       openlog("launchd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
}

void create_pid_file()
{
	pid_t otherpid;

	state.pfh = pidfile_open(options.pidfile, 0600, &otherpid);
	if (state.pfh == NULL) {
		if (errno == EEXIST) {
			errx(EXIT_FAILURE, "Daemon already running, pid: %jd.",
					(intmax_t) otherpid);
		}
		errx(EXIT_FAILURE, "Cannot open or create pidfile");
	}
}

int
main(int argc, char *argv[])
{
	int c;

	/* Sanitize environment variables */
	if ((getuid() != 0) && (access(getenv("HOME"), R_OK | W_OK | X_OK) < 0)) {
		fputs("Invalid value for the HOME environment variable\n", stderr);
		exit(1);
	}

	options.daemon = true;
	options.log_level = LOG_DEBUG;
	if (getuid() == 0) {
		if (asprintf(&options.pkgstatedir, PKGSTATEDIR) < 0) abort();
		if (asprintf(&options.pidfile, "/var/run/launchd.pid") < 0) abort();
	} else {
		if (asprintf(&options.pkgstatedir, "%s/.launchd/run", getenv("HOME")) < 0) abort();
		if (asprintf(&options.pidfile, "%s/launchd.pid", options.pkgstatedir) < 0) abort();
	}

	setup_job_dirs();

	while ((c = getopt(argc, argv, "fv")) != -1) {
			switch (c) {
			case 'f':
					options.daemon = false;
					break;
			case 'v':
					options.log_level++;
					break;
			default:
					usage();
					break;
			}
	}

	create_pid_file();

	if (options.daemon && daemon(0, 0) < 0) {
		fprintf(stderr, "Unable to daemonize");
		pidfile_remove(state.pfh);
		exit(EX_OSERR);
	}

	pidfile_write(state.pfh);

	if ((state.kq = kqueue()) < 0) abort();
	manager_init(state.kq);
	setup_logging();
	setup_signal_handlers();
	setup_socket_activation(state.kq);
	if (setup_timers(state.kq) < 0) abort();
	if (calendar_init(state.kq) < 0) abort();
	manager_update_jobs();
	main_loop();

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
