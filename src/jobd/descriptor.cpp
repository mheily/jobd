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

extern "C" {
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
}

#include <libjob/logger.h>
#include "descriptor.h"

using json_t = nlohmann::json;

static int create_sys_kqueue(const json_t& j)
{
	return kqueue();
}

static int create_sys_socket(const json_t& j)
{
	static const std::map<string, int> socket_domains = {
			{ "PF_LOCAL", PF_LOCAL },
			{ "PF_UNIX", PF_UNIX },
			{ "PF_INET", PF_INET },
			{ "PF_INET6", PF_INET6 },
	};
	static const std::map<string, int> socket_types = {
			{ "SOCK_STREAM", SOCK_STREAM },
			{ "SOCK_DGRAM", SOCK_DGRAM },
	};

	if (j[3] != "0") {
		log_error("unsupported protocol");
		return -1;
	}

	auto kv = socket_domains.find(j[1]);
	if (kv == socket_domains.end()) {
		return -1;
	}
	int domain = kv->second;

	kv = socket_types.find(j[2]);
	if (kv == socket_types.end()) {
		return -1;
	}
	int sock_type = kv->second;

	return socket(domain, sock_type, 0);
}

int create_descriptor_for(const json_t& j)
{
	static const std::map<string, int (*)(const json_t&)> vtable = {
		{"kqueue", create_sys_kqueue},
		{"socket", create_sys_socket},
	};
	int fd;
	string syscall = j[0];

	auto kv = vtable.find(syscall);
	if (kv == vtable.end()) {
		log_error("unsupported system call: %s", syscall.c_str());
		return -1;
	}

	int (*func)(const json_t&) = kv->second;
	fd = (*func)(j);

	if (fd < 0) {
		log_errno("%s syscall", syscall.c_str());
		return -1;
	}

	return fd;
}
