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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "logger.h"
#include "memory.h"
#include "ipc.h"

static int initialized;
static char *socketpath;
static struct sockaddr_un ipc_server_addr;
static int ipc_sockfd = -1;

int
ipc_init(void)
{
    if (initialized)
        return -1;

    if (jsonrpc_init() < 0)
        return -2;

    initialized = 1;

    return 0;
}

void ipc_shutdown(void)
{
    if (initialized) {
        jsonrpc_shutdown();
        close(ipc_sockfd);
        free(socketpath);
        initialized = 0;
    }
}

int
ipc_client_request(const char *job_id, const char *method)
{
    ssize_t bytes;
    struct sockaddr_un sa_to;
    socklen_t len;
    struct jsonrpc_request CLEANUP_JSONRPC_REQUEST *req = NULL;
    struct jsonrpc_response CLEANUP_JSONRPC_RESPONSE *response = NULL;

    if (!initialized)
        return -1;

    memset(&sa_to, 0, sizeof(struct sockaddr_un));
    sa_to.sun_family = AF_UNIX;
    strncpy(sa_to.sun_path, socketpath, sizeof(sa_to.sun_path) - 1);

    req = jsonrpc_request_new("1", method, 1, "job_id", job_id);
    if (!req)
        return printlog(LOG_ERR, "unable to allocate request");
    char CLEANUP_STR *buf = NULL;
    if (jsonrpc_request_serialize(&buf, req) < 0)
        return printlog(LOG_ERR, "serialization failed");
    len = (socklen_t) sizeof(struct sockaddr_un);
    bytes = sendto(ipc_sockfd, buf, strlen(buf), 0,
                   (const struct sockaddr *) &sa_to,
                   sizeof(sa_to));
    if (bytes < 0) {
        return printlog(LOG_ERR, "sendto(2): %s", strerror(errno));
    } else if ((size_t) bytes < sizeof(req)) {
        return printlog(LOG_ERR, "TODO - handle short write");
    }
    printlog(LOG_DEBUG, "sent IPC request: %s", buf);

    char resbuf[IPC_MAX_MSGLEN + 1];
    len = sizeof(struct sockaddr_un);
    bytes = recvfrom(ipc_sockfd, resbuf, sizeof(resbuf), 0,
                     (struct sockaddr *) &sa_to,
                     &len);
    if (bytes < 0)
        return printlog(LOG_ERR, "recvfrom(2): %s", strerror(errno));
    resbuf[bytes] = '\0';
    printlog(LOG_DEBUG, "<<< %s", resbuf);

    if (jsonrpc_response_parse(&response, resbuf, bytes) < 0)
        return printlog(LOG_ERR, "error parsing response");

    return (response->error.code);
}

static char *
_make_socketpath(const char *service)
{
    char *result;
    struct sockaddr_un saun;

    if (asprintf(&result, "%s/%s/%s.sock", compile_time_option.runstatedir,
                 compile_time_option.project_name, service) < 0) {
        printlog(LOG_ERR, "memory error");
        return NULL;
    }
    if (strlen(result) > sizeof(saun.sun_path) - 1) {
        printlog(LOG_ERR, "socket path is too long");
        free(result);
        return NULL;
    }
    return result;
}

static int
create_ipc_socket(const char *service)
{
    int sd;
    struct sockaddr_un saun;

    if (socketpath)
        return printlog(LOG_ERR, "socket already exists");

    socketpath = _make_socketpath(service);
    if (!socketpath)
        return printlog(LOG_ERR, "allocation failed");

    sd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sd < 0)
        return printlog(LOG_ERR, "socket(2): %s", strerror(errno));
    if (fcntl(sd, F_SETFD, FD_CLOEXEC) < 0) {
        printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));
        close(sd);
        return -1;
    }

    memset(&saun, 0, sizeof(saun));
    saun.sun_family = AF_UNIX;
    strncpy(saun.sun_path, socketpath, sizeof(saun.sun_path) - 1);
    memcpy(&ipc_server_addr, &saun, sizeof(ipc_server_addr));
    ipc_sockfd = sd;
    return 0;
}

