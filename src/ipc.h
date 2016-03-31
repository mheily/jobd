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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define IPC_INTERFACE_DEFAULT ((void *)(0))

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

#endif /* _RELAUNCHD_IPC_H_ */
