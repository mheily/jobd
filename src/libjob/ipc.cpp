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

	#include <fcntl.h>
	#include <sys/un.h>
	#include <sys/event.h>
	#include <sys/socket.h>
#include <unistd.h>
}

#include "namespaceImport.hpp"
#include "ipc.h"
#include "job.h"
#include "logger.h"

#if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
#error No mechanism available to ignore SIGPIPE
#endif

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
#define MSG_NOSIGNAL 0
#endif

namespace libjob {

// Launch jobd if it is not running
void ipcClient::bootstrapJobDaemon()
{
	// TODO: replace this horrible mess w/ actual config data
	std::string jobd_path =
#ifdef __linux__
			"/usr/sbin/jobd";
#else
			"/usr/local/sbin/jobd";
#endif

	if (access(jobd_path.c_str(), F_OK) < 0) {
		log_errno("access");
		throw "jobd path not found";
	}

	int fd = open(jobd_config.getRuntimeDir().c_str(), O_RDONLY);
	if (fd < 0) {
		log_errno("open");
		throw std::system_error(errno, std::system_category());
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		log_errno("flock");
		throw std::system_error(errno, std::system_category());
	}

	if (system(jobd_path.c_str()) < 0) {
		log_error("unable to execute %s", jobd_path.c_str());
		throw std::system_error(errno, std::system_category());
	}

	for (int i = 0; i < 10; i++) {
		if (access(jobd_config.getSocketPath().c_str(), F_OK) < 0) {
			sleep(1);
		} else {
			break;
		}
	}

	if (flock(fd, LOCK_UN) < 0) {
		log_errno("flock");
		throw std::system_error(errno, std::system_category());
	}

	(void) close(fd);
}

ipcClient::ipcClient()
{
	create_socket();
}

ipcServer::ipcServer(std::string path)
{
	socket_path = path;
	create_socket();
}

ipcClient::~ipcClient() {
	if (sockfd >= 0)
		(void) close(sockfd);
}

ipcServer::~ipcServer() {
	if (sockfd >= 0)
		(void) close(sockfd);
	if (socket_path != "")
		(void) unlink(socket_path.c_str());
}

void ipcClient::create_socket() {
	struct sockaddr_un sock;

	sock.sun_family = AF_LOCAL;
	strncpy(sock.sun_path, jobd_config.getSocketPath().c_str(), sizeof(sock.sun_path));

	sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd < 0)
		throw std::system_error(errno, std::system_category());

	//todo for linux
#if 0
	int on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof (on));
#endif

	bool bootstrap_done = false;
	for (int i = 0; i < 3; i++) {
		int rv = connect(sockfd, (struct sockaddr *) &sock, SUN_LEN(&sock));
		if (rv == 0)
			break;

		// KLUDGE: connrefused is racy. What if the socket is created but listen() has not been called yet?
		if (errno == ENOENT || errno == ECONNREFUSED) {
			if (!bootstrap_done) {
				bootstrapJobDaemon();
				bootstrap_done = true;
			}
			sleep(1);
		} else {
			log_errno("connect(2) to %s", jobd_config.getSocketPath().c_str());
			throw std::system_error(errno, std::system_category());
		}
	}
}

void ipcServer::create_socket()
{
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_LOCAL;
	strncpy(sa.sun_path, socket_path.c_str(), sizeof(sa.sun_path));

    sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd < 0) {
		log_errno("socket(2)");
		throw std::system_error(errno, std::system_category());
	}

	(void) unlink(socket_path.c_str());
	if (bind(sockfd, (struct sockaddr *) &sa, SUN_LEN(&sa)) < 0) {
		log_errno("bind(2)");
		throw std::system_error(errno, std::system_category());
	}

	if (listen(sockfd, 1024) < 0) {
        	log_errno("listen(2)");
        	throw std::system_error(errno, std::system_category());
    }
}

void ipcSession::readRequest() {
	sa_len = sizeof(server_sa);
	ssize_t bytes = read(sockfd, &buf, sizeof(buf));
	if (bytes < 0) {
		bufsz = 0;
		log_errno("read(2)");
		throw std::system_error(errno, std::system_category());
	}
	bufsz = bytes;
	try {
		request.parse(std::string(buf));
	} catch (...) {
		log_error("request parsing failed; buf=%s", buf);
		throw;
	}
}

ipcSession ipcServer::acceptConnection() {
	return ipcSession(sockfd, sa);
}

void ipcSession::sendResponse(jsonRpcResponse response) {
	auto buf = response.dump();

	log_debug("sending `%s' to %d", buf.c_str(), sockfd);
	if (send(sockfd, buf.c_str(), buf.length(), MSG_NOSIGNAL) < 0) {
		log_errno("sendto(2)");
		throw std::system_error(errno, std::system_category());
	}
}

void ipcSession::close() {
	if (sockfd >= 0) {
		log_debug("closing socket %d", sockfd);
		(void) ::close(sockfd);
		sockfd = -1;
	} else {
		log_warning("unnecessary call - socket is already closed");
	}
}

void ipcClient::dispatch(jsonRpcRequest request, jsonRpcResponse& response) {
	request.validate();
	std::string bufstr = request.dump();
	if (send(sockfd, bufstr.c_str(), bufstr.length() + 1, MSG_NOSIGNAL) < 0)
		throw std::system_error(errno, std::system_category());

	char cbuf[9999]; // XXX-HORRIBLE HARDCODED BUFFER SIZE
	ssize_t bytes = read(sockfd, &cbuf, sizeof(cbuf));
	if (bytes < 0 || bytes >= (ssize_t)sizeof(cbuf))
		throw std::system_error(errno, std::system_category());
	cbuf[bytes] = '\0';
	json j = json::parse(string(cbuf));
	//TODO: look at the validity of the response
	response.setResult(j["result"]);
}

ipcSession::ipcSession(int server_fd, struct sockaddr_un sa) {
	socklen_t sa_len = sizeof(sa);
        server_sa = sa;
        sockfd = accept(server_fd, (struct sockaddr *)&client_sa, &sa_len);
        if (sockfd < 0) {
        	log_errno("accept(2)");
                throw std::system_error(errno, std::system_category());
        }

#ifdef SO_NOSIGPIPE
	int flags = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &flags, sizeof(flags)) < 0) {
        	log_errno("setsockopt(2)");
                throw std::system_error(errno, std::system_category());
        }
#endif

        log_debug("accepted incoming connection on server fd %d, client fd %d",
        	server_fd, sockfd);
}

ipcSession::~ipcSession() {
	log_debug("closing session");
	if (sockfd >= 0)
		(void) ::close(sockfd);
}

void ipcServer::fork_handler()
{
	if (sockfd >= 0)
		(void) close(sockfd);
	socket_path = "";
}

} // namespace
