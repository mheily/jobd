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

#pragma once

#include <string>
#include <nlohmann/json.hpp>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include "../../vendor/FreeBSD/sys/queue.h"
#include <unistd.h>

#include "manifest.h"
#include <libjob/jobProperty.hpp>
#include <libjob/jobStatus.hpp>
#include "../libjob/namespaceImport.hpp"
#include "../libjob/manifest.hpp"
#include "../libjob/logger.h"

class JobManager;

typedef enum {
	JOB_SCHEDULE_NONE = 0,
	JOB_SCHEDULE_PERIODIC,
	JOB_SCHEDULE_CALENDAR,
	JOB_SCHEDULE_KEEPALIVE
} job_schedule_t;

typedef enum e_job_state {
	/** The job is invalid in some way; e.g. a syntax error in the manifest */
	JOB_STATE_INVALID,

	/** The job has been parsed, but nothing more. */
	JOB_STATE_DEFINED,

	/** The load() method has been invoked. */
	JOB_STATE_LOADED,

	JOB_STATE_WAITING,
	JOB_STATE_RUNNING,
	JOB_STATE_KILLED,
	JOB_STATE_EXITED,
} job_state_t;


struct job {
	LIST_ENTRY(job)	joblist_entry;
	SLIST_ENTRY(job) start_interval_sle;
	SLIST_ENTRY(job) watchdog_sle;

	//DEADWOOD
	job_manifest_t jm;

	/** Full path to the JSON file containing the manifest */
	std::string jobdir_path;

	/** A parsed JSON manifest */
	libjob::Manifest manifest;

	job_state_t state;
	pid_t pid;

	time_t  next_scheduled_start;
	job_schedule_t schedule;
};
typedef struct job *job_t;

class Job {
	friend class JobManager;

public:
	Job() {
		this->setState(JOB_STATE_INVALID);
	}

	Job(const string label)
	{
		this->setLabel(label);
	}

	~Job() {}

	bool operator<(const Job& j) const
	{
		return j.label < this->label;
	}

	string getLabel() const
	{
		return label;
	}

	void setLabel(string label)
	{
		this->label = label;
	}

	void parseManifest(const string path)
	{
		try {
			this->manifest.readFile(path);
			this->setLabel(this->manifest.getLabel());
			this->setState(JOB_STATE_DEFINED);
		} catch (...) {
			log_error("readFile() failed");
			this->setState(JOB_STATE_INVALID);
		}
	}

	enum e_job_state getState() const
	{
		return state;
	}

	const std::string getStateString() const
	{
		switch (state) {
		case JOB_STATE_INVALID: return "invalid";
		case JOB_STATE_DEFINED: return "defined";
		case JOB_STATE_LOADED: return "loaded";
		case JOB_STATE_WAITING: return "waiting";
		case JOB_STATE_RUNNING: return "running";
		case JOB_STATE_KILLED: return "killed";
		case JOB_STATE_EXITED: return "exited";
		}
		return "corrupt"; // as in, memory corruption
	}

	const std::string getFaultStateString() const
	{
		return this->jobProperty.getFaultStateString();
	}

	void setState(enum e_job_state state)
	{
		this->state = state;
	}

	bool isRunnable() const
	{
		if (this->isEnabled() && !this->isFaulted() && this->state == JOB_STATE_LOADED) {
			return true;
		} else {
			log_debug("not runnable: enabled=%d faulted=%d",
					this->isEnabled(), this->isFaulted());
			return false;
		}
	}

	pid_t getPid() const { return this->jobStatus.getPid(); }

	bool isEnabled() const
	{
		return this->jobProperty.isEnabled();
	}

	bool isFaulted() const
	{
		return this->jobProperty.isFaulted();
	}

	void setEnabled(bool enabled)
	{
		this->jobProperty.setEnabled(enabled);
		if (enabled && this->isRunnable()) {
			this->run();
		} else if (!enabled && this->getState() == JOB_STATE_RUNNING) {
			this->unload();
		}
	}

	void setManager(JobManager* manager)
	{
		this->manager = manager;
	}

	void clearFault();
	void load();
	void run();
	void unload();

private:
	JobManager* manager;
	struct job jm; // XXX-FIXME for build testing
	char	*program;
///^^^kill the above

	string label = "__invalid_label__";
	libjob::Manifest manifest;
	libjob::JobStatus jobStatus;
	libjob::JobProperty jobProperty;
	enum e_job_state state;


	/* Credentials and /etc/passwd info */
	uid_t uid;
	gid_t gid;
	std::string home_directory;
	std::string shell;

	/** KeepAlive=true ? After this walltime, the job should be restarted */
	time_t restart_after = 0;

	/** Environment variables, in the form of KEY=value */
	vector<string> environment;

	void capsicumize();
	void acquire_resources();
	void apply_resource_limits();
	void lookup_credentials();
	void modify_credentials();
	void start_child_process();
	void redirect_stdio();
	void setup_environment();
	void exec();
};

extern const int launchd_signals[];


job_t	job_new(job_manifest_t jm);
void	job_free(job_t job);
int	job_load(job_t job);
int	job_unload(job_t job);
