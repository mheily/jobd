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
#include <nlohmann/json.hpp>

extern "C" {
	#include <sys/types.h>

	#include <sys/un.h>
	#include <sys/event.h>
	#include <sys/socket.h>
#include <unistd.h>
}

#include "ipc.h"
#include "logger.h"

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
	log_debug("shutting down IPC server");
	if (this->sockfd >= 0)
		(void) close(this->sockfd);
	if (this->socket_path != "")
		(void) unlink(this->socket_path.c_str());
}

void ipcClient::create_socket() {
	struct sockaddr_un sock;

        sock.sun_family = AF_LOCAL;
        strncpy(sock.sun_path, this->socket_path.c_str(), sizeof(sock.sun_path));

        this->sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (this->sockfd < 0)
        	throw std::system_error(errno, std::system_category());

        //todo for linux
#if 0
        int on = 1;
        setsockopt(this->sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof (on));
#endif

        if (connect(this->sockfd, (struct sockaddr *) &sock, SUN_LEN(&sock)) < 0)
        	throw std::system_error(errno, std::system_category());
}

void ipcServer::create_socket()
{
	memset(&this->sa, 0, sizeof(this->sa));
        this->sa.sun_family = AF_LOCAL;
        strncpy(this->sa.sun_path, this->socket_path.c_str(), sizeof(this->sa.sun_path));

        this->sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (this->sockfd < 0) {
        	log_errno("socket(2)");
        	throw std::system_error(errno, std::system_category());
        }
        (void) unlink(this->socket_path.c_str());
        if (bind(this->sockfd, (struct sockaddr *) &this->sa, SUN_LEN(&this->sa)) < 0) {
        	log_errno("bind(2)");
        	throw std::system_error(errno, std::system_category());
        }
        if (listen(this->sockfd, 1024) < 0) {
        	log_errno("listen(2)");
        	throw std::system_error(errno, std::system_category());
        }
}

void ipcSession::readRequest() {
	this->sa_len = sizeof(this->server_sa);
	ssize_t bytes = read(this->sockfd, &this->buf, sizeof(this->buf));
	if (bytes < 0) {
		this->bufsz = 0;
		log_errno("read(2)");
		throw std::system_error(errno, std::system_category());
	}
	this->bufsz = bytes;
	try {
		this->request.parse(std::string(buf));
	} catch (...) {
		log_error("request parsing failed; buf=%s", buf);
		throw;
	}
}

ipcSession ipcServer::acceptConnection() {
	return ipcSession(this->sockfd, this->sa);
}

void ipcSession::sendResponse(jsonRpcResponse response) {
	auto buf = response.dump();

	log_debug("sending `%s' to %d", buf.c_str(), this->sockfd);
	if (write(this->sockfd, buf.c_str(), buf.length()) < 0) {
		log_errno("sendto(2)");
		throw std::system_error(errno, std::system_category());
	}
}

void ipcSession::close() {
	if (this->sockfd >= 0) {
		log_debug("closing socket %d", this->sockfd);
		(void) ::close(this->sockfd);
		this->sockfd = -1;
	} else {
		log_warning("unnecessary call - socket is already closed");
	}
}

void ipcClient::dispatch(jsonRpcRequest request, jsonRpcResponse& response) {
	request.validate();
	std::string bufstr = request.dump();
	if (write(this->sockfd, bufstr.c_str(), bufstr.length() + 1) < 0)
		throw std::system_error(errno, std::system_category());

	char cbuf[9999]; // XXX-HORRIBLE HARDCODED BUFFER SIZE
	ssize_t bytes = read(this->sockfd, &cbuf, sizeof(cbuf));
	if (bytes < 0)
		throw std::system_error(errno, std::system_category());
	//TODO: set response
}

ipcSession::ipcSession(int server_fd, struct sockaddr_un sa) {
        this->server_sa = sa;
        this->sockfd = accept(server_fd, (struct sockaddr *)&this->client_sa, &sa_len);
        if (this->sockfd < 0) {
        	log_errno("accept(2)");
                throw std::system_error(errno, std::system_category());
        }
        log_debug("accepted incoming connection on server fd %d, client fd %d",
        	server_fd, this->sockfd);
}

ipcSession::~ipcSession() {
	log_debug("closing session");
	if (this->sockfd >= 0)
		(void) ::close(this->sockfd);
}

} // namespace
