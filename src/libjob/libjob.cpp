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

	#ifdef __APPLE__
	#include "TargetConditionals.h"
	#if !TARGET_OS_MAC
	#error Unsupported OS
	#endif
	#endif
}

#include "job.h"
#include "jobStatus.hpp"

void libjob::jobdConfig::createDirectories() {
	std::vector<std::string> paths;

	paths.push_back(getDataDir());
	paths.push_back(getRuntimeDir());
	paths.push_back(getManifestDir());

	for (auto& it : paths) {
		(void) mkdir(it.c_str(), 0700);
	}
}

void libjob::jobdConfig::get_xdg_base_directory()
{
	const char *config_home, *runtime_dir;

	if (getuid() == 0) {
		config_home = "/usr/local/etc";
		runtime_dir = "/var/run";
	} else {
		config_home = getenv("XDG_CONFIG_HOME");
		runtime_dir = getenv("XDG_RUNTIME_DIR");
	}

	char *home = getenv("HOME");
	char *logname = getenv("LOGNAME");

	if (config_home) {
			xdg_config_home = std::string(config_home);
	} else {
		if (!home) {
			throw std::runtime_error("HOME is not set");
		}
		if (access(home, W_OK) != 0) {
			throw std::runtime_error("HOME does not exist or is not writable");
		}
		xdg_config_home = std::string(home) + "/.config";
		if (access(xdg_config_home.c_str(), W_OK) < 0) {
			if (errno == ENOENT) {
				if (mkdir(xdg_config_home.c_str(), 0700) < 0) {
					throw std::system_error(errno, std::system_category());
				}
			} else {
				throw std::system_error(errno, std::system_category());
			}
		}
	}

	if (runtime_dir) {
		xdg_runtime_dir = std::string(runtime_dir);
	} else {
		if (!logname)
			throw std::runtime_error("LOGNAME is not set");
		xdg_runtime_dir = "/tmp/jobd-" + std::string(logname);
		if (mkdir(xdg_runtime_dir.c_str(), 0700) < 0) {
			if (errno == EEXIST) {
				struct stat sb;

				if (stat(xdg_runtime_dir.c_str(), &sb) < 0) {
					throw std::system_error(errno, std::system_category());
				}
				if (sb.st_uid != getuid()) {
					throw std::runtime_error("Bad ownership of xdg_runtime_dir");
				}
			} else {
				throw std::system_error(errno, std::system_category());
			}
		}
	}

/*
	printf("xdg_config_home=%s, xdg_runtime_dir=%s\n",
			xdg_config_home.c_str(),
			xdg_runtime_dir.c_str());
	abort();
	*/
}

libjob::jobdConfig::jobdConfig() {
	get_xdg_base_directory();
	runtimeDir = xdg_runtime_dir + "/jobd";
	dataDir = xdg_config_home + "/jobd";
	createDirectories();
	socketPath = getRuntimeDir() + "/jobd.sock";
	pidfilePath = getRuntimeDir() + "/jobd.pid";
}
