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

#ifndef _JOB_H
#define _JOB_H

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include "queue.h"
#else
#include <sys/queue.h>
#endif /* <sys/queue.h> */

#include "array.h"

#define job_id_t int64_t

/* Max length of a job ID. Equivalent to FILE_MAX */
#define JOB_ID_MAX 255

/* Maximum length of job->command or any inline shell script used as a job method.
   TODO: verify this is not larger than the output of "getconf ARG_MAX" during
         ./configure runs
*/
#define JOB_ARG_MAX 200000

enum job_state {
	JOB_STATE_UNKNOWN,
	JOB_STATE_DISABLED,
	JOB_STATE_PENDING,
	JOB_STATE_STARTING,
	JOB_STATE_RUNNING,
	JOB_STATE_STOPPING,
	JOB_STATE_STOPPED,
	JOB_STATE_COMPLETE,
	JOB_STATE_ERROR
};

enum job_type {
	JOB_TYPE_UNKNOWN,
	JOB_TYPE_TASK,
	JOB_TYPE_SERVICE
};

struct job_parser;

struct job {
	int64_t row_id;

	/* Items below here are parsed from the manifest */
	struct string_array *before, *after;
	char *id;
	char *command;
	char *description;
	bool wait_flag;
	struct string_array *environment_variables;
	gid_t gid;
	char *group_name;
	bool init_groups;
	bool keep_alive;
	char *title;
	char *root_directory;
	char *standard_error_path;
	char *standard_in_path;
	char *standard_out_path;
	mode_t umask;
	char *umask_str;
	uid_t uid;
	char *user_name;
	char *working_directory;
	char **options;
	enum job_type job_type;
};

int job_start(pid_t *pid, job_id_t id);
int job_stop(job_id_t id);
int job_enable(job_id_t id);
int job_disable(job_id_t id);

struct job *job_new(void);
void job_free(struct job *);

const char * job_id_to_str(job_id_t jid);

int job_get_label_by_pid(char label[JOB_ID_MAX], pid_t pid);
int job_get_id(int64_t *jid, const char *label);
int job_get_pid(pid_t *pid, int64_t row_id);
int job_get_property(char **value, const char *key, int64_t jid);
int job_get_command(char dest[JOB_ARG_MAX], job_id_t id);
int job_get_method(char **dest, job_id_t jid, const char *method_name);
int job_get_state(enum job_state *state, job_id_t id);
int job_set_property(int64_t jid, const char *key, const char *value);
int job_set_state(int64_t job_id, enum job_state state);
int job_get_type(enum job_type *type, job_id_t id);

int job_register_pid(int64_t row_id, pid_t pid);

int job_set_exit_status(pid_t pid, int status);
int job_set_signal_status(pid_t pid, int signum);

const char *job_state_to_str(enum job_state state);
const char *job_id_to_str(job_id_t id);

#endif /* _JOB_H */
