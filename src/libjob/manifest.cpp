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
#include <unistd.h>

#include <grp.h>
#include <pwd.h>

#include "logger.h"
#include "manifest.hpp"
#include "parser.hpp"

namespace libjob
{

using json = nlohmann::json;

static string getFileExtension(const string& path)
{
	string::size_type offset = path.rfind('.');

	if (offset != string::npos) {
		//WORKAROUND: would prefer to use offset, rather than find_last_of
		string buf = path.substr(path.find_last_of(".") + 1);
		return buf;
	} else {
		return string("");
	}
}

void Manifest::parseJSON()
{
	try {
		std::ifstream ifs(path, std::ifstream::in);
		ifs >> this->json;
	} catch (std::exception& e) {
		log_error("error parsing %s: %s", path.c_str(), e.what());
		throw;
	} catch (...) {
		throw;
	}
}

void Manifest::parseUCL()
{
	struct ucl_parser* parser = NULL;
	ucl_object_t* obj = NULL;
	unsigned char *ucl_buf = NULL;

	try {
		parser = ucl_parser_new(0);
		if (!parser) {
			throw std::runtime_error("parser_new() failed");
		}

		std::ifstream ifs(path, std::ifstream::in);
		std::stringstream buf;
		buf << ifs.rdbuf();

		if (!ucl_parser_add_string(parser, buf.str().c_str(), buf.str().length() + 1)) {
			throw std::runtime_error("add_string() failed");
		}

		if (ucl_parser_get_error (parser)) {
			throw std::runtime_error("UCL parser error"); //TODO: add ucl_parser_get_error()
		}

	    obj = ucl_parser_get_object (parser);
	    ucl_buf = ucl_object_emit(obj, UCL_EMIT_JSON);
	    if (!ucl_buf) {
			throw std::runtime_error("ucl_object_emit() failed");
	    }
	    this->json = nlohmann::json::parse((char*)ucl_buf);

	    free(ucl_buf);
	    ucl_parser_free(parser);
	    ucl_object_unref(obj);
	} catch (std::exception& e) {
		log_error("parsing failed: %s", e.what());
		throw;
	} catch (...) {
		if (parser != NULL) {
		    ucl_parser_free(parser);
		}
		if (obj != NULL) {
		    ucl_object_unref(obj);
		}
		if (ucl_buf != NULL) {
			free(ucl_buf);
		}
		throw;
	}
}

void Manifest::readFile(const string path)
{
	this->path = path;
	string ext = getFileExtension(path);

	if (ext == "json") {
		parseJSON();
	} else if (ext == "ucl") {
		parseUCL();
	} else {
		throw std::invalid_argument("unsupported file extension");
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
