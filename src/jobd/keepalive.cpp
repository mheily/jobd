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

#if 0

#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <limits.h>
#include <unistd.h>

#include "clock.h"
#include <libjob/logger.h>
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

//DEADWOOD
int keepalive_init(int kqfd)
{
	parent_kqfd = kqfd;
	return 0;
}

int keepalive_add_job(Job& job)
{
	if (job.manifest.json["KeepAlive"].get<bool>() == true) {
//		log_debug("job `%s' will be automatically restarted in %d seconds",
//				job.getLabel(), job.);
		update_wake_interval();
	}
	return 0;
}

void keepalive_remove_job(Job& job) {
	job.setRestartAfter(0);
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

#endif