int
ipc_bind(const char *service)
{
    int rv;

    if (create_ipc_socket(service) < 0)
        return -1;

    rv = bind(ipc_sockfd, (struct sockaddr *) &ipc_server_addr, sizeof(ipc_server_addr));
    if (rv < 0) {
        if (errno == EADDRINUSE) {
            unlink(socketpath);
            if (bind(ipc_sockfd, (struct sockaddr *) &ipc_server_addr, sizeof(ipc_server_addr)) < 0)
                return printlog(LOG_ERR, "bind(2) to %s: %s", socketpath, strerror(errno));
        } else {
            return printlog(LOG_ERR, "bind(2) to %s: %s", socketpath, strerror(errno));
        }
    }
    printlog(LOG_DEBUG, "bound to %s", socketpath);

    return 0;
}

int ipc_send_response(struct ipc_session *s, const struct ipc_result result)
{
    ssize_t bytes;
    struct jsonrpc_response CLEANUP_JSONRPC_RESPONSE *response = NULL;

    char *id = s->req ? s->req->id : NULL;
    response = jsonrpc_response_new(id);
    if (!response)
        return printlog(LOG_ERR, "error allocating response");
    if (result.code == 0) {
        if (jsonrpc_response_set_result(response, result.data) < 0)
            return printlog(LOG_ERR, "error building response");
    } else {
        if (jsonrpc_response_set_error(response, result.code, result.errmsg) < 0)
            return printlog(LOG_ERR, "error building response");
    }
    char CLEANUP_STR *buf = NULL;
    if (jsonrpc_response_serialize(&buf, response) < 0)
        return printlog(LOG_ERR, "serialization failed");

    printlog(LOG_DEBUG, ">>> %s", buf);
    bytes = sendto(ipc_sockfd, buf, strlen(buf), 0,
                   (struct sockaddr *) &s->client_addr, s->client_addrlen);
    if (bytes < 0) {
        printlog(LOG_ERR, "sendto(2): %s", strerror(errno));
        return (-1);
    }

    return (0);
}

int
ipc_read_request(struct ipc_session *session)
{
    char buf[IPC_MAX_MSGLEN];

    session->client_addrlen = sizeof(struct sockaddr_un);
    ssize_t bytes = recvfrom(ipc_sockfd, buf, sizeof(buf), 0,
                             (struct sockaddr *) &session->client_addr, &session->client_addrlen);
    if (bytes < 0)
        return printlog(LOG_ERR, "recvfrom(2): %s", strerror(errno));
    printlog(LOG_DEBUG, "<<< %s", buf);
    if (jsonrpc_request_parse(&session->req, buf, bytes) < 0) {
        (void) ipc_send_response(session, IPC_RES_ERR(-32600, "Invalid request"));
        return printlog(LOG_ERR, "unable to parse client request");
    }
    return 0;
}

int
ipc_connect(const char *service)
{
    struct sockaddr_un saun;

    if (create_ipc_socket(service) < 0)
        return -1;

    memset(&saun, 0, sizeof(saun));
    saun.sun_family = AF_UNIX;
    memset(saun.sun_path, 0, sizeof(saun.sun_path));
    if (bind(ipc_sockfd, (struct sockaddr *) &saun, sizeof(saun)) < 0)
        return printlog(LOG_ERR, "bind(2) to %s: %s", saun.sun_path, strerror(errno));

    return 0;
}

int
ipc_get_sockfd(void)
{
    return ipc_sockfd;
}

struct ipc_session * ipc_session_new(void)
{
    struct ipc_session *p;
    p = calloc(1, sizeof(*p));
    return p;
}

void ipc_session_destroy(struct ipc_session **s) {
    if (s && *s) {
        jsonrpc_request_free((*s)->req);
        free(*s);
        *s = NULL;
    }
}
