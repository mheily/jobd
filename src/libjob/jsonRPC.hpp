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


namespace libjob {

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

	using json = nlohmann::json;

	class jsonRpcRequest {
	public:
		jsonRpcRequest(std::string buf) {
			this->request = json::parse(buf);
		}

		jsonRpcRequest(unsigned int id, std::string method) {
			request["jsonrpc"] = "2.0",
			request["id"] = id;
			request["method"] = method;
			request["params"] = json::array();
		}

		void addParam(std::string value) {
			request["params"].push_back(value);
		}

		std::string getParam(unsigned int where) { return this->request["params"][where]; }

		unsigned int id() { return this->request["id"]; }
		std::string method() { return this->request["method"]; }

		std::string dump() { return this->request.dump(); }

	private:
		json request;
	};

	class jsonRpcResponse {
	public:
		jsonRpcResponse() {
			this->response["jsonrpc"] = "2.0";
		}

		jsonRpcResponse(unsigned int id) {
			this->response["jsonrpc"] = "2.0",
			this->response["id"] = id;
		}

		void setResult(std::string str) { this->response["result"] = str; }
		std::string getResult() { return this->response["result"]; }
		std::string dump() { return this->response.dump(); }

	private:
		json response;
	};
}
