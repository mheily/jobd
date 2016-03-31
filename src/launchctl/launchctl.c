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

int setup_client_socket()
{
	char path[PATH_MAX];
	struct sockaddr_un sock;
	int fd;

	if (ipc_get_socket_path((char *)path, sizeof(path),
			RELAUNCHD_IPC_SERVICE, IPC_INTERFACE_DEFAULT) < 0) {
		return -1;
	}

	sock.sun_family = AF_LOCAL;
        strncpy(sock.sun_path, path, sizeof(sock.sun_path));

	fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0) {
		//client->last_error = IPC_CAPTURE_ERRNO;
		//log_errno("socket(2)");
		return -1;
	}

	if (connect(fd, (struct sockaddr *) &sock, SUN_LEN(&sock)) < 0) {
		//client->last_error = IPC_CAPTURE_ERRNO;
		//log_errno("connect(2) to %s", sock.sun_path);
		return -1;
	}

	return fd;
}

void usage() 
{
	printf("todo: usage\n");
}

void send_request(int sock, const char *command)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "ipc_version", 1); /* API version */
	nvlist_add_string(nvl, "service", "com.heily.relaunchd.launchctl"); /* IPC service name */
	nvlist_add_string(nvl, "method", command);
	if (nvlist_send(sock, nvl) < 0) {
	     nvlist_destroy(nvl);
	     err(1, "request failed");
	}
	nvlist_destroy(nvl);

	/* TODO: receive the response */
}

int
main(int argc, char *argv[])
{
	//int c;
	int sock;
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

	sock = setup_client_socket();
	if (sock < 0)
		errx(1, "unable to connect to launchd");
	send_request(sock, command);

	exit(EXIT_SUCCESS);
}
