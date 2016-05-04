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

#include <iostream>
#include <system_error>

extern "C" {
	#include <sys/types.h>

	#include <sys/un.h>
	#include <sys/event.h>
	#include <sys/socket.h>
#include <unistd.h>
}

#include "ipc.h"

namespace libjob {

ipcClient::ipcClient(std::string path) {
	this->socket_path = path;
	this->create_socket();
}

ipcServer::ipcServer(std::string path) {
	this->socket_path = path;
	this->create_socket();
}

ipcClient::~ipcClient() {
	if (this->sockfd >= 0)
		(void) close(this->sockfd);
}

ipcServer::~ipcServer() {
	std::cout << "ipc server shutdown:" << this->sockfd << '\n';
	if (this->sockfd >= 0)
		(void) close(this->sockfd);
	if (this->socket_path != "")
		(void) unlink(this->socket_path.c_str());
}

void ipcClient::create_socket() {
	struct sockaddr_un sock;

        sock.sun_family = AF_LOCAL;
        strncpy(sock.sun_path, this->socket_path.c_str(), sizeof(sock.sun_path));

        this->sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
        if (this->sockfd < 0)
        	throw std::system_error(errno, std::system_category());

        if (connect(this->sockfd, (struct sockaddr *) &sock, SUN_LEN(&sock)) < 0)
        	throw std::system_error(errno, std::system_category());
}

void ipcServer::create_socket()
{
        struct sockaddr_un name;

        name.sun_family = AF_LOCAL;
        strncpy(name.sun_path, this->socket_path.c_str(), sizeof(name.sun_path));

        this->sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
        if (this->sockfd < 0)
        	throw std::system_error(errno, std::system_category());

        if (bind(this->sockfd, (struct sockaddr *) &name, SUN_LEN(&name)) < 0)
        	throw std::system_error(errno, std::system_category());
}

std::string ipcServer::parse_request() {
	char request[4096];

	ssize_t bytes = recv(this->sockfd, &request, sizeof(request), 0);
	if (bytes < 0)
		throw std::system_error(errno, std::system_category());
	return std::string(request);
}

std::string ipcClient::request(std::string buf) {
	char response[4096];

	if (send(this->sockfd, buf.c_str(), buf.length(), 0) < 0)
		throw std::system_error(errno, std::system_category());

	ssize_t bytes = recv(this->sockfd, &response, sizeof(response), 0);
	if (bytes < 0)
		throw std::system_error(errno, std::system_category());
	return std::string(response);
}



} // namespace
