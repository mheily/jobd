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

#include <sys/types.h>
#include <sys/queue.h>
#include <unistd.h>

#include "manifest.h"

struct job {
    LIST_ENTRY(job) 	joblist_entry;
	job_manifest_t		jm;
	enum	{
		JOB_STATE_DEFINED,
		JOB_STATE_LOADED,
		JOB_STATE_RUNNING,
		JOB_STATE_EXITED,
	} state;
	pid_t	pid;
};
typedef struct job *job_t;

job_t 	job_new(job_manifest_t jm);
void	job_free(job_t job);
int		job_load(job_t job);
int		job_run(job_t job);

#endif /* JOB_H_ */
