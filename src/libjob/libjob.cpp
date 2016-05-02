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
#include <fstream>
#include <string>
#include <cstdlib>

extern "C" {
	#include <unistd.h>
}

#include "job.h"

LibJob::LibJob() {
	set_jobdir();
}

void LibJob::set_jobdir() {
	if (getuid() == 0) {
		this->jobdir = "/usr/local/etc/job.d"; //FIXME: harcoded prefix
	} else {
		const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
		const char *home = getenv("HOME");
		if (xdg_config_home == NULL && home == NULL)
			throw "unable to locate configuration: HOME or XDG_CONFIG_HOME be set";

		if (xdg_config_home) {
			this->jobdir = std::string(xdg_config_home) + "/job.d";
		} else {
			this->jobdir = std::string(home) + "/.config/job.d";
		}
	}
	//log_info("jobdir=" + this->jobdir);
};

void LibJob::load_manifest(std::string path) {
	std::cout << "loading: " + path + " into " + this->jobdir << '\n';

	// TODO: check if src exists
	// Copy the manifest into the configuration directory, using a special extension
	std::ifstream src(path, std::ios::binary);
	std::ofstream dst(this->jobdir + "/test.load", std::ios::binary);
	dst << src.rdbuf();
}

struct libjob_s * libjob_new(void) {
        LibJob *lj = new LibJob();
        return reinterpret_cast<struct libjob_s *>(lj);
}
