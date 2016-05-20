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

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>

#include <grp.h>
#include <pwd.h>

#include "logger.h"
#include "manifest.hpp"

namespace libjob
{

void Manifest::readFile(const string path)
{
	this->path = path;
	try {
		std::ifstream ifs(path, std::ifstream::in);
		ifs >> this->json;
	} catch (std::exception& e) {
		log_error("error parsing %s: %s", path.c_str(), e.what());
		throw;
	}
	try {
		this->normalize();
	} catch (std::exception& e) {
		log_error("normalization failed: %s", e.what());
		throw;
	}
	this->label = this->json["Label"];
}

void Manifest::normalize() {
	auto default_json = R"(
	  {
            "ChrootDirectory": null,
            "Description": "",
            "EnableGlobbing": false,
            "EnvironmentVariables": [],
	    "KeepAlive": false,
	    "Nice": 0,            
	    "InitGroups": true,
	    "RootDirectory": "/",
	    "Enable": false,
	    "Sockets": {},
            "StandardErrorPath": "/dev/null",
            "StandardInPath": "/dev/null",
            "StandardOutPath": "/dev/null",
            "StartInterval": 0,
            "ThrottleInterval": 10,
            "Umask": "022",
            "WorkingDirectory": "/"
	  }
	)"_json;

	if (this->json.count("Program") == 1) {
		//FIXME: convert from string to array,
		// this->json["Program"].type()
		//abort();
	}
	if (this->json.count("UserName") == 0) {
		struct passwd *pwd = getpwuid(getuid());
		this->json["UserName"] = string(pwd->pw_name);
	}

	if (this->json.count("GroupName") == 0) {
		struct group *grp = getgrgid(getgid());
		this->json["GroupName"] = string(grp->gr_name);
	}


	// Add default values for missing keys
	for (json::iterator it = default_json.begin(); it != default_json.end(); ++it) {
		if (this->json.count(it.key()) == 0) {
			this->json[it.key()] = it.value();
		}
	}
}

}
