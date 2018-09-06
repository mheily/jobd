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

#include "database.h"
#include "logger.h"
#include "job.h"
#include "parser.h"

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
	char *filename, *script;
	char *argv[5];
	char **envp;

	if (job->state != JOB_STATE_STOPPED)
		return (-1); //TODO: want -IPC_RESPONSE_INVALID_STATE

	if (asprintf(&script, "exec %s", job->command) < 0)
		return (-1);

	filename = "/bin/sh";
	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = script;
	argv[3] = '\0';
	envp = string_array_data(job->environment_variables);

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
		if (execve(filename, argv, envp) < 0) {
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
		printlog(LOG_DEBUG, "job %s started with pid %d: %s", job->id, job->pid, script);
		free(script);
	}

	return (0);
}

struct job *
job_new(void)
{
	struct job *j;

	j = calloc(1, sizeof(*j));
	if (!j)
		return (NULL);
	
	j->enable = 1;
	if (!(j->after = string_array_new()))
		goto err_out;
	if (!(j->before = string_array_new()))
		goto err_out;
	if (!(j->environment_variables = string_array_new()))
		goto err_out;

	return (j);

err_out:
	job_free(j);
	return (NULL);
}

void 
job_free(struct job *job)
{
	if (job) {
		string_array_free(job->after);
		string_array_free(job->before);
		free(job->command);
		free(job->description);
		string_array_free(job->environment_variables);
		free(job->id);
		free(job->title);
		free(job->root_directory);
		free(job->standard_error_path);
		free(job->standard_in_path);
		free(job->standard_out_path);
		free(job->working_directory);
		free(job->user_name);
		free(job->group_name);
		free(job->umask_str);
		free(job);
	}
}

struct job *
job_list_lookup(const struct job_list *jobs, const char *id)
{
	struct job *job;

	LIST_FOREACH(job, jobs, entries) {
		if (!strcmp(id, job->id)) {
			return job;
		}
	}
	return (NULL);
}

static int
job_get_depends(struct job *job)
{
	sqlite3_stmt *stmt;
	char *sql;
	int rv;

	/* Get the job->after deps */
	sql = "SELECT before_job_id FROM job_depends WHERE after_job_id = ?";
	if ((rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK))
		goto db_err;
	if ((rv = sqlite3_bind_text(stmt, 1, job->id, -1, SQLITE_STATIC) != SQLITE_OK))
		goto db_err;
	if (db_select_into_string_array(job->after, stmt) < 0)
		goto err_out;
	sqlite3_finalize(stmt);

	/* Get the job->before deps */
	sql = "SELECT after_job_id FROM job_depends WHERE before_job_id = ?";
	if ((rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK))
		goto db_err;
	if ((rv = sqlite3_bind_text(stmt, 1, job->id, -1, SQLITE_STATIC) != SQLITE_OK))
		goto db_err;
	if (db_select_into_string_array(job->before, stmt) < 0)
		goto err_out;
	sqlite3_finalize(stmt);

	return (0);

db_err:
	db_log_error(rv);

err_out:
	sqlite3_finalize(stmt);
	return (-1);
}

int
job_db_select_all(struct job_list *dest)
{
	const char *sql = "SELECT job_id, description, gid, init_groups,"
					  "keep_alive, root_directory, standard_error_path,"
					  "standard_in_path, standard_out_path, umask, user_name,"
					  "working_directory, id, enable,command "
					  "FROM jobs ";

	sqlite3_stmt *stmt = NULL;
	struct job *job;
	int rv;

	rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0);
	if (rv != SQLITE_OK)
		goto db_err;
	
	for (;;) {
		rv = sqlite3_step(stmt);
		if (rv == SQLITE_DONE)
			break;
		if (rv != SQLITE_ROW)
			goto db_err;

		job = job_new();
		if (!job)
			goto os_err;

		if (!(job->id = strdup((char *)sqlite3_column_text(stmt, 0))))
			goto os_err;
		if (!(job->description = strdup((char *)sqlite3_column_text(stmt, 1))))
			goto os_err;
		if (!(job->group_name = strdup((char *)sqlite3_column_text(stmt, 2))))
			goto os_err;
		job->init_groups = sqlite3_column_int(stmt, 3);
		job->keep_alive = sqlite3_column_int(stmt, 4);
		if (!(job->root_directory = strdup((char *)sqlite3_column_text(stmt, 5))))
			goto os_err;
		if (!(job->standard_error_path = strdup((char *)sqlite3_column_text(stmt, 6))))
			goto os_err;
		if (!(job->standard_in_path = strdup((char *)sqlite3_column_text(stmt, 7))))
			goto os_err;
		if (!(job->standard_out_path = strdup((char *)sqlite3_column_text(stmt, 8))))
			goto os_err;
		if (!(job->umask_str = strdup((char *)sqlite3_column_text(stmt, 9))))
			goto os_err;
		if (!(job->user_name = strdup((char *)sqlite3_column_text(stmt, 10))))
			goto os_err;
		if (!(job->working_directory = strdup((char *)sqlite3_column_text(stmt, 11))))
			goto os_err;
		job->row_id = sqlite3_column_int64(stmt, 12);
		job->enable = sqlite3_column_int(stmt, 13);
		if (!(job->command = strdup((char *)sqlite3_column_text(stmt, 14))))
			goto os_err;

		//FIXME - need to deal w/ gid, umask parsing

		if (job_get_depends(job) < 0)
			goto err_out;

		LIST_INSERT_HEAD(dest, job, entries);
	}

	sqlite3_finalize(stmt);
	return (0);

os_err:
	printlog(LOG_ERR, "OS error: %s", strerror(errno));
	goto err_out;

db_err:
	db_log_error(rv);

err_out:
	sqlite3_finalize(stmt);
	job_free(job);
	return (-1);
}

char *
job_get_method(const struct job *job, const char *method_name)
{
	sqlite3_stmt *stmt;
	int success;
	char *result;

	if (!job)
		return (NULL);
	
	const char *sql = "SELECT script FROM job_methods WHERE job_id = ? AND name = ?";
	success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
		sqlite3_bind_int64(stmt, 1, job->row_id) == SQLITE_OK &&
		sqlite3_bind_text(stmt, 2, method_name, -1, SQLITE_STATIC) == SQLITE_OK &&
  	    sqlite3_step(stmt) == SQLITE_ROW &&
		(result = strdup((char *)sqlite3_column_text(stmt, 0)));

	sqlite3_finalize(stmt);

	return (success ? result : NULL);
}