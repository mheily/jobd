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

#include <err.h>
#include <errno.h>
// #include <fcntl.h>
// #include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include "logger.h"
#include "ipc.h"

static int initialized;
static char *socketpath;
static struct sockaddr_un ipc_server_addr;
static int ipc_sockfd = -1;

int
ipc_init(const char *_socketpath)
{
	if (initialized)
		return (-1);

	if (_socketpath) {
		socketpath = strdup(_socketpath);
	} else {
		if (getuid() == 0) {
			socketpath = strdup("/var/run/jobd.sock");
		} else {
			if (asprintf(&socketpath, "%s/.jobd.sock", getenv("HOME")) < 0)
				socketpath = NULL;
		}
	}
	if (!socketpath)
		return (-1);

	initialized = 1;

	return (0);
}

int
ipc_client_request(int opcode, char *job_id)
{
	ssize_t bytes;
	struct sockaddr_un sa_to, sa_from;
	socklen_t len;
	struct ipc_request req;
	struct ipc_response res;

	if (!initialized)
		return (-1);

	memset(&sa_to, 0, sizeof(struct sockaddr_un));
    sa_to.sun_family = AF_UNIX;
	if (strlen(socketpath) > sizeof(sa_to.sun_path) - 1)
		errx(1, "socket path is too long");
    strncpy(sa_to.sun_path, socketpath, sizeof(sa_to.sun_path) - 1);

	req.opcode = opcode;
	if (job_id) {
		strncpy((char*)&req.job_id, job_id, JOB_ID_MAX);
		req.job_id[JOB_ID_MAX] = '\0';
	} else {
		req.job_id[0] = '\0';
	}
	len = (socklen_t) sizeof(struct sockaddr_un);
	if (sendto(ipc_sockfd, &req, sizeof(req), 0, (struct sockaddr*) &sa_to, len) < 0) {
		err(1, "sendto(2)");
	}
	printlog(LOG_DEBUG, "sent IPC request; opcode=%d job_id=%s", req.opcode, req.job_id);

	len = sizeof(struct sockaddr_un);
	bytes = recvfrom(ipc_sockfd, &res, sizeof(res), 0, (struct sockaddr *) &sa_from, &len);
    if (bytes < 0) {
		err(1, "recvfrom(2)");
	}
	printlog(LOG_DEBUG, "got IPC response; retcode=%d", res.retcode);
	return (res.retcode);
}

static int
create_ipc_socket(void)
{
	int sd;
	struct sockaddr_un saun;

	if (strlen(socketpath) > sizeof(saun.sun_path) - 1) {
		printlog(LOG_ERR, "socket path is too long");
		return (-1);
	}

    sd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sd < 0) {
		printlog(LOG_ERR, "socket(2): %s", strerror(errno));
		return (-1);
	}

	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	strncpy(saun.sun_path, socketpath, sizeof(saun.sun_path) - 1);
	memcpy(&ipc_server_addr, &saun, sizeof(ipc_server_addr));//FIXME: is this used?
	ipc_sockfd = sd;

	return (0);
}

int
ipc_bind(void)
{
	int rv;

	if (create_ipc_socket() < 0)
		return (-1);
	
	rv = bind(ipc_sockfd, (struct sockaddr *) &ipc_server_addr, sizeof(ipc_server_addr));
	if (rv < 0) {
		if (errno == EADDRINUSE) {
			//TODO: check for multiple jobd instances
			unlink(socketpath);
			if (bind(ipc_sockfd, (struct sockaddr *) &ipc_server_addr, sizeof(ipc_server_addr)) == 0) {
				return (0);
			}				
		}
		printlog(LOG_ERR, "bind(2): %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
ipc_connect(void)
{
	struct sockaddr_un saun;

	if (create_ipc_socket() < 0)
		return (-1);
      
	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	if (bind(ipc_sockfd, (struct sockaddr *) &saun, sizeof(saun)) < 0) {
		printlog(LOG_ERR, "bind(2): %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
ipc_get_sockfd(void)
{
	return (ipc_sockfd);
}