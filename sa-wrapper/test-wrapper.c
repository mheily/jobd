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
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
	struct sockaddr_in sa, client_sa;
	int socklen, client_fd;

	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0)
		err(1, "socket");
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(8088);

	if (bind(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		err(1, "bind(2)");
	}

	if (listen(sd, 5) < 0)
		err(1, "listen(2)");

	socklen = sizeof(client_sa);
	client_fd = accept(sd, (struct sockaddr *) &client_sa, &socklen);
	if (client_fd < 0)
	  err(1, "accept(2)");

	if (write(client_fd, "hello world", 11) < 11)
		err(1, "write(2)");

	exit(0);
}
