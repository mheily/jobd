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
#include <system_error>

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

static void mkdir_p(const std::string& path, mode_t mode)
{
	if (access(path.c_str(), X_OK) < 0) {
		if (errno == ENOENT) {
			if (mkdir(path.c_str(), mode) < 0) {
					throw std::system_error(errno, std::system_category());
			}
		} else {
			throw std::system_error(errno, std::system_category());
		}
	}
}
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
	const char *data_home, *runtime_dir;

	if (getuid() == 0) {
		data_home = "/var/db";
		runtime_dir = "/var/run";
	} else {
		data_home = getenv("XDG_DATA_HOME");
		runtime_dir = getenv("XDG_RUNTIME_DIR");
	}

	char *home = getenv("HOME");
	char *logname = getenv("LOGNAME");

	if (data_home) {
			xdg_data_home = std::string(data_home);
	} else {
		if (!home) {
			throw std::runtime_error("HOME is not set");
		}
		if (access(home, W_OK) != 0) {
			throw std::runtime_error("HOME does not exist or is not writable");
		}
		std::string home_s = std::string(home);
		xdg_data_home = home_s + "/.local/share";
		mkdir_p(home_s + "/.local", 0700);
		mkdir_p(home_s + "/.local/share", 0700);
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
	dataDir = xdg_data_home + "/jobd";
	createDirectories();
	socketPath = getRuntimeDir() + "/jobd.sock";
	pidfilePath = getRuntimeDir() + "/jobd.pid";
}
