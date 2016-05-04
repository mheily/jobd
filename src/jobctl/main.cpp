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
#include <iostream>

extern "C" {
	#include <getopt.h>
}

#include "../libjob/job.h"

static std::unique_ptr<libjob::jobdConfig> jobd_config(new libjob::jobdConfig);

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

		for (int i = 0; i < argc; i++) {
			std::string arg = std::string(argv[i]);
			if (arg == "load") {
				ipc_client->request("load " + std::string(argv[i+1]));
				i++;
			} else {
				puts(arg.c_str());
			}
		}


	} catch(const std::system_error& e) {
		std::cout << "Caught system_error with code " << e.code()
	                  << " meaning " << e.what() << '\n';
		exit(1);
	} catch(...) {
		std::cout << "Unhandled exception\n";
		exit(1);
	}

	return EXIT_SUCCESS;
}
