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
	#include <sys/stat.h>
	#include <unistd.h>
}

#include "job.h"

static std::string get_runtime_dir() {
	if (getuid() == 0) {
		return "/var/run";
	} else {
		const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
		const char *home = getenv("HOME");

		/* TODO: Per the XDG Base Directory Specification,
		 * we should validate the ownership and permissions of this directory. */
		if (xdg_runtime_dir != NULL) {
			return xdg_runtime_dir;
		} else if (home != NULL) {
			std::string dir = std::string(home) + "/.jobd";
			(void) mkdir(dir.c_str(), 0700);
			dir += "/run";
			(void) mkdir(dir.c_str(), 0700);
			return dir;
		} else {
			throw "unable to locate runtime dir: HOME or XDG_RUNTIME_DIR must be set";
		}
	}
}

static std::string get_socketpath() {
	if (getuid() == 0) {
		return "/var/run/jobd.sock";
	} else {
		return get_runtime_dir() + "/jobd.sock";
	}
}

static std::string get_jobdir() {
	if (getuid() == 0) {
		return "/usr/local/etc/job.d"; //FIXME: harcoded prefix
	} else {
		const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
		const char *home = getenv("HOME");
		if (xdg_config_home == NULL && home == NULL)
			throw "unable to locate configuration: HOME or XDG_CONFIG_HOME must be set";

		if (xdg_config_home) {
			return std::string(xdg_config_home) + "/job.d";
		} else {
			return std::string(home) + "/.config/job.d";
		}
	}
	//log_info("jobdir=" + this->jobdir);
};


libjob::jobdConfig::jobdConfig() {
	this->runtimeDir = get_runtime_dir();
	this->jobdir = get_jobdir();
	this->socketPath = get_socketpath();
}



#if 0
void LibJob::load_manifest(std::string path) {
	std::cout << "loading: " + path + " into " + this->jobdir << '\n';

	// TODO: check if src exists
	// Copy the manifest into the configuration directory, using a special extension
	std::ifstream src(path, std::ios::binary);
	std::ofstream dst(this->jobdir + "/test.load", std::ios::binary);
	dst << src.rdbuf();
}
#endif

#if 0
struct libjob_s * libjob_new(void) {
        LibJob *lj = new LibJob();
        return reinterpret_cast<struct libjob_s *>(lj);
}
#endif
