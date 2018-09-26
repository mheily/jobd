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

static const char *select_sql = "SELECT job_id, description, gid, init_groups,"
					  "keep_alive, root_directory, standard_error_path,"
					  "standard_in_path, standard_out_path, umask, user_name,"
					  "working_directory, id, enable, command, exclusive "
					  "FROM jobs ";

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

/* Run actions in the child after fork(2) but before execve(2) */
static void
_job_child_pre_exec(const struct job *job)
{
	sigset_t mask;
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
}

static int
job_command_exec(pid_t *child, const struct job *job, const char *command)
{
	pid_t pid;
	char *filename;
	char exec_command[JOB_ARG_MAX + sizeof("exec ")];
	char *argv[5];
	char **envp;
	ssize_t rv;

	*child = 0;

	rv = snprintf((char *)&exec_command, sizeof(exec_command), "exec %s", command);
	if (rv < 0 || rv >= (int)sizeof(exec_command)) {
		printlog(LOG_ERR, "buffer too small");
		return (-1);
	}

	filename = "/bin/sh";
	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = exec_command;
	argv[3] = NULL;
	envp = string_array_data(job->environment_variables);

	pid = fork();
	if (pid < 0) {
		printlog(LOG_ERR, "fork(2): %s", strerror(errno));
		return (-1);
	} else if (pid == 0) {
		_job_child_pre_exec(job);
		if (execve(filename, argv, envp) < 0) {
			printlog(LOG_ERR, "execve(2): %s", strerror(errno));
			exit(EXIT_FAILURE);
    	}
		/* NOTREACHED */
	} else {
		printlog(LOG_DEBUG, "job `%s': executing command as pid %d", job->id, pid);
		*child = pid;
	}

	return (0);
}

static int
job_method_exec(pid_t *child, const struct job *job, const char *method_name)
{
	pid_t pid;
	char *filename, *script = NULL;
	char *argv[5];
	char **envp;

	*child = 0;

	script = job_get_method(job, method_name);
	if (!script) {
		printlog(LOG_DEBUG, "job `%s': method not found: `%s'", job->id, method_name);
		return (0);
	}

	filename = "/bin/sh";
	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = script;
	argv[3] = NULL;
	envp = string_array_data(job->environment_variables);

	pid = fork();
	if (pid < 0) {
		printlog(LOG_ERR, "fork(2): %s", strerror(errno));
		goto err_out;
	} else if (pid == 0) {
		_job_child_pre_exec(job);
		if (execve(filename, argv, envp) < 0) {
			printlog(LOG_ERR, "execve(2): %s", strerror(errno));
			exit(EXIT_FAILURE);
    	}
		/* NOTREACHED */
	} else {
		printlog(LOG_DEBUG, "job `%s': method `%s' running as pid %d", job->id, method_name, pid);
		*child = pid;
	}

	free(script);
	return (0);

err_out:
	free(script);
	return (-1);
}

// TODO: handle the ripple effects of scheduling jobs affected by changes in the current job state
void
job_solve(struct job *job)
{
	if (job->state != JOB_STATE_UNKNOWN) {
		//FIXME: causes lots of noise right now: printlog(LOG_ERR, "bad usage");
		return;
	}

	job->state = JOB_STATE_STOPPED;
	if (job->enable) {
		job_start(job);
	}
}

int
job_start(struct job *job)
{
	pid_t pid;

	if (job->state != JOB_STATE_STOPPED) {
		printlog(LOG_ERR, "job is in the wrong state");
		return (-1);
	}

	if (job->command) {
		if (job_command_exec(&pid, job, job->command) < 0) {
			printlog(LOG_ERR, "start command failed");
			return (-1);			
		}
	} else {
	if (job_method_exec(&pid, job, "start") < 0) {
		printlog(LOG_ERR, "start method failed");
		return (-1);
	}
	}
	
	/*
		job->state = JOB_STATE_STARTING;
		
		TODO: allow the use of a check script that waits for the service to finish initializing.
	*/
	job->pid = pid;
	if (job->pid > 0) {
		job->state = JOB_STATE_RUNNING;
		printlog(LOG_DEBUG, "job %s started with pid %d", job->id, job->pid);
	}

	return (0);
}

