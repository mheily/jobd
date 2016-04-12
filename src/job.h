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

#ifndef JOB_H_
#define JOB_H_

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include "../vendor/FreeBSD/sys/queue.h"
#include <unistd.h>

#include "manifest.h"

typedef enum {
	JOB_SCHEDULE_NONE = 0,
	JOB_SCHEDULE_PERIODIC,
	JOB_SCHEDULE_CALENDAR,
	JOB_SCHEDULE_KEEPALIVE
} job_schedule_t;

struct job {
	LIST_ENTRY(job)	joblist_entry;
	SLIST_ENTRY(job) start_interval_sle;
	SLIST_ENTRY(job) watchdog_sle;
	job_manifest_t jm;
	enum {
		JOB_STATE_DEFINED,
		JOB_STATE_LOADED,
		JOB_STATE_WAITING,
		JOB_STATE_RUNNING,
		JOB_STATE_KILLED,
		JOB_STATE_EXITED,
	} state;
	pid_t pid;
	int last_exit_status, term_signal;
	time_t  next_scheduled_start;
	job_schedule_t schedule;
};
typedef struct job *job_t;

job_t	job_new(job_manifest_t jm);
void	job_free(job_t job);
int	job_load(job_t job);
int	job_unload(job_t job);
int	job_run(job_t job);

static inline int
job_is_runnable(job_t job)
{
	return (job->state == JOB_STATE_LOADED && job->jm->run_at_load);
}

#endif /* JOB_H_ */
