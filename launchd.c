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
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include "vendor/FreeBSD/sys/queue.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "manager.h"
#include "manifest.h"
#include "options.h"
#include "job.h"
#include "socket.h"
#include "timer.h"
#include "uset.h"

FILE *logfile;

struct launchd_options options;

static struct {
	int 	kq;				/* kqueue(2) descriptor */
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
	char *path;
	if (asprintf(&path, "%s/launchd.pid", options.pkgstatedir) < 0) abort();
	(void) unlink(path);
	free(path);
	free(options.pkgstatedir);
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

static bool pidfile_is_stale(const char *path) {
	char *buf;

	if (asprintf(&buf, "kill -0 `cat %s`", path) < 0) abort();
	if (system(buf) == 0) {
		return false;
	} else {
		log_warning("detected a stale pidfile");
		return true;
	}
}

static void create_pid_file()
{
	char *path, *buf;
	int fd;
	ssize_t len;

	if (asprintf(&path, "%s/launchd.pid", options.pkgstatedir) < 0) abort();
	if (asprintf(&buf, "%d", getpid()) < 0) abort();
	len = strlen(buf);
retry:
	if ((fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0644)) < 0) {
		if (errno == EEXIST) {
			if (pidfile_is_stale(path)) {
				log_warning("detected a stale pidfile");
				if (unlink(path) < 0) abort();
				goto retry;
			}
			log_error("another instance of launchd is running");
			exit(EX_SOFTWARE);
		} else {
			log_errno("open of %s", path);
			abort();
		}
	}
	if (ftruncate(fd, 0) < 0) abort();
	if (write(fd, buf, len) < len) abort();
	if (close(fd) < 0) abort();
	free(path);
	free(buf);
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
	}

	job = manager_get_job_by_pid(pid);
	if (!job) {
		log_error("child exited but no job found");
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
	char *buf, *cur;
	int i;
	if (getuid() == 0) {
		load_jobs("/usr/share/launchd/daemons");
		load_jobs("/etc/launchd/daemons");
	} else {
		load_jobs("/usr/share/launchd/agents");
		load_jobs("/etc/launchd/agents");
		if (getenv("HOME") && asprintf(&buf, "%s/.launchd/agents", getenv("HOME")) < 0) abort();
		load_jobs(buf);
		free(buf);
	}
}

static void main_loop()
{
	struct kevent kev;
	uset_t new_jobs;

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
		} else {
			log_warning("spurious wakeup, no known handlers");
		}
	}
}

static inline void setup_logging()
{
       char *path = NULL;

       if (getuid() == 0) {
               path = strdup("/var/db/launchd/launchd.log");
       } else {
               asprintf(&path, "%s/.launchd/launchd.log", getenv("HOME"));
       }
       if (log_open(path) < 0) abort();
       free(path);
}

int
main(int argc, char *argv[])
{
	int c;

	options.daemon = true;
	options.log_level = LOG_DEBUG;
	if (getuid() == 0) {
		if (asprintf(&options.pkgstatedir, "/var/db/launchd/run") < 0) abort();
	} else {
		if (asprintf(&options.pkgstatedir, "%s/.launchd/run", getenv("HOME")) < 0) abort();
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

	if (options.daemon && daemon(0, 0) < 0) {
		fprintf(stderr, "Unable to daemonize");
		exit(EX_OSERR);
	}

	if ((state.kq = kqueue()) < 0) abort();
	manager_init();
	setup_logging();
	create_pid_file();
	setup_signal_handlers();
	setup_socket_activation(state.kq);
	if (setup_timers(state.kq) < 0) abort();
	load_all_jobs();
	manager_update_jobs();
	main_loop();

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
