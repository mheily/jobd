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

struct ipc_request {
	enum {
		IPC_REQUEST_UNDEFINED,
		IPC_REQUEST_START,
		IPC_REQUEST_STOP,
		IPC_REQUEST_ENABLE,
		IPC_REQUEST_DISABLE,
		IPC_REQUEST_MAX, /* Not a real opcode, just setting the maximum number of codes */
	} opcode;
	char job_id[JOB_ID_MAX + 1];
};

struct ipc_response {
	enum {
		IPC_RESPONSE_OK,
		IPC_RESPONSE_ERROR,
		IPC_RESPONSE_NOT_FOUND,
		IPC_RESPONSE_INVALID_STATE,
	} retcode;
};

int ipc_init(const char *_socketpath);
int ipc_bind(void);
int ipc_connect(void);
int ipc_client_request(int opcode, char *job_id);
int ipc_get_sockfd(void);

#endif /* _IPC_H */
