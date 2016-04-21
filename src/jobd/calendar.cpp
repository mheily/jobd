/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
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
#include <sys/event.h>
#include <sys/time.h>
#include <time.h>

#include "clock.h"
#include "log.h"
#include "manager.h"
#include "calendar.h"
#include "job.h"

/* The main kqueue descriptor used by launchd */
static int parent_kqfd;

static SLIST_HEAD(, job) calendar_list;

static time_t next_wakeup = 0;

static inline time_t
find_next_time(const struct cron_spec *cron, const struct tm *now)
{
	uint32_t hour, minute;

	if (cron->hour == CRON_SPEC_WILDCARD) {
		hour = now->tm_hour;
	} else {
		hour = cron->hour;
	}

	if (cron->minute == CRON_SPEC_WILDCARD) {
		minute = now->tm_min;
	} else {
		minute = cron->minute;
	}

	return ((60 * hour) + minute);
}

static inline time_t
schedule_calendar_job(job_t job)
{
	const struct cron_spec *cron = &job->jm->calendar_interval;
	time_t t0 = current_time();
	struct tm tm;
	time_t result;

	localtime_r(&t0, &tm);

	/* Try to disqualify the job from running based on the current day */
	if (cron->month != CRON_SPEC_WILDCARD && cron->month != tm.tm_mon) {
		return 0;
	}
	if (cron->day != CRON_SPEC_WILDCARD && cron->day != tm.tm_mday) {
		return 0;
	}
	if (cron->weekday != CRON_SPEC_WILDCARD && cron->weekday != tm.tm_wday) {
		return 0;
	}

	/* Get the offset in minutes of the current time and the next job time,
	 * where 0 represents 00:00 of the current day.
	 */
	time_t cur_offset = (60 * tm.tm_hour) + tm.tm_min;
	time_t job_offset = find_next_time(cron, &tm);

	/* Disqualify jobs that are scheduled in the past */
	if (cur_offset > job_offset) {
		return 0;
	}

	result = job_offset - cur_offset;
	if (next_wakeup > result) {
		next_wakeup = result;
	}

	/* KLUDGE: this is ugly, b/c the manifest did not actually set StartInterval.
	 * We should really have a job_t field for this instead.
	 */
	job->jm->start_interval = result;

	log_debug("job %s scheduled to run in %ld minutes", job->jm->label, (long)result);

	return current_time() + (60 * result);
}

static inline void update_job_interval(job_t job)
{
	job->next_scheduled_start = schedule_calendar_job(job);
	log_debug("job %s will start after T=%lu", job->jm->label, (unsigned long)job->next_scheduled_start);
}

int calendar_init(int kqfd)
{
	parent_kqfd = kqfd;
	SLIST_INIT(&calendar_list);
	return 0;
}

int calendar_register_job(struct job *job)
{
	//TODO: keep the list sorted
	SLIST_INSERT_HEAD(&calendar_list, job, start_interval_sle);
	update_job_interval(job);
	return 0;
}

int calendar_unregister_job(struct job *job)
{
	if (job->schedule == JOB_SCHEDULE_NONE)
		return -1;

	SLIST_REMOVE(&calendar_list, job, job, start_interval_sle);
	//update_min_interval();
	return 0;
}

int calendar_handler()
{
	job_t job;
	time_t now = current_time();

	//log_debug("waking up after %u seconds", min_interval);
	SLIST_FOREACH(job, &calendar_list, start_interval_sle) {
		if (now >= job->next_scheduled_start) {
			log_debug("job %s starting due to timer interval", job->jm->label);
			update_job_interval(job);
			(void) manager_wake_job(job); //FIXME: error handling
		}
	}
	return 0;
}
