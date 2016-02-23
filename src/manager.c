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

#include "../vendor/FreeBSD/sys/queue.h"

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

static void manager_install_job(const char *filename);

extern struct launchd_options options;

static LIST_HEAD(,job_manifest) pending; /* Jobs that have been submitted but not loaded */
static LIST_HEAD(,job) jobs;			/* All active jobs */

//TODO: static'ize this once there is a way for the testsuite to un-static private functions.
/* NOTE: the caller must free the returned string */
char * get_file_extension(const char *filename)
{
        char *token, *prev, *fncopy;
        char *result;

        /* Special case: no occurance of the delimiter */
        if (strchr(filename, '.') == NULL)
		return strdup("");

        fncopy = strdup(filename);
        if (fncopy == NULL) abort();
        prev = NULL;
        while ((token = strsep(&fncopy, ".")) != NULL) {
                prev = token;
        }
        if (strlen(prev) > 0) {
		asprintf(&result, ".%s", prev); //TODO: error checking
        } else {
		/* Special case: trailing dot, e.g. "foo." */
		result = strdup(prev);
        }
        free(fncopy);
        return result;
}

static job_manifest_t read_job(const char *filename)
{
	job_manifest_t retval = NULL;
	char *path = NULL, *rename_to = NULL;
	job_manifest_t jm = NULL;

	jm = job_manifest_new();
	asprintf(&path, "%s/%s", options.watchdir, filename);
	asprintf(&rename_to, "%s/%s", options.activedir, filename);
	if (!jm || !path || !rename_to) {
		log_warning("malloc error");
		goto out;
	}

	log_debug("loading %s", path);
	if (job_manifest_read(jm, path) < 0) {
		log_error("parse error");
		goto out;
	}

	if (rename(path, rename_to) != 0) {
		log_errno("rename(2) of %s to %s", path, rename_to);
		goto out;
	}

	log_debug("defined job: %s", jm->label);

	retval = jm;

out:
	if (!retval)
		job_manifest_free(jm);
	free(path);
	free(rename_to);
	return (retval);
}

static ssize_t poll_watchdir()
{
	DIR	*dirp;
	struct dirent entry, *result;
	job_manifest_t jm;
	ssize_t found_jobs = 0;
	char *ext;

	if ((dirp = opendir(options.watchdir)) == NULL) abort();

	while (dirp) {
		if (readdir_r(dirp, &entry, &result) < 0) abort();
		if (!result) break;
		if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
			continue;
		}
		ext = get_file_extension(entry.d_name);
		if (strcmp(ext, ".json") == 0) {
			jm = read_job(entry.d_name);
			if (jm) {
				LIST_INSERT_HEAD(&pending, jm, jm_le);
				found_jobs++;
			} else {
				// note the failure?
			}
		} else if (strcmp(ext, ".install") == 0) {
			manager_install_job(entry.d_name);
		} else if (strcmp(ext, ".unload") == 0) {
			char *path;
			if (asprintf(&path, "%s/%s", options.watchdir, entry.d_name) < 0) {
				log_errno("asprintf");
				goto next_entry;
			}
			if (unlink(path) < 0) {
				log_errno("unlink(2) of %s", path);
				free(path);
				goto next_entry;
			}
			free(path);
			char *dot = strrchr(entry.d_name, '.');
			if (dot) {
				*dot = '\0';
			}
			if (manager_unload_job(entry.d_name) < 0) {
				log_error("unable to unload job: %s", entry.d_name);
			}
		} else {
			log_error("skipping %s: unsupported file extension", entry.d_name);
		}
next_entry:
		free(ext);
	}
	if (closedir(dirp) < 0) abort();
	return (found_jobs);
}

void update_jobs(void)
{
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

job_t manager_get_job_by_label(const char *label)
{
	job_t job;

	LIST_FOREACH(job, &jobs, joblist_entry) {
		if (strcmp(label, job->jm->label) == 0) {
			return job;
		}
	}
	return NULL;
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
	return 0;
}

void manager_free_job(job_t job) {
	LIST_REMOVE(job, joblist_entry);
	job_free(job);
}

int manager_unload_job(const char *label)
{
	job_t job, job_tmp, job_cur;
	int retval = -1;
	char *path = NULL;

	job = NULL;
	LIST_FOREACH_SAFE(job_cur, &jobs, joblist_entry, job_tmp) {
		if (strcmp(job_cur->jm->label, label) == 0) {
			job = job_cur;
			break;
		}
	}

	if (!job) {
		log_error("job not found: %s", label);
		goto out;
	}

	if (asprintf(&path, "%s/%s.json", options.activedir, label) < 0) {
		log_errno("asprintf");
		goto out;
	}

	if (unlink(path) < 0) {
		log_errno("unlink(2) of %s", path);
		goto out;
	}

	if (job_unload(job) < 0) {
		goto out;
	}

	log_debug("job %s unloaded", label);

	if (job->state == JOB_STATE_DEFINED) {
		manager_free_job(job);
	}

	retval = 0;

out:
	free(path);
	return retval;
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

/* Copy the job to the appropriate directory to cause it to start at the next
 * system boot. Also, install any prerequisites such as packages. Create jails
 * as needed.
 */
static void manager_install_job(const char *filename)
{
	job_manifest_t jm = NULL;
	char *buf = NULL;
	char *destdir = NULL;

	jm = read_job(filename);
	if (!jm) {
		log_error("unable to read job");
		goto out;
	}

	if (jm->job_is_agent) {
		if (getuid() == 0) {
			destdir = strdup("/usr/local/etc/launchd/agents");
		} else {
			asprintf(&destdir, "%s/.launchd/agents", getenv("HOME"));
		}
	} else {
		if (getuid() == 0) {
			destdir = strdup("/usr/local/etc/launchd/daemons");
		} else {
			log_error("tried to install a daemon as a non-root user");
			goto out;
		}
	}
	if (!destdir) {
		log_error("unable to set destdir");
		goto out;
	}

	if (asprintf(&buf, "/bin/mv %s/%s %s/%s.json", options.activedir, filename, destdir, jm->label) < 0) {
		log_errno("asprintf");
		goto out;
	}
	log_debug("installing manifest: %s", buf);
	if (system(buf) < 0) {
		log_errno("command failed: %s", buf);
		goto out;
	}

	/* TODO: install packages, create jails */

	free(buf); buf = NULL;
	if (asprintf(&buf, "/usr/local/bin/launchctl load %s/%s.json", destdir, jm->label) < 0) {
		log_errno("asprintf");
		goto out;
	}
	log_debug("loading manifest: %s", buf);
	if (system(buf) < 0) {
		log_errno("command failed: %s", buf);
		goto out;
	}

out:
	job_manifest_free(jm);
	free(buf);
	free(destdir);
}
