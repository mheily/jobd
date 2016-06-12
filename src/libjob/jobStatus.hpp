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

#pragma once

#include <unistd.h>
#include <string>

#include "parser.hpp"

namespace libjob
{

class JobStatus
{
public:
	JobStatus() {
		auto default_json = R"(
		          {
                          "JobStatusAPI": 0,
                          "Pid": 0,
                          "LastExitStatus": 0,
                          "TermSignal": 0
                          }
                )"_json;

		this->json = default_json;
	}

	~JobStatus() {}

	pid_t getPid() const { return this->json["Pid"].get<unsigned int>(); }
	void setPid(pid_t pid) { this->json["Pid"] = pid; }
	static void setRuntimeDirectory(std::string& path);
	void setLabel(const std::string& label) {
		this->json["Label"] = label;
		this->path = JobStatus::runtimeDir + "/" + label + ".json";
		//FIXME: definitely not accurate yet: this->readFile();
		//TODO: verify the status is still accurate, the process might not exist anymore
	}

	int getLastExitStatus() const
	{
		return this->json["LastExitStatus"].get<unsigned int>();
	}

	void setLastExitStatus(int lastExitStatus)
	{
		this->json["LastExitStatus"] = lastExitStatus;
		this->sync();
	}

	int getTermSignal() const
	{
		return this->json["LastExitStatus"].get<unsigned int>();
	}

	void setTermSignal(int termSignal)
	{
		this->json["TermSignal"] = termSignal;
		this->sync();
	}

	void unloadHandler();

private:
	static std::string runtimeDir;
	std::string path;
	nlohmann::json json;
	void readFile();
	void sync();
};
}
