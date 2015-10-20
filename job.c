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

#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "job.h"
#include "log.h"

static void job_dump(job_t job) {
	log_debug("job dump: label=%s state=%d", job->jm->label, job->state);
}

static int apply_resource_limits(const job_t job) {
	//TODO - SoftResourceLimits, HardResourceLimits
	//TODO - LowPriorityIO

	if (setpriority(PRIO_PROCESS, 0, job->jm->nice) < 0)
		return (-1);

	return (0);
}

static inline int modify_credentials(job_t const job)
{
	//TODO: support all the things when running as root
	return (0);
}

static inline int exec_job(const job_t job) {
	int rv;
	char *path;
	char **argv, **envp;

	envp = cvec_to_array(job->jm->environment_variables);
	argv = cvec_to_array(job->jm->program_arguments);
	path = argv[0];
	if (job->jm->program) {
		path = job->jm->program;
	} else {
		path = argv[0];
	}
	if (job->jm->enable_globbing) {
    	//TODO: globbing
    }
	log_debug("exec: %s", path);
#if DEBUG
	log_debug("argv[]:");
	for (char **item = argv; *item; item++) {
		log_debug(" - arg: %s", *item);
	}
	log_debug("envp[]:");
	for (char **item = envp; *item; item++) {
		log_debug(" - env: %s", *item);
	}
#endif

    rv = execve(path, argv, envp);
    if (rv < 0) {
    	log_error("failed to call execve(2)");
    	return (-1);
    }
    log_notice("executed job");
    return (0);
}

static int start_child_process(const job_t job)
{
	int rv;

	if (apply_resource_limits(job) < 0) {
    	log_error("unable to apply resource limits");
    	goto err_out;
    }
	if (job->jm->working_directory) {
		if (chdir(job->jm->working_directory) < 0) {
			log_error("unable to chdir to %s", job->jm->working_directory);
			goto err_out;
		}
	}
	if (job->jm->root_directory && getuid() == 0) {
		if (chroot(job->jm->root_directory) < 0) {
			log_error("unable to chroot to %s", job->jm->root_directory);
			goto err_out;
		}
	}
	if (modify_credentials(job) < 0) {
    	log_error("unable to modify credentials");
    	goto err_out;
	}
    if (job->jm->umask) {
    	//TODO: umask
    }
    if (job->jm->stdin_path) {
	int fd = open(job->jm->stdin_path, O_RDONLY);
	if (fd < 0) goto err_out;
    	if (dup2(fd, 0) < 0) goto err_out;
    	if (close(fd) < 0) goto err_out;
    }
    if (job->jm->stdout_path) {
	int fd = open(job->jm->stdout_path, O_WRONLY);
	if (fd < 0) goto err_out;
    	if (dup2(fd, 1) < 0) goto err_out;
    	if (close(fd) < 0) goto err_out;
    }
    if (job->jm->stderr_path) {
	int fd = open(job->jm->stderr_path, O_WRONLY);
	if (fd < 0) goto err_out;
    	if (dup2(fd, 2) < 0) goto err_out;
    	if (close(fd) < 0) goto err_out;
    }

    return (exec_job(job));

err_out:
	log_error("job %s failed to start; see previous log message for details", job->jm->label);
	return (-1);
}

job_t job_new(job_manifest_t jm)
{
	job_t j;

	j = calloc(1, sizeof(*j));
	if (!j) return NULL;
	j->jm = jm;
	j->state = JOB_STATE_DEFINED;
	return (j);
}

void job_free(job_t job)
{
	if (job == NULL) return;
	free(job->jm);
	free(job);
}

int job_load(job_t job)
{
	/* TODO: This is the place to setup on-demand watches for the following keys:
			WatchPaths
			QueueDirectories
			StartInterval
			StartCalendarInterval
			Sockets
	*/
	job->state = JOB_STATE_LOADED;
	log_debug("loaded %s", job->jm->label);
	job_dump(job);
	return (0);
}

int job_run(job_t job)
{
	pid_t pid;

	// temporary for debugging
#ifdef NOFORK
	(void) start_child_process(job);
#else
    pid = fork();
    if (pid < 0) {
    	return (-1);
    } else if (pid == 0) {
    	if (start_child_process(job) < 0) {
    		//TODO: report failures to the parent
    		exit(127);
    	}
    } else {
    	log_debug("running %s", job->jm->label);
    	job->pid = pid;
    	job->state = JOB_STATE_RUNNING;
    }
#endif
	return (0);
}
