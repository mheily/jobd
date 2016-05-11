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
#include <nlohmann/json.hpp>

namespace libjob
{

/** Job properties that persist across a reboot. For transient properties, see jobStatus */
class JobProperty
{
public:
	JobProperty() {
		/*
		 * faultstatus: can be none, degraded, offline, outdated
		 * faultdetail: detailed message describing the fault (optional)
		 * customproperties: user-defined properties in the manifest
		 */
		auto default_json = R"(
		          {
                          "JobPropertyAPI": 0,
                          "Label": "",
                          "Enabled": false,
                          "Faulted": false,
                          "FaultStatus": "none",
                          "FaultDetail": "",
                          "CustomProperties": {}
                          }
                )"_json;

		this->json = default_json;
	}

	~JobProperty() {}
	void readFile(const std::string& path);
	void sync();

	static void setDataDirectory(std::string& path);
	void setLabel(const std::string& label) {
		this->json["Label"] = label;
		this->path = JobProperty::dataDir + "/" + label + ".json";
	}


private:
	static std::string dataDir;
	std::string path;
	nlohmann::json json;
};
}
