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
#include "job.h"

namespace libjob {

	using json = nlohmann::json;

	class ipcSession {
	public:
		void readRequest();
		void sendResponse(jsonRpcResponse response);
		void close();
		jsonRpcRequest getRequest() { return this->request; }
		jsonRpcResponse getResponse() { return this->response; }
		ipcSession(int server_fd, struct sockaddr_un sa);
		~ipcSession();

	private:
		char buf[9999]; // FIXME: hardcoded
		size_t bufsz = 0;
		jsonRpcRequest request;
		jsonRpcResponse response;
	        struct sockaddr_un server_sa, client_sa;
	        socklen_t sa_len;
		int sockfd = -1;
	};

	class ipcServer {
	public:
		ipcSession acceptConnection();
		ipcServer(std::string path);
		~ipcServer();
		int get_sockfd() { return this->sockfd; }

		/** Gracefully terminate the server after fork(2) is called */
		void fork_handler();

	private:
		//libjob::jobdConfig* jobd_config;
		void create_socket();
		std::string socket_path = "";
	        struct sockaddr_un sa;
		int sockfd = -1;
	};

	class ipcClient {
	public:
		void dispatch(jsonRpcRequest request, jsonRpcResponse& response);
		int get_sockfd() { return this->sockfd; }
		ipcClient();
		~ipcClient();

	private:
		libjob::jobdConfig jobd_config;
		void create_socket();
		int sockfd = -1;
		void bootstrapJobDaemon();
	};
}
