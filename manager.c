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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>

#include "log.h"
#include "job.h"
#include "manager.h"
#include "socket.h"
#include "options.h"

extern struct launchd_options options;

static LIST_HEAD(,job_manifest) pending; /* Jobs that have been submitted but not loaded */
static LIST_HEAD(,job) jobs;			/* All active jobs */

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
		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
			continue;
		}
		jm = read_job(entry.d_name);
		if (jm) {
			LIST_INSERT_HEAD(&pending, jm, jm_le);
			found_jobs++;
		} else {
			// note the failure?
		}
	}
	if (closedir(dirp) < 0) abort();
	return (found_jobs);
}

void update_jobs(void)
{
	int i;
	job_manifest_t jm;
	job_t job, job_tmp;
	LIST_HEAD(,job) joblist;

	LIST_INIT(&joblist);

	/* Pass #1: load all jobs */
	LIST_FOREACH(jm, &pending, jm_le) {
		job = job_new(jm);
		LIST_INSERT_HEAD(&joblist, job, joblist_entry);
		if (!job) abort();
		(void) job_load(job); // FIXME failure handling?
		log_debug("loaded job: %s", job->jm->label);
	}
	LIST_INIT(&pending);

	/* Pass #2: run all loaded jobs */
	LIST_FOREACH(job, &joblist, joblist_entry) {
		if (job_is_runnable(job)) {
			log_debug("running job %s from state %d", job->jm->label, job->state);
			(void) job_run(job); // FIXME failure handling?
		}
	}

	/* Pass #3: move all new jobs to the main jobs list */
	LIST_FOREACH_SAFE(job, &joblist, joblist_entry, job_tmp) {
		LIST_REMOVE(job, joblist_entry);
		LIST_INSERT_HEAD(&jobs, job, joblist_entry);
	}
}

int manager_wake_job(job_t job)
{
	if (job->state != JOB_STATE_WAITING) {
		log_error("tried to wake job %s that was not asleep (state=%d)",
				job->jm->label, job->state);
		return -1;
	}

	return job_run(job);
}

int manager_activate_job_by_fd(int fd)
{
	return -1; //STUB
}

job_t manager_get_job_by_pid(pid_t pid)
{
	job_t job;

	LIST_FOREACH(job, &jobs, joblist_entry) {
		if (job->pid == pid) {
			return job;
		}
	}
	return NULL;
}

int manager_write_status_file()
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
	LIST_FOREACH(job, &jobs, joblist_entry) {
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
	return 0;
}

void manager_init()
{
	LIST_INIT(&jobs);
}

void manager_update_jobs()
{
	if (poll_watchdir() > 0) {
		update_jobs();
	}
}
