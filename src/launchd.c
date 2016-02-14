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
#include <sys/wait.h>
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

static void signal_handler(int signum) {
	(void) signum;
}

static void setup_signal_handlers()
{
	const int signals[] = {SIGHUP, SIGUSR1, SIGCHLD, SIGINT, SIGTERM, 0};
	int i;
	struct kevent kev;

	for (i = 0; signals[i] != 0; i++) {
		EV_SET(&kev, signals[i], EVFILT_SIGNAL, EV_ADD, 0, 0,
				&setup_signal_handlers);
		if (kevent(state.kq, &kev, 1, NULL, 0, NULL) < 0)
			abort();
		if (signal(signals[i], signal_handler) == SIG_ERR)
			abort();
	}
}

static void reap_child() {
	pid_t pid;
	int status;
	job_t job;

	pid = waitpid(-1, &status, WNOHANG);
	if (pid < 0) {
		if (errno == ECHILD) return;
		log_errno("waitpid");
		abort();
	} else if (pid == 0) {
		return;
	}

	job = manager_get_job_by_pid(pid);
	if (!job) {
		log_error("child pid %d exited but no job found", pid);
		return;
	}

	if (job->state == JOB_STATE_KILLED) {
		/* The job is unloaded, so nobody cares about the exit status */
		manager_free_job(job);
		return;
	}

	if (job->jm->start_interval > 0) {
		job->state = JOB_STATE_WAITING;
	} else {
		job->state = JOB_STATE_EXITED;
	}
	if (WIFEXITED(status)) {
		job->last_exit_status = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		job->last_exit_status = -1;
		job->term_signal = WTERMSIG(status);
	} else {
		log_error("unhandled exit status");
	}
	log_debug("job %d exited with status %d", job->pid,
			job->last_exit_status);
	job->pid = 0;
	return;
}

static void load_jobs(const char *path)
{
	char *buf;
	if (asprintf(&buf, "/usr/bin/find %s -type f -exec cp {} %s \\;", path, options.watchdir) < 0) abort();
	log_debug("loading: %s", buf);
	if (system(buf) < 0) {
		log_errno("system");
		abort();
	}
	free(buf);
}

static void load_all_jobs()
{
	char *buf;

	if (getuid() == 0) {
		load_jobs("/usr/share/launchd/daemons");
		load_jobs("/etc/launchd/daemons");
	} else {
		load_jobs("/usr/share/launchd/agents");
		load_jobs("/etc/launchd/agents");
		/* FIXME: proper error handling needed here */
		if (!getenv("HOME")) abort();
		if (asprintf(&buf, "%s/.launchd/agents", getenv("HOME")) < 0) abort();
		if (access(buf, F_OK) < 0) abort();
		load_jobs(buf);
		free(buf);
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
		if (kev.udata == &setup_signal_handlers) {
			switch (kev.ident) {
			case SIGHUP:
				manager_update_jobs();
				break;
			case SIGUSR1:
				manager_write_status_file();
				break;
			case SIGCHLD:
				reap_child();
				break;
			case SIGINT:
			case SIGTERM:
				log_notice("caught signal %lu, exiting", kev.ident);
				do_shutdown();
				exit(0);
				break;
			default:
				log_error("caught unexpected signal");
			}
		} else if (kev.udata == &setup_socket_activation) {
			if (socket_activation_handler() < 0) abort();
		} else if (kev.udata == &setup_timers) {
			if (timer_handler() < 0) abort();
		} else if (kev.udata == &calendar_init) {
			if (calendar_handler() < 0) abort();
		} else {
			log_warning("spurious wakeup, no known handlers");
		}
	}
}

static inline void setup_logging()
{
       char *path = NULL;

       if (options.daemon) {
	       if (getuid() == 0) {
		       path = strdup("/var/log/launchd.log");
	       } else {
		       asprintf(&path, "%s/.launchd/launchd.log", getenv("HOME"));
	       }
       } else {
	       path = strdup("/dev/stdout");
       }
       if (log_open(path) < 0) abort();
       free(path);
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
	manager_init();
	setup_logging();
	setup_signal_handlers();
	setup_socket_activation(state.kq);
	if (setup_timers(state.kq) < 0) abort();
	if (calendar_init(state.kq) < 0) abort();
	load_all_jobs();
	manager_update_jobs();
	main_loop();

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
