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
#include <syslog.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "manifest.h"
#include "job.h"
#include "uset.h"

static struct {
	char *	pkgstatedir;	/* Top-level directory for state data */
	char *	watchdir;		/* Directory to watch for new jobs */
	char *	activedir;		/* Directory that holds info about active jobs */
	bool 	daemon;
	int		log_level;
} options;

static struct {
	int 	kq;				/* kqueue(2) descriptor */
	LIST_HEAD(,job_manifest) pending; /* Jobs that have been submitted but not loaded */
	LIST_HEAD(,job) jobs;			/* All active jobs */
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

	/* Clear any record of active jobs that may be leftover from a previous program crash */
	if (asprintf(&buf, "/bin/rm -f %s/*", options.activedir) < 0) abort();
	if (system(buf) < 0) abort();
	free(buf);

	if (asprintf(&buf, "/usr/bin/touch %s/.notify", options.watchdir) < 0) abort();
	if (system(buf) < 0) abort();
	free(buf);
}

static void at_shutdown()
{
	free(options.pkgstatedir);
}

static job_manifest_t read_job(const char *filename)
{
	char *path = NULL, *rename_to = NULL;
	job_manifest_t jm;

	if ((jm = job_manifest_new()) == NULL) goto err_out;

	if (asprintf(&path, "%s/%s", options.watchdir, filename) < 0) goto err_out;

	log_debug("loading %s", path);
	if (job_manifest_read(jm, path) < 0) goto err_out;
	if (asprintf(&rename_to, "%s/%s", options.activedir, filename) < 0) goto err_out;
	if (rename(path, rename_to) != 0) goto err_out;
	free(path);
	free(rename_to);

	log_debug("defined job: %s", jm->label);

	return (jm);

err_out:
	if (path) {
		(void) unlink(path);
	}
	job_manifest_free(jm);
	free(path);
	return (NULL);
}

static ssize_t poll_watchdir()
{
	DIR	*dirp;
	struct dirent entry, *result;
	job_manifest_t jm;
	ssize_t found_jobs = 0;

	if ((dirp = opendir(options.watchdir)) == NULL) abort();

	while (dirp) {
		if (readdir_r(dirp, &entry, &result) < 0) abort();
		if (!result) break;
		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0 ||
				strcmp(entry.d_name, ".notify") == 0) {
			continue;
		}
		jm = read_job(entry.d_name);
		if (jm) {
			LIST_INSERT_HEAD(&state.pending, jm, jm_le);
			found_jobs++;
		} else {
			// note the failure?
		}
	}
	if (closedir(dirp) < 0) abort();
	return (found_jobs);
}

static void update_jobs(void)
{
	int i;
	job_manifest_t jm;
	job_t job, job_tmp;
	LIST_HEAD(,job) joblist;

	LIST_INIT(&joblist);

	/* Pass #1: load all jobs */
	LIST_FOREACH(jm, &state.pending, jm_le) {
		job = job_new(jm);
		LIST_INSERT_HEAD(&joblist, job, joblist_entry);
		if (!job) abort();
		(void) job_load(job); // FIXME failure handling?
		log_debug("loaded job: %s", job->jm->label);
	}
	LIST_INIT(&state.pending);

	/* Pass #2: run all loaded jobs */
	LIST_FOREACH(job, &joblist, joblist_entry) {
		if (job->state == JOB_STATE_LOADED) {
			log_debug("job %s state %d", job->jm->label, job->state);
			(void) job_run(job); // FIXME failure handling?
		}
	}

	/* Pass #3: move all new jobs to the main jobs list */
	LIST_FOREACH_SAFE(job, &joblist, joblist_entry, job_tmp) {
		LIST_REMOVE(job, joblist_entry);
		LIST_INSERT_HEAD(&state.jobs, job, joblist_entry);
	}
}

static void signal_handler(int signum) {
	(void) signum;
}

static void setup_signal_handlers()
{
	const int signals[] = {SIGHUP, SIGUSR1, SIGCHLD, 0};
	int i;
    struct kevent kev;

    for (i = 0; signals[i] != 0; i++) {
        EV_SET(&kev, signals[i], EVFILT_SIGNAL, EV_ADD, 0, 0, &setup_signal_handlers);
        if (kevent(state.kq, &kev, 1, NULL, 0, NULL) < 0) abort();
        if (signal(signals[i], signal_handler) == SIG_ERR) abort();
    }
}

static inline void setup_logging()
{
	//close(0);
	//close(1);
	//TODO: redirect stderr to a logfile
}

static void create_pid_file()
{
	char *path, *buf;
	int fd;
	ssize_t len;

	if (asprintf(&path, "%s/launchd.pid", options.pkgstatedir) < 0) abort();
	if (asprintf(&buf, "%d", getpid()) < 0) abort();
	len = strlen(buf);
	if ((fd = open(path, O_CREAT | O_WRONLY, 0644)) < 0) {
		log_errno("open of %s", path);
		abort();
	}
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
	if (pid < 0) abort();

	LIST_FOREACH(job, &state.jobs, joblist_entry) {
		if (job->pid == pid) {
			job->state = JOB_STATE_EXITED;
			if (WIFEXITED(status)) {
				job->last_exit_status = WEXITSTATUS(status);
			} else if (WIFSIGNALED(status)) {
				job->last_exit_status = -1;
				job->term_signal = WTERMSIG(status);
			} else {
				log_error("unhandled exit status");
			}
			log_debug("job %d exited with status %d", job->pid, job->last_exit_status);
			job->pid = 0;
			return;
		}
	}
	log_error("child exited but no job found");
}

static void write_status_file()
{
	char *path, *buf, *pid;
	int fd;
	ssize_t len;
	job_t job;

	/* FIXME: should write to a .new file, then rename() over the old file */
	if (asprintf(&path, "%s/launchctl.list", options.pkgstatedir) < 0) abort();
	if ((fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		log_errno("open of %s", path);
		abort();
	}
	if (asprintf(&buf, "%-8s %-8s %s\n", "PID", "Status", "Label") < 0) abort();
	len = strlen(buf) + 1;
	if (write(fd, buf, len) < len) abort();
	free(buf);
	LIST_FOREACH(job, &state.jobs, joblist_entry) {
		if (job->pid == 0) {
			if ((pid = strdup("-")) == NULL) abort();
		} else {
			if (asprintf(&pid, "%d", job->pid) < 0) abort();
		}
		if (asprintf(&buf, "%-8s %-8d %s\n", pid, job->last_exit_status, job->jm->label) < 0) abort();
		len = strlen(buf) + 1;
		if (write(fd, buf, len) < len) abort();
		free(buf);
		free(pid);
	}
	if (close(fd) < 0) abort();
	free(path);
	free(buf);
}

static void main_loop()
{
    struct kevent kev;
    uset_t new_jobs;

	if ((state.kq = kqueue()) < 0) abort();
	LIST_INIT(&state.jobs);

	create_pid_file();
	setup_signal_handlers();
	poll_watchdir();

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
				if (poll_watchdir() > 0) {
					update_jobs();
				}
				break;
			case SIGUSR1:
				write_status_file();
				break;
			case SIGCHLD:
				reap_child();
				break;
			default:
				log_error("got unexpected signal");
			}
		} else {
			log_warning("spurious wakeup, no known handlers");
		}
	}
	at_shutdown(); //TODO-make this actually run
}

int
main(int argc, char *argv[])
{
    int c;

	options.daemon = true;
	options.log_level = LOG_DEBUG;
	if (getuid() == 0) {
		abort();
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

	if (options.daemon && daemon(0, 0) < 0) abort();

	setup_logging();
	main_loop();

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
