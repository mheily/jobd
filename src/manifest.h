/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 * Copyright (c) 2015 Steve Gerbino <steve@gerbino.co>
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

#ifndef _MANIFEST_H_
#define _MANIFEST_H_

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "../vendor/FreeBSD/sys/queue.h"
#include "cvec.h"
#include "socket.h"

/** A wildcard value in a crontab(5) specification */
#define CRON_SPEC_WILDCARD UINT32_MAX

struct cron_spec {
	uint32_t minute;
	uint32_t hour;
	uint32_t day;
	uint32_t weekday;
	uint32_t month;
};

typedef struct job_manifest {
	LIST_ENTRY(job_manifest) jm_le;

	char    *label;

	char    *user_name;
	char    *group_name;

	bool     job_is_agent; /* Temporary hack to detect agents v.s. daemons */

	char	*program;
	cvec_t 	 program_arguments;

	bool	 enable_globbing;
	bool     run_at_load;
	char	*working_directory;
	char 	*root_directory;
	char    *jail_name;

	cvec_t	 environment_variables;

	mode_t   umask;
	uint32_t timeout;
	uint32_t exit_timeout;
	uint32_t start_interval;
	uint32_t throttle_interval;
	uint32_t nice;
	bool	 init_groups;
	cvec_t	 watch_paths;
	cvec_t	 queue_directories;
	bool	 start_on_mount;
	char	*stdin_path;
	char	*stdout_path;
	char    *stderr_path;
	bool	 abandon_process_group;
	bool     start_calendar_interval;
	struct cron_spec calendar_interval;
	struct {
		bool always; /* Equivalent to setting { "KeepAlive": true } */
		/* TODO: various other conditions */
	} keep_alive;

	// TODO: ResourceLimits, HopefullyExits*, inetd, LowPriorityIO, LaunchOnlyOnce
	SLIST_HEAD(,job_manifest_socket) sockets;
	int32_t refcount;
} *job_manifest_t;

job_manifest_t job_manifest_new(void);
void job_manifest_free(job_manifest_t job_manifest);
int job_manifest_read(job_manifest_t job_manifest, const char *filename);
int job_manifest_parse(job_manifest_t job_manifest, unsigned char *buf, size_t bufsize);

#endif /* _MANIFEST_H_ */
