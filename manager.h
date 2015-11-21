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

/** Given a pending connection on a socket descriptor, activate the associated job */
int manager_activate_job_by_fd(int fd);

/**
 *
 * Given a process ID, find the associated job
 *
 * @return the job, or NULL if there are no matching jobs
 */
job_t manager_get_job_by_pid(pid_t pid);

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

void manager_init();
void manager_update_jobs();
int manager_write_status_file();

#endif /* MANAGER_H_ */
