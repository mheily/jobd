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

#include <sys/stat.h>
#include <unistd.h>

#include "logger.h"
#include "jobProperty.hpp"
#include "namespaceImport.hpp"

namespace libjob
{

std::string JobProperty::dataDir = "";

void JobProperty::readFile()
{
	if (access(this->path.c_str(), F_OK) < 0) {
		if (errno == ENOENT) {
			return;
		} else {
			log_errno("access(2)");
			throw std::system_error(errno, std::system_category());
		}
	}

	try {
		std::ifstream ifs(this->path, std::ifstream::in);
		ifs >> this->json;
	} catch (std::exception& e) {
		log_error("error parsing %s: %s", this->path.c_str(), e.what());
		throw;
	}
	log_debug("loaded properties from %s", this->path.c_str());
}

void JobProperty::sync()
{
	try {
		std::ofstream ofs(this->path, std::ofstream::out);
		ofs << this->json;
	} catch (std::exception& e) {
		log_error("error writing to %s: %s", path.c_str(), e.what());
		throw;
	}
}

void JobProperty::setDataDirectory(std::string& path)
{
	JobProperty::dataDir = path;
	(void) mkdir(path.c_str(), 0755);
}

void JobProperty::unloadHandler()
{
	(void) unlink(path.c_str()); // TODO: error checking
}

}
