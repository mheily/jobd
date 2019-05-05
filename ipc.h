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

#ifndef _IPC_H
#define _IPC_H

#include <sys/socket.h>
#include <sys/un.h>

#include "jsonrpc.h"

/* Maximum length of an IPC message */
#define IPC_MAX_MSGLEN  32768U

struct ipc_response {
    enum {
        IPC_RESPONSE_OK,
        IPC_RESPONSE_ERROR,
        IPC_RESPONSE_NOT_FOUND,
        IPC_RESPONSE_INVALID_STATE,
    } retcode;
};

struct ipc_result {
    int code;
    char *data;
    char *errmsg;
};

#define IPC_RES(_code, _data, _errmsg) ((struct ipc_result) { _code, _data, _errmsg })
#define IPC_RES_OK                    ((struct ipc_result) { 0, "{}", NULL })
#define IPC_RES_DATA(_data)        ((struct ipc_result) { 0, _data, NULL })
#define IPC_RES_ERR(_code, _errmsg) ((struct ipc_result) { _code, NULL, _errmsg })

struct ipc_session {
    struct sockaddr_un client_addr;
    socklen_t client_addrlen;
    struct jsonrpc_request *req;
};

int ipc_init(void);
void ipc_shutdown(void);

int ipc_bind(const char *service);

int ipc_connect(const char *service);

int ipc_client_request(const char *job_id, const char *method);

int ipc_read_request(struct ipc_session *s);

int ipc_send_response(struct ipc_session *s, struct ipc_result res);

struct ipc_session * ipc_session_new(void);

void ipc_session_destroy(struct ipc_session **s);
#define CLEANUP_IPC_SESSION __attribute__((__cleanup__(ipc_session_destroy)))

int ipc_get_sockfd(void);

#endif /* _IPC_H */
