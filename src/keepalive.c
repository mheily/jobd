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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <limits.h>
#include <unistd.h>

#include "clock.h"
#include "log.h"
#include "keepalive.h"
#include "job.h"

/* The main kqueue descriptor used by launchd */
static int parent_kqfd;

struct watchdog {
	job_t job;
	time_t restart_after; /* After this walltime, the job should be restarted */
	SLIST_ENTRY(watchdog) watchdog_sle;
};
typedef struct watchdog * watchdog_t;

static SLIST_HEAD(, watchdog) watchdog_list;

static struct watchdog *watchdog_new(job_t job);
static void update_wake_interval();

int keepalive_init(int kqfd)
{
	parent_kqfd = kqfd;
	SLIST_INIT(&watchdog_list);
	return 0;
}

int keepalive_add_job(struct job *job)
{
	if (job->jm->keep_alive.always) {
		watchdog_t w = watchdog_new(job);
		if (!w) return -1;
		SLIST_INSERT_HEAD(&watchdog_list, w, watchdog_sle);
		log_debug("job `%s' will be automatically restarted in %d seconds",
				job->jm->label, job->jm->throttle_interval);
		update_wake_interval();
	}
	return 0;
}

void keepalive_remove_job(struct job *job) {
	watchdog_t cur;

	SLIST_FOREACH(cur, &watchdog_list, watchdog_sle) {
		if (cur->job == job) {
			goto found;
		}
	}

	return;

found:
	SLIST_REMOVE(&watchdog_list, cur, watchdog, watchdog_sle);
	free(cur);
	update_wake_interval();
}

void keepalive_wake_handler(void)
{
	watchdog_t cur, tmp;
	time_t now = current_time();

	log_debug("watchdog handler running");
	update_wake_interval();

	SLIST_FOREACH_SAFE(cur, &watchdog_list, watchdog_sle, tmp) {
		if (now >= cur->restart_after) {
			if (cur->job->state != JOB_STATE_RUNNING) {
				log_debug("job `%s' restarted due to KeepAlive mechanism", cur->job->jm->label);
				job_run(cur->job);
			}
			SLIST_REMOVE(&watchdog_list, cur, watchdog, watchdog_sle);
			free(cur);
		}
	}
}

static struct watchdog *
watchdog_new(job_t job)
{
	watchdog_t w = malloc(sizeof(*w));
	if (w) {
		w->job = job;
		w->restart_after = current_time() + job->jm->throttle_interval;
	}
	return w;
}

static void update_wake_interval()
{
	watchdog_t w;
	struct kevent kev;
	int next_wake_time;
	static int interval = 0;

	if (SLIST_EMPTY(&watchdog_list)) {
		EV_SET(&kev, JOB_SCHEDULE_KEEPALIVE, EVFILT_TIMER, EV_ADD | EV_DISABLE, 0, 0, &keepalive_wake_handler);
		if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0) {
			err(1, "kevent(2)");
		}
		log_debug("disabling keepalive polling; no more watchdogs");
	} else {
		next_wake_time = INT_MAX;
		SLIST_FOREACH(w, &watchdog_list, watchdog_sle) {
			if (!w) break;
			if (w->restart_after < next_wake_time)
				next_wake_time = w->restart_after;
		}
		int time_delta = (next_wake_time - current_time()) * 1000;
		if (time_delta <= 0)
			time_delta = 10000;
		if (interval != time_delta) {
			EV_SET(&kev, JOB_SCHEDULE_KEEPALIVE, EVFILT_TIMER,
						EV_ADD | EV_ENABLE, 0, time_delta, &keepalive_wake_handler);
			if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0) {
					err(1, "kevent(2)");
			}

			log_debug("scheduled next wakeup event in %d ms", time_delta);
			interval = time_delta;
		}
	}
}
