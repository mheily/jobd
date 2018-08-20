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

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "logger.h"
#include "job.h"

static void
string_array_free(char **strarr)
{
	char **p;

	if (strarr) {
		for (p = strarr; *p; p++) {
			free(*p);
		}
		free(strarr);
	}
}

static int
redirect_file_descriptor(int oldfd, const char *path, int flags, mode_t mode)
{
	int newfd;

	newfd = open(path, flags, mode);
	if (newfd < 0) {
		printlog(LOG_ERR, "open(2) of %s: %s", path, strerror(errno));
		return (-1);
	}
	if (dup2(newfd, oldfd) < 0) {
		printlog(LOG_ERR, "dup2(2): %s", strerror(errno));
		(void) close(newfd);
		return (-1);
	}
	if (close(newfd) < 0) {
		printlog(LOG_ERR, "close(2): %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
job_start(struct job *job)
{
	sigset_t mask;

	if (job->state != JOB_STATE_STOPPED)
		return (-1); //TODO: want -IPC_RESPONSE_INVALID_STATE

	job->pid = fork();
	if (job->pid < 0) {
		printlog(LOG_ERR, "fork(2): %s", strerror(errno));
		return (-1);
	} else if (job->pid == 0) {
		(void)setsid();

		sigfillset(&mask);
		(void) sigprocmask(SIG_UNBLOCK, &mask, NULL);

		//TODO: setrlimit
		if (chdir(job->working_directory) < 0) {
			printlog(LOG_ERR, "chdir(2) to %s: %s", job->working_directory, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (getuid() == 0) {
			if (strcmp(job->root_directory, "/") && (chroot(job->root_directory) < 0)) {
				printlog(LOG_ERR, "chroot(2) to %s: %s", job->root_directory, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (job->init_groups && (initgroups(job->user_name, job->gid) < 0)) {
				printlog(LOG_ERR, "initgroups(3): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (setgid(job->gid) < 0) {
				printlog(LOG_ERR, "setgid(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
#ifndef __GLIBC__
			/* KLUDGE: above is actually a test for BSD */
			if (setlogin(job->user_name) < 0) {
				printlog(LOG_ERR, "setlogin(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
#endif
			if (setuid(job->uid) < 0) {
				printlog(LOG_ERR, "setuid(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		(void) umask(job->umask);

		//TODO this->setup_environment();
		//this->createDescriptors();

		// TODO: deal with globbing

		if (redirect_file_descriptor(STDIN_FILENO, job->standard_in_path, O_RDONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDIN");
			exit(EXIT_FAILURE);
		}
		if (redirect_file_descriptor(STDOUT_FILENO, job->standard_out_path, O_CREAT | O_WRONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDOUT");
			exit(EXIT_FAILURE);
		}
		if (redirect_file_descriptor(STDERR_FILENO, job->standard_error_path, O_CREAT | O_WRONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDERR");
			exit(EXIT_FAILURE);
		}
		if (execve(job->argv[0], job->argv, job->environment_variables) < 0) {
			printlog(LOG_ERR, "execve(2): %s", strerror(errno));
			exit(EXIT_FAILURE);
    	}
		/* NOTREACHED */
	} else {
		/*
		job->state = JOB_STATE_STARTING;
		
		TODO: allow the use of a check script that waits for the service to finish initializing.
		*/
		job->state = JOB_STATE_RUNNING;

		//TODO: manager
		printlog(LOG_DEBUG, "job %s started with pid %d", job->id, job->pid);
	}

	return (0);
}

void 
job_free(struct job *job)
{
	if (job) {
		string_array_free(job->after);
		string_array_free(job->before);
		free(job->description);
		string_array_free(job->environment_variables);
		free(job->id);
		free(job->title);
		string_array_free(job->argv);
		free(job->root_directory);
		free(job->standard_error_path);
		free(job->standard_in_path);
		free(job->standard_out_path);
		free(job->working_directory);
		free(job->user_name);
		free(job);
	}
}