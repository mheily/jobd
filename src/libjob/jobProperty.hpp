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

	typedef enum e_fault_state {
		JOB_FAULT_STATE_NONE,	// The job is not in a faulted state
		JOB_FAULT_STATE_DEGRADED,	// The job is running, but not healthy
		JOB_FAULT_STATE_OFFLINE,	// The job failed and the process is dead
	} job_fault_state_t;

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
                          "FaultState": 0,
                          "FaultMessage": "",
                          "CustomProperties": {}
                          }
                )"_json;

		this->json = default_json;
	}

	~JobProperty() {}

	static void setDataDirectory(std::string& path);

	void setLabel(const std::string& label) {
		this->json["Label"] = label;
		this->path = JobProperty::dataDir + "/" + label + ".json";
		this->readFile();
	}

	bool isEnabled() const
	{
		return this->json["Enabled"];
	}

	void setEnabled(bool enabled = false)
	{
		this->json["Enabled"] = enabled;
		this->sync();
	}

	bool isFaulted() const
	{
		return (this->json["FaultState"].get<int>() != 0);
	}

	void setFaulted(e_fault_state state, std::string message = "")
	{
		this->json["FaultState"] = state;
		this->json["FaultMessage"] = message;
		this->sync();
	}

	const std::string getFaultStateString() const
	{
		static const std::string fault_state_as_string[3] = {
				"online",
				"degraded",
				"offline",
		};
		return fault_state_as_string[this->json["FaultState"].get<int>()];
	}

private:
	static std::string dataDir;
	std::string path;
	nlohmann::json json;

	void sync();
	void readFile();
};
}
