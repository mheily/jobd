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

#ifndef _JOB_DESCRIPTOR_H_
#define _JOB_DESCRIPTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdlib.h>

static inline int
job_descriptor_get(const char *name)
{
	char key[1024];
	char *val;
	int len;

	len = snprintf((char *)&key, sizeof(key), "JOB_DESCRIPTOR_%s", name);
	if (len < 0 || len >= sizeof(key)) {
		errno = EINVAL;
		return -1;
	}

	val = getenv(key);
	if (val == NULL) {
		errno = ENOENT;
		return -1;
	}

	return ((int)strtol(val, (char **)NULL, 10));
}

#ifdef __cplusplus
}
#endif

#endif /* !_JOB_DESCRIPTOR_H_*/
