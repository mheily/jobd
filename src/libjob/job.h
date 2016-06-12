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

#ifndef LIBJOB_JOB_H_
#define LIBJOB_JOB_H_

#ifdef __cplusplus

#include <string>

#include "logger.h"
#include "ipc.h"

namespace libjob {

class jobdConfig {
public:

	jobdConfig();

	const std::string getManifestDir() const {
		return (dataDir + "/manifest");
	}

	const std::string getDataDir() const {
		return dataDir;
	}

	const std::string getPidfilePath() const {
		return pidfilePath;
	}

	const std::string getRuntimeDir() const {
		return runtimeDir;
	}

	const std::string getSocketPath() const {
		return socketPath;
	}

	const std::string getVersion() const {
		return version;
	}

	private:

		std::string version = "0.0.0";

		/** Directory where transient runtime files are stored */
		std::string runtimeDir;

		/** Directory where persistent data files are stored */
		std::string dataDir;

		/** The path to the IPC socket */
		std::string socketPath;

		/** The path to the pidfile */
		std::string pidfilePath;

		// Tell jobd to reload it's configuration
		void signal_jobd_reload();

		void set_jobdir();
		void set_socketpath();
		void createDirectories();
	};
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct libjob_s;

struct libjob_s * libjob_new(void);

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_H_ */
