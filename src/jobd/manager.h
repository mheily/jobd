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

#ifndef MANAGER_H_
#define MANAGER_H_

#include "job.h"
#include "pidfile.h"

/** Given a pending connection on a socket descriptor, activate the associated job */
int manager_activate_job_by_fd(int fd);

/**
 * Given a process ID, find the associated job
 *
 * @return the job, or NULL if there are no matching jobs
 */
job_t manager_get_job_by_pid(pid_t pid);

/**
 * Given a label, find the associated job
 *
 * @return the job, or NULL if there are no matching jobs
 */
job_t manager_get_job_by_label(const char *label);

/**
 * Unload a job with a given <label>
 */
int manager_unload_job(const char *label);

/**
 * Remove the job from the joblist and free it.
 */
void manager_free_job(job_t job);

/**
 * Wake up a job that has been waiting for an external event.
 */
int manager_wake_job(job_t job);

void manager_init(struct pidfh *);
void manager_update_jobs();
int manager_write_status_file();
void manager_reap_child(pid_t pid, int status);
void manager_pid_event_add(int pid);
void manager_pid_event_delete(int pid);
void manager_main_loop();
void manager_unload_all_jobs();

#endif /* MANAGER_H_ */
