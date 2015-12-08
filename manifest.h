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

#ifndef __MANIFEST_H__
#define __MANIFEST_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include "vendor/FreeBSD/sys/queue.h"
#include "cvec.h"
#include "socket.h"

struct job_manifest {
	LIST_ENTRY(job_manifest) jm_le;

	/* The original JSON manifest. Will be freed along with this structure */
	char	*json_buf;
	size_t	 json_size;

	char 	*label;
	//not implemented: Disabled key
	bool     job_is_agent; /* Temporary hack to detect agents v.s. daemons */
	char 	*user_name;
	char 	*group_name;

	char	*program;
	cvec_t 	program_arguments;

	bool	enable_globbing,
			run_at_load;
	char	*working_directory;
	char 	*root_directory;
	char *	jail_name;

	cvec_t	environment_variables;  // Hash coerced into a flat array of key/value pairs

	uint32_t umask,
			timeout,	//TODO: figure out the default value for this
			exit_timeout,
			start_interval,
			throttle_interval,
			nice;
	bool	init_groups;
	cvec_t	watch_paths;
	cvec_t	queue_directories;
	bool	start_on_mount;
	char	*stdin_path,
			*stdout_path,
			*stderr_path;
	bool	abandon_process_group;
	// TODO: ResourceLimits, HopefullyExits*, KeepAlive, inetd, cron, LowPriorityIO, LaunchOnlyOnce
	SLIST_HEAD(,job_manifest_socket) sockets;
	int32_t refcount;
};
typedef struct job_manifest *job_manifest_t;

job_manifest_t job_manifest_new();
void job_manifest_free(job_manifest_t jm);
int job_manifest_read(job_manifest_t jm, const char *infile);
int job_manifest_parse(job_manifest_t jm, char *buf, size_t bufsz);
static inline void job_manifest_retain(job_manifest_t jm)
{
	jm->refcount++;
}

static inline void job_manifest_release(job_manifest_t jm)
{
	jm->refcount--;
	if (jm->refcount < 0)
		job_manifest_free(jm);
}

#endif /* __MANIFEST_H__ */
