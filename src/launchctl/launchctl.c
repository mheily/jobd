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

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <sys/nv.h>

#include "ipc.h"
#include "options.h"
#include "job.h"
#include "util.h"

static ipc_session_t launchd_session;

void setup_client_socket()
{
	launchd_session = ipc_session_new();
	if (!launchd_session)
		errx(1, "ipc_session_new()");

	if (ipc_session_connect(launchd_session, RELAUNCHD_IPC_SERVICE, IPC_INTERFACE_DEFAULT) < 0)
		errx(1, "ipc_session_connect()");
}

void usage() 
{
	printf("todo: usage\n");
}

void do_command_load(const char *argv[])
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	//todo add args

	if (ipc_request(launchd_session, "load", nvl, IPC_NO_RETURN) < 0)
		errx(1, "ipc_request() error; sd=%d", launchd_session->sockfd);

	nvlist_destroy(nvl);

	/* TODO: receive the response */
}

int
main(int argc, const char *argv[])
{
	//int c;
	const char *command;

	/* Sanitize environment variables */
	if ((getuid() != 0) && (access(getenv("HOME"), R_OK | W_OK | X_OK) < 0)) {
		fputs("Invalid value for the HOME environment variable\n", stderr);
		exit(1);
	}

	if (argc == 1)
		errx(1, "invalid usage");
	command = argv[1];
	/* TODO: parse flags and such
	for (int i = 2; i < argc; i++)
	*/

	setup_client_socket();

	do_command_load(argv);

	exit(EXIT_SUCCESS);
}
