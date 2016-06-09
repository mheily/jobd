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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <streambuf>
#include <unordered_set>

extern "C" {
	#include <getopt.h>
}

#include "../libjob/job.h"
#include <libjob/namespaceImport.hpp>

using std::cout;
using std::endl;
using json = nlohmann::json;

static std::unique_ptr<libjob::jobdConfig> jobd_config(new libjob::jobdConfig);

// All commands that this utility accepts
const std::unordered_set<string> commands = {
	"list",
};

void usage() {
	std::cout <<
		"Usage:\n\n"
		"  jobadm <list>\n"
		"  -or-\n"
		"  job [-h|--help|-v|--version]\n"
		"\n"
		"  Miscellaneous options:\n\n"
		"    -h, --help         This screen\n"
		"    -v, --version      Display the version number\n"
	<< std::endl;
}

void show_version() {
	std::cout << "job version " + jobd_config->version << std::endl;
}

std::string format_job_status(json& status) {
	if (status["Enabled"].get<bool>() == false) {
		return "disabled";
	} else {
		if (status["State"] == "running") {
			return "\033[0;32mrunning\033[0m ";
		} else {
			return "\033[1;31moffline\033[0m ";
		}
	}
}

void list_response_handler(libjob::jsonRpcResponse& response)
{
	try {
		const char* format = "%s     %s\n";
		json o = response.getResult();
		printf(format, "\033[4mSTATUS\033[0m  ", "\033[4mLABEL\033[0m");
		for (json::iterator it = o.begin(); it != o.end(); ++it) {
			std::string status = format_job_status(it.value());
			printf(format, status.c_str(), it.key().c_str());
		}
	} catch(const std::exception& e) {
		std::cout << "ERROR: Unhandled exception: " << e.what() << '\n';
		throw;
	}
}

int
main(int argc, char *argv[])
{

	char ch;
	static struct option longopts[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "version", no_argument, NULL, 'v' },
			{ NULL, 0, NULL, 0 }
	};

	while ((ch = getopt_long(argc, argv, "hv", longopts, NULL)) != -1) {
		switch (ch) {
		case 'h':
			usage();
			return 0;
			break;

		case 'v':
			show_version();
			return 0;
			break;

		default:
			usage();
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	try {
		std::unique_ptr<libjob::ipcClient> ipc_client(new libjob::ipcClient(jobd_config->socketPath));
		libjob::jsonRpcResponse response;
		libjob::jsonRpcRequest request;

		request.setId(1); // Not used

		if (argc < 1)
			throw "insufficient arguments";

		std::string command = std::string(argv[0]);

		if (command == "list") {
			request.setMethod("list");
			ipc_client->dispatch(request, response);
			list_response_handler(response);
		}

		if (command == "load") {
			request.setMethod(command);
			char *resolved_path = realpath(argv[1], NULL);
			if (!resolved_path) {
				puts("ERROR: unable to resolve path");
				exit(EXIT_FAILURE);
			}
			request.addParam(std::string(resolved_path));
			ipc_client->dispatch(request, response);
			//FIXME: check response
			free(resolved_path);
		}

		if (command == "unload") {
			request.setMethod(command);
			request.addParam(std::string(argv[1]));
			ipc_client->dispatch(request, response);
			//FIXME: check response
		}
	} catch(const std::system_error& e) {
		std::cout << "Caught system_error with code " << e.code()
	                  << " meaning " << e.what() << '\n';
		exit(1);
	} catch(const std::exception& e) {
		std::cout << "ERROR: Unhandled exception: " << e.what() << '\n';
		exit(1);
	} catch(...) {
		std::cout << "Unhandled exception\n";
		exit(1);
	}

	return EXIT_SUCCESS;
}
