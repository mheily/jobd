/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
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

#ifndef MANAGER_H_
#define MANAGER_H_

#include <string>

#include "job.h"
#include "pidfile.h"

#include "../libjob/job.h"

class JobManager {
public:
	void setup(struct pidfh *pfh);
	void mainLoop();
	void enableJob(const string& label);
	void unloadJob(const string& label);
	void unloadAllJobs();
	void listAllJobs(json& result);

	JobManager() {}
	~JobManager();

	libjob::jobdConfig jobd_config;

private:
	/** kqueue(2) descriptor for the main event loop */
	int kqfd;

	struct pidfh *pidfile_handle;

	map<string,unique_ptr<Job>> jobs;

	/** The walltime when we should wake up and scan for KeepAlive=true jobs to restart */
	time_t next_keepalive_wakeup = 0;

	void scanJobDirectory();
	void reapChildProcess(pid_t pid, int status);
	void createProcessEventWatch(pid_t pid);
	void deleteProcessEventWatch(pid_t pid);
	unique_ptr<Job>& getJobByPid(pid_t pid);
	void removeJob(Job& job);
	void rescheduleJob(unique_ptr<Job>& job);
	void runPendingJobs();
	void updateKeepaliveWakeInterval();
	void handleKeepaliveWakeup();
	void wakeJob(const string& label);
	void setupSignalHandlers();
	void setupDataDirectory();
};

#endif /* MANAGER_H_ */
