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

#include <dlfcn.h>
#include <err.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct sock_info {
	int fd;
	struct sockaddr_in sa;
	socklen_t sa_len;
};

static void wrapper_init() __attribute__((constructor));
/* TODO: write a destructor */

static int * (*libc_bind_ptr)(int, const struct sockaddr *, socklen_t);
static struct sock_info **s_info;
static size_t s_cnt;

static void wrapper_init()
{
	void *handle;

	/* FIXME: hardcoded to FreeBSD 10 */
	handle = dlopen("libc.so.7", RTLD_LAZY);
	if (!handle) err(1, "dlopen failed");
	*(void **)(&libc_bind_ptr) = dlsym(handle, "bind");
	if (!libc_bind_ptr) err(1, "dlsym failed: %s", dlerror());

	/* FIXME: parse environment variables for launchd */
	s_cnt = 1;
	s_info = calloc(s_cnt, sizeof(struct sock_info));
	for (size_t i = 0; i < s_cnt; i++) {
		s_info[0]->fd = 3; //FIXME: read env var
		s_info[0]->sa_len = sizeof(struct sockaddr);
		if (getsockname(s_info[0]->fd, (struct sockaddr *) &s_info[0]->sa, &s_info[0]->sa_len) < 0) {
			err(1, "getsockname");
		}
	}
}

int
bind(int s, const struct sockaddr *addr, socklen_t addrlen)
{
	int newfd;
	struct sockaddr_in *sain = (struct sockaddr_in *) addr;
	size_t i;

	for (i = 0; i < s_cnt; i++) {
		if (sain->sin_port == s_info[i]->sa.sin_port) {
			/* replace the socket with the one inherited
				from launchd. */

			// TODO: get socket flags of s
			if (dup2(s_info[i]->fd, s) < 0) {
				return -1;
			}
			// TODO: restore socket flags
		
			return 0;
		}
	}

	return *(*libc_bind_ptr)(s, addr, addrlen);
}

int main() {
  int sd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (sd < 0) err(1, "socket");
	if (bind(sd, NULL, 0) < 0) {
		err(1, "bind");
	}
  puts("ok");
  exit(0);
}
