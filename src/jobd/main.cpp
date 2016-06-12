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

#include <iostream>
#include <string>

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
#include "../../vendor/FreeBSD/sys/queue.h"
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include "config.h"
#include "calendar.h"
#include "daemon.h"
#include <libjob/logger.h>
#include "manager.h"
#include "manifest.h"
#include "options.h"
#include "job.h"
#include "pidfile.h"
#include "socket.h"
#include "timer.h"
#include "util.h"

JobManager manager;

extern launchd_options_t options;

static struct {
	struct pidfh *pfh;
} state;

void usage() 
{
	printf("todo: usage\n");
}

void create_pid_file(const std::string& pidfilePath)
{
	pid_t otherpid;

	state.pfh = pidfile_open(pidfilePath.c_str(), 0600, &otherpid);
	if (state.pfh == NULL) {
		if (errno == EEXIST) {
			fprintf(stderr, "WARNING: Daemon already running, pid: %jd.\n", (intmax_t) otherpid);
		} else {
			fprintf(stderr, "ERROR: Cannot open or create pidfile: %s\n", options.pidfile);
		}
		exit(EXIT_FAILURE);
	}
}

int
main(int argc, char *argv[])
{
	libjob::jobdConfig jobd_config;
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

	create_pid_file(jobd_config.getPidfilePath());

	if (options.daemon && compat_daemon(0, 0) < 0) {
		fprintf(stderr, "ERROR: Unable to daemonize\n");
		pidfile_remove(state.pfh);
		exit(EX_OSERR);
	} else {
		log_freopen(stdout);
	}

	pidfile_write(state.pfh);

	try {
		manager.setup(state.pfh);
		manager.mainLoop();
	}
	catch (std::exception& e) {
		std::cout << "Caught fatal exception: " << e.what() << std::endl;
		abort();
	}
	catch (...) {
		std::cout << "Caught unknown exception; exiting" << std::endl;
		abort();
	}

	/* NOTREACHED */
	exit(EXIT_SUCCESS);
}
