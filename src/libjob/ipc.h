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

#pragma once

#include <string>
#include <nlohmann/json.hpp>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
}

#include "jsonRPC.hpp"

namespace libjob {

	using json = nlohmann::json;

	class ipcSession {
	public:
		std::string parseRequest();
		void sendResponse(jsonRpcResponse response);
		void close();
		ipcSession(int server_fd);
		~ipcSession();

	private:
	        struct sockaddr_storage sa;
	        socklen_t sa_len;
		int sockfd = -1;
	};

	class ipcServer {
	public:
		ipcSession acceptConnection();
		ipcServer(std::string path);
		~ipcServer();
		int get_sockfd() { return this->sockfd; }

	private:
		void create_socket();
		std::string socket_path = "";
		int sockfd = -1;
	};

	class ipcClient {
	public:
		void dispatch(jsonRpcRequest request, jsonRpcResponse& response);
		int get_sockfd() { return this->sockfd; }
		ipcClient(std::string path);
		~ipcClient();

	private:
		void create_socket();
		std::string socket_path = "";
		int sockfd = -1;
	};
}
