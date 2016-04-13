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

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include "../vendor/FreeBSD/sys/queue.h"
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include "config.h"
#include "calendar.h"
#include "log.h"
#include "manager.h"
#include "manifest.h"
#include "options.h"
#include "job.h"
#include "pidfile.h"
#include "socket.h"
#include "timer.h"
#include "uset.h"
#include "util.h"

struct launchd_options options;

static struct {
	struct pidfh *pfh;
} state;

void usage() 
{
	printf("todo: usage\n");
}

void create_pid_file()
{
	pid_t otherpid;

	if (getuid() == 0) {
		path_sprintf(&options.pidfile, "/var/run/launchd.pid");
	} else {
		char statedir[PATH_MAX];

		path_sprintf(&statedir, "%s/.launchd", getenv("HOME"));
		mkdir_idempotent(statedir, 0700);
		path_sprintf(&statedir, "%s/.launchd/run", getenv("HOME"));
		mkdir_idempotent(statedir, 0700);

		path_sprintf(&options.pidfile, "%s/launchd.pid", statedir);
	}

	state.pfh = pidfile_open(options.pidfile, 0600, &otherpid);
	if (state.pfh == NULL) {
		if (errno == EEXIST) {
			fprintf(stderr, "WARNING: Daemon already running, pid: %jd.\n", (intmax_t) otherpid);
		} else {
			fprintf(stderr, "ERROR: Cannot open or create pidfile: %s\n", options.pidfile);
		}
		exit(EXIT_FAILURE);
	}
}

#ifndef UNIT_TEST
int
main(int argc, char *argv[])
{
	int c;

	/* Sanitize environment variables */
	if ((getuid() != 0) && (access(getenv("HOME"), R_OK | W_OK | X_OK) < 0)) {
		fputs("Invalid value for the HOME environment variable\n", stderr);
		exit(1);
	}

	options.daemon = true;
	options.log_level = LOG_NOTICE;

	while ((c = getopt(argc, argv, "fv")) != -1) {
			switch (c) {
			case 'f':
					options.daemon = false;
					break;
			case 'v':
					options.log_level = LOG_DEBUG;
					break;
			default:
					usage();
					break;
			}
	}

	create_pid_file();

/* daemon(3) is deprecated on MacOS */
//DISABLED: gcc hates these pragmas and will not compile
//pragma clang diagnostic push
//pragma clang diagnostic ignored "-Wdeprecated-declarations"

	if (options.daemon && daemon(0, 0) < 0) {
		fprintf(stderr, "ERROR: Unable to daemonize\n");
		pidfile_remove(state.pfh);
		exit(EX_OSERR);
	} else {
		log_freopen(stdout);
	}

//pragma clang diagnostic pop

	pidfile_write(state.pfh);

	manager_init(state.pfh);
	manager_update_jobs();
	manager_main_loop();

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
#endif /* !UNIT_TEST */
