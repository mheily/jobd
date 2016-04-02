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

#ifndef _RELAUNCHD_IPC_H_
#define _RELAUNCHD_IPC_H_


#define RELAUNCHD_IPC_SERVICE "com.heily.relaunchd"

void setup_ipc_server(int kqfd);
int ipc_connection_handler();

/* TODO: move everything below here to libipc */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/nv.h>
#include <sys/un.h>

#define IPC_INTERFACE_DEFAULT "__ipc_interface_default__"
#define IPC_NO_RETURN ((nvlist_t *)(NULL))

struct ipc_session {
	char *service;
	char *interface;
	int sockfd;
	int saved_errno;
};
typedef struct ipc_session *ipc_session_t;

static inline int
ipc_get_socket_path(char *buf, size_t bufsz, const char *service, const char *interface)
{
	const char *sys_ipcdir = "/var/run/ipc";
	int len;

	if (interface == IPC_INTERFACE_DEFAULT)
		interface = "__default__";

	if (getuid() == 0) {
		len = snprintf(buf, bufsz, "%s/%s/%s.sock", sys_ipcdir, service, interface);
	} else {
		len = snprintf(buf, bufsz, "%s/.ipc/%s/%s.sock", getenv("HOME"), service, interface);
	}
	if (len < 0 || len >= bufsz) {
        	return -1;
        }

        return 0;
}

static inline ipc_session_t
ipc_session_new()
{
	struct ipc_session *p;

	p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;
	p->sockfd = -1;

	return p;
}

static inline int
ipc_session_connect(ipc_session_t session, const char *service, const char *interface)
{
	char path[PATH_MAX];
	struct sockaddr_un sock;

	if (!session || !service || !interface)
		return -1;

	if (ipc_get_socket_path((char *)&path, sizeof(path), service, interface) < 0) {
		return -2;
	}
	session->service = strdup(service);
	session->interface = strdup(interface);
	if (!session->service || !session->interface) {
		free(session->service);
		free(session->interface);
		close(session->sockfd);
		return -3;
	}

	sock.sun_family = AF_LOCAL;
	strncpy(sock.sun_path, path, sizeof(sock.sun_path));

	session->sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (session->sockfd < 0) {
		session->saved_errno = errno;
		return -4;
	}

	if (connect(session->sockfd, (struct sockaddr *) &sock, SUN_LEN(&sock)) < 0) {
		session->saved_errno = errno;
		return -5;
	}

	return 0;
}

static inline int
ipc_request(ipc_session_t session, const char *method, const nvlist_t *args, nvlist_t *rets)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "ipc_version", 1);
	nvlist_add_string(nvl, "service", session->service);
	nvlist_add_string(nvl, "interface", session->interface);
	nvlist_add_string(nvl, "method", method);
	if (args) {
		nvlist_add_nvlist(nvl, "args", args);
	}
	if (rets) {
		nvlist_add_nvlist(nvl, "rets", rets);
	}
	if (nvlist_send(session->sockfd, nvl) < 0) {
		printf("nvlist error: %d\n", nvlist_error(nvl));
	     nvlist_destroy(nvl);
	     return -1;
	}
	nvlist_destroy(nvl);
	return 0;
}

static inline const char *
ipc_strerror(ipc_session_t session)
{
	return strerror(session->saved_errno);
}

#endif /* _RELAUNCHD_IPC_H_ */
