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
#if HAVE_SYS_LIMITS_H
#include <sys/limits.h>
#else
#include <limits.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#include "clock.h"
#include "event_loop.h"
#include "log.h"
#include "manager.h"
#include "timer.h"
#include "job.h"

/* The main kqueue descriptor used by launchd */
static struct evl_proxy *parent_kqfd;

static SLIST_HEAD(, job) start_interval_list;

static uint32_t min_interval = UINT_MAX;

/* Find the smallest interval that we can wait before waking up at least one job */
static void update_min_interval()
{
	job_t job;
	int saved_interval = min_interval;

	if (SLIST_EMPTY(&start_interval_list)) {
		if (min_interval == 0) {
			return;
		}
		if (evl_timer_stop(parent_kqfd, JOB_SCHEDULE_PERIODIC, setup_timers) < 0)
			abort();
		min_interval = 0;
	} else {
		SLIST_FOREACH(job, &start_interval_list, start_interval_sle) {
			if (job->jm->start_interval < min_interval)
				min_interval = job->jm->start_interval;
		}
		if (min_interval > 0 && saved_interval == 0) {
			if (evl_timer_start(parent_kqfd, (1000 * min_interval), JOB_SCHEDULE_PERIODIC, setup_timers) < 0)
				abort();
		}
	}
}

static inline void update_job_interval(job_t job)
{
	job->next_scheduled_start = current_time() + job->jm->start_interval;
	log_debug("job %s will start after T=%lu", job->jm->label, (unsigned long)job->next_scheduled_start);
}

int setup_timers(struct evl_proxy *evp)
{
	parent_kqfd = evp;
	SLIST_INIT(&start_interval_list);
	return 0;
}

int timer_register_job(struct job *job)
{
	//TODO: keep the list sorted
	SLIST_INSERT_HEAD(&start_interval_list, job, start_interval_sle);
	update_job_interval(job);
	update_min_interval();
	return 0;
}

int timer_unregister_job(struct job *job)
{
	if (job->schedule == JOB_SCHEDULE_NONE)
		return -1;

	SLIST_REMOVE(&start_interval_list, job, job, start_interval_sle);
	update_min_interval();
	return 0;
}

#ifndef UNIT_TEST
int timer_handler()
{
	job_t job;
	time_t now = current_time();

	log_debug("waking up after %u seconds", min_interval);
	SLIST_FOREACH(job, &start_interval_list, start_interval_sle) {
		if (now >= job->next_scheduled_start) {
			log_debug("job %s starting due to timer interval", job->jm->label);
			update_job_interval(job);
			(void) manager_wake_job(job); //FIXME: error handling
		}
	}
	return 0;
}
#endif