int
job_stop(struct job *job)
{
	pid_t pid;

	if (job->state == JOB_STATE_STOPPED) {
		printlog(LOG_DEBUG, "job %s is already stopped", job->id);
		return (0);
	}

	if (job->state != JOB_STATE_RUNNING) {
		printlog(LOG_ERR, "job is in the wrong state");
		return (-1);
	}

	if (job_method_exec(&pid, job, "stop") < 0) {
		printlog(LOG_ERR, "stop method failed");
		return (-1);
	}

	if (pid > 0 && (job->pid == 0)) {
		job->pid = pid;
	} else if (!pid && (job->pid > 0)) {
		printlog(LOG_DEBUG, "sending SIGTERM to job %s (pid %d)", job->id, job->pid);
		if (kill(job->pid, SIGTERM) < 0) {
			if (errno == ESRCH) {
				/* Probably a harmless race condition, but note it anyway */
				printlog(LOG_WARNING, "job %s (pid %d): no such process", job->id, job->pid);
				job->state = JOB_STATE_STOPPED;
			} else {
				printlog(LOG_ERR, "kill(2): %s", strerror(errno));
				return (-1);
			}
		}
		job->state = JOB_STATE_STOPPING;
	} else {
		/* FIXME: open design question: what about jobs with a PID *and* a stop method? */
	}
	
	//TODO: start a timeout
	
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

static int
_job_stmt_copyin(sqlite3_stmt *stmt, struct job *job)
{
	
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
		job->exclusive = sqlite3_column_int(stmt, 15);

		//FIXME - need to deal w/ gid, umask parsing

		if (job_get_depends(job) < 0)
			goto err_out;

	return (0);

os_err:
	printlog(LOG_ERR, "OS error: %s", strerror(errno));
	goto err_out;

err_out:
	sqlite3_finalize(stmt);
	job_free(job);
	return (-1);
}

// FIXME: This returns the wrong job!
int
job_find(struct job **result, const char *job_id)
{
	char sql[SQL_BUF_MAX];
	sqlite3_stmt *stmt;
	struct job *job = NULL;
	int rv;

	errx(1,"FIXME: need to debug this");
	
	*result = NULL;

	rv = snprintf((char *)&sql, sizeof(sql),  "%s WHERE job_id = ?", select_sql);
	if (rv >= (int)sizeof(sql) || rv < 0) {
			printlog(LOG_ERR, "snprintf failed");
			return (-1);
	}

	rv = sqlite3_prepare_v2(dbh, select_sql, -1, &stmt, 0);
	if (rv != SQLITE_OK) {
		db_log_error(rv);
		return (-1);
	}

	sqlite3_bind_text(stmt, 1, job_id, -1, SQLITE_STATIC); 
	if (rv != SQLITE_OK) {
		db_log_error(rv);
		return (-1);
	}

	rv = sqlite3_step(stmt);
	if (rv == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return (-1);
	}

	if (rv == SQLITE_ROW) {
		job = job_new();
		if (!job)
			goto err_out;

		if (_job_stmt_copyin(stmt, job) < 0)
			goto err_out;
	} else {
		db_log_error(rv);
		return (-1);	
	}

	sqlite3_finalize(stmt);
	*result = job;

	return (0);

err_out:
	sqlite3_finalize(stmt);
	job_free(job);
	return (-1);
}

int
job_db_select_all(struct job_list *dest)
{
	sqlite3_stmt *stmt = NULL;
	struct job *job = NULL;
	int rv;

	rv = sqlite3_prepare_v2(dbh, select_sql, -1, &stmt, 0);
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

		if (_job_stmt_copyin(stmt, job) < 0)
			goto os_err;

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

/* The caller must free the value returned by this function */
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

int
job_enable(struct job *job)
{
	sqlite3_stmt *stmt;
	int success;

	if (!job)
		return (-1);
	
	if (job->enable)
		return (0);

	const char *sql = "UPDATE jobs SET enable = 1 WHERE id = ?";
	success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
		sqlite3_bind_int64(stmt, 1, job->row_id) == SQLITE_OK &&
  	    sqlite3_step(stmt) == SQLITE_DONE;

	sqlite3_finalize(stmt);

	if (sqlite3_changes(dbh) == 0) {
		printlog(LOG_ERR, "job %s does not exist", job->id);
		return (-1);
	}

	if (success) {
		printlog(LOG_DEBUG, "job %s has been enabled", job->id);
		job->enable = true;
		job_start(job);
		return (0);
	} else {
		return (-1);
	}
}

int
job_disable(struct job *job)
{
	sqlite3_stmt *stmt;
	int success;

	if (!job)
		return (-1);
	
	if (!job->enable)
		return (0);

	const char *sql = "UPDATE jobs SET enable = 0 WHERE id = ?";
	success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
		sqlite3_bind_int64(stmt, 1, job->row_id) == SQLITE_OK &&
  	    sqlite3_step(stmt) == SQLITE_DONE;

	sqlite3_finalize(stmt);

	if (sqlite3_changes(dbh) == 0) {
		printlog(LOG_ERR, "job %s does not exist", job->id);
		return (-1);
	}

	if (success) {
		printlog(LOG_DEBUG, "job %s has been disabled (current_state=%d)", job->id, job->state);
		job->enable = false;
		job_stop(job);
		return (0);
	} else {
		return (-1);
	}
}