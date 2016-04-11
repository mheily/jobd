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

#ifndef RELAUNCHD_CLOCK_H_
#define RELAUNCHD_CLOCK_H_

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "log.h"

#if defined(__MACH__)
#include "../vendor/PosixMachTiming/src/timing_mach.h"
#endif

/*
 * Provide a mock clock object that can be manipulated when running unit tests.
 */
#ifdef UNIT_TEST

static struct timespec mock_clock = {0, 0};

static void  __attribute__((unused))
set_current_time(time_t sec)
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
		err(1, "clock_gettime(2)");
	}
	return now.tv_sec;
}

#endif /* UNIT_TEST */
#endif /* RELAUNCHD_CLOCK_H_ */
