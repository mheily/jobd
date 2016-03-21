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


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include "../../src/calendar.c"

static int test_kqfd;

//FIXME: copypaste from jmtest.c
#define run(test) do { \
	printf("%-24s", ""#test); \
	if (test() != 0) { \
		puts("FAILED"); \
		exit(1); \
	} else { \
		puts("passed"); \
	} \
} while(0)

#define fail(_message) do { \
	printf("FAIL: %s\n", _message); \
	return -1; \
} while (0)

static int test_calendar_init()
{
	assert(calendar_init(test_kqfd) == 0);
	return 0;
}

#if DISABLED
static int test_calendar_interval_1()
{
        job_manifest_t jm;
        job_t job;

        jm = job_manifest_new();
        assert(job_manifest_read(jm, "fixtures/cron1.json") == 0);
        job = job_new(jm);
        assert(job);

        /* Fool the job into thinking it should run in the next 5 minutes */
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
	if ((tm->tm_min + 5) <= 59) {
		job->jm->calendar_interval.minute = tm->tm_min + 5;
	} else {
		//FIXME: need to wrap time around to the next hour/day
		printf("SKIP -- this test needs time wrapping support");
		job_free(job);
		return 0;
	}

        set_current_time(now);

        assert(timer_register_job(job) == 0);
        log_debug("min_interval=%d", min_interval);
        assert(min_interval == 5);
        assert(timer_unregister_job(job) == 0);
        assert(min_interval == 0);
        job_free(job);
        return 0;
}
#endif

int main()
{
	test_kqfd = kqueue();
	run(test_calendar_init);
	//FIXME: run(test_calendar_interval_1);
	exit(0);
}
