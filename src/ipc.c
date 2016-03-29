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

#include <err.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/nv.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "log.h"
#include "util.h"

/* The main kqueue descriptor used by launchd */
static int parent_kqfd;

/* The socket descriptor used by the IPC server */
static int sock;

static void
setup_listen_socket()
{
	struct sockaddr_un name;
	char path[PATH_MAX];

	if (getuid() == 0) {
		path_sprintf(&path, "/var/run/launchd/ipc.sock");
		mkdir_idempotent("/var/run/launchd", 0700);
	} else {
		path_sprintf(&path, "%s/.launchd/run/ipc.sock", getenv("HOME"));
	}

	(void) unlink(path);

        name.sun_family = AF_LOCAL;
        strncpy(name.sun_path, path, sizeof(name.sun_path));

        sock = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (!sock)
                err(1, "socket");

        if (bind(sock, (struct sockaddr *) &name, SUN_LEN(&name)) < 0)
                err(1, "bind");

        if (listen(sock, 1024) < 0)
                err(1, "listen");
}

void setup_ipc_server(int kqfd)
{
	struct kevent kev;

	parent_kqfd = kqfd;
	setup_listen_socket();

	EV_SET(&kev, sock, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &setup_ipc_server);
	if (kevent(parent_kqfd, &kev, 1, NULL, 0, NULL) < 0)
		err(1, "kevent(2)");
}

static bool
validate_request(nvlist_t *nvl)
{
	if (!nvlist_exists_number(nvl, "ipc_version")) {
		log_error("invalid IPC request: version missing");
		return false;
	}
	if (!nvlist_exists_string(nvl, "service")) {
		log_error("invalid IPC request: service missing");
		return false;
	}
	if (!nvlist_exists_string(nvl, "service")) {
		log_error("invalid IPC request: method missing");
		return false;
	}
	return true;
}

int ipc_connection_handler()
{
	nvlist_t *nvl;
	const char *method;
        struct sockaddr sa;
        socklen_t sa_len;
        int client;

        client = accept(sock, &sa, &sa_len);
        if (client < 0) {
                log_errno("accept(2)");
                return 0;
        }

	nvl = nvlist_recv(client, 0);
	if (nvl == NULL) {
		log_error("nvlist_recv()");
		return 0;
	}

	if (!validate_request(nvl)) {
		log_error("bad IPC request");
		return 0;
	}

	method = nvlist_get_string(nvl, "method");

	printf("method=%s\n", method);
	nvlist_destroy(nvl);

	(void) close(client);

	return 0;
}
