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

#include "vendor/FreeBSD/sys/queue.h"
#if HAVE_SYS_LIMITS_H
#include <sys/limits.h>
#else
#include <limits.h>
#endif
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <time.h>

#include "log.h"
#include "manager.h"
#include "timer.h"
#include "job.h"

/* The main kqueue descriptor used by launchd */
static int parent_kqfd;

#define TIMER_TYPE_CONSTANT_INTERVAL 1
//#define TIMER_TYPE_CALENDAR_INTERVAL 2

static SLIST_HEAD(, job) start_interval_list;
//static SLIST_HEAD(, job) calendar_list;

static uint32_t min_interval = UINT_MAX;

/* Find the smallest interval that we can wait before waking up at least one job */
static void update_min_interval()
{
	struct kevent kev;
	job_t job;
	int saved_interval = min_interval;

	if (SLIST_EMPTY(&start_interval_list)) {
		if (min_interval == 0) {
			return;
		}
		EV_SET(&kev, TIMER_TYPE_CONSTANT_INTERVAL, EVFILT_TIMER, EV_ADD | EV_DISABLE, 0, 0, &setup_timers);
		if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0) {
			log_errno("kevent(2)");
			abort();
		}
		min_interval = 0;
	} else {
		SLIST_FOREACH(job, &start_interval_list, start_interval_sle) {
			if (job->jm->start_interval < min_interval)
				min_interval = job->jm->start_interval;
		}
		if (min_interval > 0 && saved_interval == 0) {
			EV_SET(&kev, TIMER_TYPE_CONSTANT_INTERVAL, EVFILT_TIMER,
					EV_ADD | EV_ENABLE, 0, (1000 * min_interval), &setup_timers);
			if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0) {
				log_errno("kevent(2)");
				abort();
			}
		}
	}
}

/*
 * Provide a mock clock object that can be manipulated when running unit tests.
 */
#ifdef UNIT_TEST

static struct timespec mock_clock = {0, 0};

void set_current_time(time_t sec)
{
	mock_clock.tv_sec = sec;
	mock_clock.tv_nsec = 0;
}

static inline time_t current_time() {
	return mock_clock.tv_sec;
}

#else

static inline time_t current_time() {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
		log_errno("clock_gettime(2)");
		abort();
	}
	return now.tv_sec;
}

#endif /* UNIT_TEST */

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
	if (min_interval < result) {
		min_interval = result;
	}

	/* KLUDGE: this is ugly, b/c the manifest did not actually set StartInterval.
	 * We should really have a job_t field for this instead.
	 */
	job->jm->start_interval = result;

	log_debug("job %s scheduled to run in %ld minutes", job->jm->label, result);

	return current_time() + (60 * result);
}

static inline void update_job_interval(job_t job)
{
	if (job->schedule == JOB_SCHEDULE_PERIODIC) {
		job->next_scheduled_start = current_time() + job->jm->start_interval;
	} else if (job->schedule == JOB_SCHEDULE_CALENDAR) {
		job->next_scheduled_start = schedule_calendar_job(job);
	}
	log_debug("job %s will start after T=%lu", job->jm->label, job->next_scheduled_start);
}

int setup_timers(int kqfd)
{
	parent_kqfd = kqfd;
	SLIST_INIT(&start_interval_list);
	//SLIST_INIT(&calendar_list);
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

int timer_register_calendar_interval(struct job *job)
{
	return timer_register_job(job);
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
