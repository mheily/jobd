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

static std::string get_user_datadir() 
{
	const char *home = getenv("HOME");
	if (!home) {
		throw "Missing HOME directory environment variable";	
	}

#if TARGET_OS_MAC
	std::string dir = std::string(home) + "/Library/Jobd";
	return dir;
#else
	const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");

	/* TODO: Per the XDG Base Directory Specification,
	 * we should validate the ownership and permissions of this directory. */
	if (xdg_runtime_dir != NULL) {
		return xdg_runtime_dir;
	} else if (home != NULL) {
		std::string dir = std::string(home) + "/.jobd";
		(void) mkdir(dir.c_str(), 0700);
		dir += "/db";
		(void) mkdir(dir.c_str(), 0700);
		return dir;
	} else {
		throw "unable to locate data dir: HOME or XDG_RUNTIME_DIR must be set";
	}
#endif
}

static std::string get_user_runtimedir() 
{
	const char *home = getenv("HOME");
	if (!home) {
		throw "Missing HOME directory environment variable";	
	}

#if TARGET_OS_MAC
	std::string dir = std::string(home) + "/Library/Jobd/run";
//TODO: mkdir_idempotent
	return dir;
#else
	const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");

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
#endif
}

static std::string get_data_dir() {
	if (getuid() == 0) {
		(void) mkdir("/var/db/jobd", 0755);
		return "/var/db/jobd";
	} else {
		return get_user_datadir();
	}
}

static std::string get_runtime_dir() {
	if (getuid() == 0) {
		(void) mkdir("/var/run/jobd", 0755);
		return "/var/run/jobd";
	} else {
		return get_user_runtimedir();
	}
}

static std::string get_socketpath() {
	return get_runtime_dir() + "/jobd.sock";
}

void libjob::jobdConfig::createDirectories() {
	std::vector<std::string> paths;

	const char *home = getenv("HOME");
	if (getuid() > 0 && !home) {
		throw "Missing HOME directory environment variable";	
	}

#if TARGET_OS_MAC
	if (getuid() == 0) {
		home = "/";
	}

	paths.push_back(std::string(home) + "/Library/Jobd");
	paths.push_back(std::string(home) + "/Library/Jobd/run");
	paths.push_back(std::string(home) + "/Library/Jobd/manifest");

	for (auto& it : paths) {
		(void) mkdir(it.c_str(), 0700);
	}
#else
	if (getuid() > 0) {
		std::string path = std::string(home) + "/.config";
		(void) mkdir(path.c_str(), 0700);
	}
	//TODO: move other dir creation code here
#endif
}

libjob::jobdConfig::jobdConfig() {
	this->runtimeDir = get_runtime_dir();
	this->dataDir = get_data_dir();
	this->createDirectories();
	this->socketPath = get_socketpath();
	this->pidfilePath = get_runtime_dir() + "/jobd.pid";
}
