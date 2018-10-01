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

#include "job.h" // just for JOB_ID_MAX

/* Maximum length of a method name */
#define JOB_METHOD_NAME_MAX	128

/* Maximum length of arguments to a method */
#define JOB_METHOD_ARG_MAX	512

struct ipc_request {
	char job_id[JOB_ID_MAX];
	char method[JOB_METHOD_NAME_MAX];
	char args[JOB_METHOD_ARG_MAX];
};

struct ipc_response {
	enum {
		IPC_RESPONSE_OK,
		IPC_RESPONSE_ERROR,
		IPC_RESPONSE_NOT_FOUND,
		IPC_RESPONSE_INVALID_STATE,
	} retcode;
};

struct ipc_session {
	int client_fd;
	struct ipc_request req;
	struct ipc_response res;
};

int ipc_init(const char *_socketpath);
int ipc_bind(void);
int ipc_connect(void);
int ipc_client_request(const char *job_id, const char *method);
int ipc_read_request(struct ipc_session *s);
int ipc_send_response(struct ipc_session *s);
int ipc_get_sockfd(void);

#endif /* _IPC_H */
