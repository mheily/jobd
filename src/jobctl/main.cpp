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

extern "C" {
	#include <getopt.h>
}

#include "../libjob/job.h"
#include <libjob/namespaceImport.hpp>

using std::cout;
using std::endl;

static std::unique_ptr<libjob::jobdConfig> jobd_config(new libjob::jobdConfig);

static void read_manifest(const std::string path) {
	try {
		std::cout << path << '\n';
		std::ifstream ifs(path);
		std::stringstream buf;
		buf << ifs.rdbuf();
	} catch (const std::invalid_argument& ia) {
	}
}

void usage() {
	std::cout <<
		"Usage:\n\n"
		"  jobctl <label> [enable|disable|clear|status]\n"
		"  -or-\n"
		"  jobctl list\n"
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

void list_response_handler(libjob::jsonRpcResponse& response)
{
	try {
		const char* format = "%-24s %s\n";
		json o = response.getResult();
		printf(format, "Label", "Status");
		printf("%s\n", string(72, '-').c_str());
		for (json::iterator it = o.begin(); it != o.end(); ++it) {
			printf(format, it.key().c_str(), it.value().dump().c_str());
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

		std::string command_or_label = std::string(argv[0]);

		if (command_or_label == "list") {
			request.setMethod("list");
			ipc_client->dispatch(request, response);
			list_response_handler(response);
		} else {
			for (int i = 0; i < argc; i++) {
				std::string label = std::string(argv[i]);
				i++;
				std::string command = std::string(argv[i]);
				i++;

				request.setMethod(command);
				if (command == "load") {
					//char *resolved_path = realpath(label.c_str(), NULL);
					//std::string path(resolved_path);
					//free(resolved_path);
					//request.addParam(path);
					read_manifest(label);
					throw "XXX-TESTING";
				} else if (command == "unload") {
					request.addParam(label);
				} else {
					puts(command.c_str());
					throw "unexpected argument";
				}
				ipc_client->dispatch(request, response);
				//TODO: handle response
				break;
			}
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
