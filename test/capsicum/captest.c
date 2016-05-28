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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <job/descriptor.h>

int main()
{
	int server;
	int client;
	struct sockaddr_in server_sa;
	struct sockaddr_storage client_sa;
	socklen_t client_sa_size;

	memset(&server_sa, 0, sizeof(server_sa));
	server_sa.sin_family = AF_INET;
	server_sa.sin_port = htons(7654);
	server_sa.sin_addr.s_addr = inet_addr("127.0.0.1");

	if ((server = job_descriptor_get("server_socket")) < 0) {
		server = socket(PF_INET, SOCK_STREAM, 0);
	}
	if (server < 0)
		err(1, "socket");
	if (bind(server, (struct sockaddr *) &server_sa, sizeof(server_sa)) < 0)
		err(1, "bind");
	if (listen(server, 1) < 0)
		err(1, "listen");

	client_sa_size = sizeof(client_sa);
	client = accept(server, (struct sockaddr *) &client_sa, &client_sa_size);

	if (send(client, "hello\r\n", 7, 0) < 0)
		err(1, "send");

	puts("done");
}
