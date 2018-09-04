/*
 * Copyright (c) 2018 Mark Heily <mark@heily.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "tsort.h"
#include "job.h"
#include "logger.h"
#include "array.h"

static struct job *
find_job_by_id(const struct job_list *jobs, const char *id)
{
	struct job *job;

	LIST_FOREACH(job, jobs, entries) {
		if (!strcmp(id, job->id)) {
			return job;
		}
	}
	return (NULL);
}

/* This sorting algorithm is not efficient, but is fairly simple. */
int
topological_sort(struct job_list *dest, struct job_list *src)
{
	struct job *cur, *tmp, *tail;
	uint32_t i;

	/* Find all incoming edges and keep track of how many each node has */
	LIST_FOREACH(cur, src, entries) {
		LIST_FOREACH(tmp, src, entries) {
			if (cur != tmp && string_array_contains(cur->after, tmp->id)) {
				printlog(LOG_DEBUG, "edge from %s to %s", tmp->id, cur->id);
				cur->incoming_edges++;
			}
		}
	}
	LIST_FOREACH(cur, src, entries) {
		LIST_FOREACH(tmp, src, entries) {
			if (cur != tmp && string_array_contains(cur->before, tmp->id)) {
				printlog(LOG_DEBUG, "edge from %s to %s", cur->id, tmp->id);
				tmp->incoming_edges++;
			}
		}
	}

	/* Iteratively remove nodes with zero incoming edges */
	tail = NULL;
	while (!LIST_EMPTY(src)) {
		cur = NULL;
		LIST_FOREACH(tmp, src, entries) {
			if (tmp->incoming_edges == 0) {
				cur = tmp;
				break;
			}
		}

		if (cur) {
			/* Update edge counts to reflect the removal of <cur> */
			for (i = 0; i < string_array_len(cur->before); i++) {
				tmp = find_job_by_id(src, string_array_data(cur->before)[i]);
				if (tmp) {
					printlog(LOG_DEBUG, "removing edge from %s to %s", cur->id, tmp->id);
					tmp->incoming_edges--;
				}
			}
			LIST_FOREACH(tmp, src, entries) {
				if (cur != tmp && string_array_contains(tmp->after, cur->id)) {
					printlog(LOG_DEBUG, "removing edge from %s to %s", cur->id, tmp->id);
					tmp->incoming_edges--;
				}
			}

			/* Remove <cur> and place it on the sorted destination list */
			LIST_REMOVE(cur, entries);
			if (tail) {
				LIST_INSERT_AFTER(tail, cur, entries);
			} else {
				LIST_INSERT_HEAD(dest, cur, entries);
			}
			tail = cur;
			continue;
		} else {
			/* Any leftover nodes are part of a cycle. */
			LIST_FOREACH_SAFE(cur, src, entries, tmp) {
				LIST_REMOVE(cur, entries);
				printlog(LOG_WARNING, "job %s is part of a cycle", cur->id);
				cur->state = JOB_STATE_ERROR;
				LIST_INSERT_AFTER(tail, cur, entries);
				tail = cur;
			}
		}
	}

	return (0);
}