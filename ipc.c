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
#include <fcntl.h>
// #include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include "config.h"
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
		if (asprintf(&socketpath, "%s/%s/jobd.sock", compile_time_option.runstatedir, compile_time_option.project_name) < 0)
			socketpath = NULL;
	}
	if (!socketpath)
		return (-1);

	initialized = 1;

	return (0);
}

int
ipc_client_request(const char *job_id, const char *method)
{
	ssize_t bytes;
	struct sockaddr_un sa_to;
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

	strncpy(req.method, method, sizeof(req.method));
	req.method[sizeof(req.method) - 1] = '\0';

	strncpy((char*)&req.job_id, job_id, JOB_ID_MAX);
	req.job_id[sizeof(req.job_id) - 1] = '\0';

	len = (socklen_t) sizeof(struct sockaddr_un);
	bytes = write(ipc_sockfd, &req, sizeof(req));
	if (bytes < 0) {
		err(1, "write(2)");
	} else if (bytes < len) {
		err(1, "TODO - handle short write");
	}
	printlog(LOG_DEBUG, "sent IPC request: %s::%s()",req.job_id, req.method);

	len = sizeof(struct sockaddr_un);
	bytes = read(ipc_sockfd, &res, sizeof(res));
    if (bytes < 0) {
		err(1, "recvfrom(2)");
	} else if ((size_t)bytes < sizeof(res)) {
		err(1, "TODO - handle short read of %zu bytes", bytes);
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

    sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd < 0) {
		printlog(LOG_ERR, "socket(2): %s", strerror(errno));
		return (-1);
	}
	if (fcntl(sd, F_SETFD, FD_CLOEXEC) < 0) {
		printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));
		close(sd);
		return (-1);
	}

	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	strncpy(saun.sun_path, socketpath, sizeof(saun.sun_path) - 1);
	memcpy(&ipc_server_addr, &saun, sizeof(ipc_server_addr));//FIXME: is this used?
	ipc_sockfd = sd;
	printlog(LOG_DEBUG, "bound to %s", socketpath);
	return (0);
}

static int
_ipc_listen(int sd)
{
	int rv;

	rv = listen(sd, 512);
	if (rv < 0) {
		printlog(LOG_ERR, "listen(2)");
		return (-1);		
	}

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
			unlink(socketpath);
			if (bind(ipc_sockfd, (struct sockaddr *) &ipc_server_addr, sizeof(ipc_server_addr)) == 0) {
				return (_ipc_listen(ipc_sockfd));
			}				
		}
		printlog(LOG_ERR, "bind(2) to %s: %s", socketpath, strerror(errno));
		return (-1);
	}

	return (_ipc_listen(ipc_sockfd));
}

int ipc_send_response(struct ipc_session *s)
{
	ssize_t bytes;

	printlog(LOG_DEBUG, "sending IPC response; retcode=%d", s->res.retcode);
	bytes = write(s->client_fd, &s->res, sizeof(s->res));
	if (bytes < 0) {
		printlog(LOG_ERR, "write(2): %s", strerror(errno));
		(void)close(s->client_fd);
		return (-1);
	}

	printlog(LOG_DEBUG, "closing IPC client session %d", s->client_fd);
	if (close(s->client_fd) < 0) {
		printlog(LOG_ERR, "close(2): %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
ipc_read_request(struct ipc_session *session)
{
	ssize_t bytes;
	struct sockaddr_un client_addr;
	socklen_t len;
	
	int addrlen = sizeof(struct sockaddr_un);
	session->client_fd = accept(ipc_get_sockfd(), (struct sockaddr *)&client_addr, (socklen_t*) &addrlen);
	if (session->client_fd < 0) {
		printlog(LOG_ERR, "accept(2): %s", strerror(errno));
		return (-1);
	}

	len = sizeof(struct sockaddr_un);
	bytes = read(session->client_fd, &session->req, sizeof(session->req));
    if (bytes < 0) {
		printlog(LOG_ERR, "read(2): %s", strerror(errno));
		(void)close(session->client_fd);
		return (-1);
	} else if (bytes < len) {
		printlog(LOG_ERR, "read(2): expected %zu bytes but got %zu bytes",
			(size_t)len, (size_t) bytes);
		(void)close(session->client_fd);
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
	strncpy(saun.sun_path, socketpath, sizeof(saun.sun_path) - 1);
	if (connect(ipc_sockfd, (struct sockaddr *) &saun, sizeof(saun)) < 0) {
		printlog(LOG_ERR, "connect(2) to %s: %s", saun.sun_path, strerror(errno));
		return (-1);
	}

	return (0);
}

int
ipc_get_sockfd(void)
{
	return (ipc_sockfd);
}