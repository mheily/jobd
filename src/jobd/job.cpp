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

extern "C" {
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/jail.h>
#endif
}

#include "capsicum.h"
#include "chroot.h"
#include "calendar.h"
#include "descriptor.h"
#include "dataset.h"
#include "job.h"
#include <libjob/logger.h>
#include <libjob/namespaceImport.hpp>
#include "manager.h"
#include "socket.h"
#include "timer.h"
#include "util.h"

int reset_signal_handlers();

void Job::apply_resource_limits() {
	//TODO - SoftResourceLimits, HardResourceLimits
	//TODO - LowPriorityIO

	int nice = this->manifest.json["Nice"];
	if (nice != 0) {
		log_debug("calling setpriority(2) to set nice value = %d", nice);
		if (setpriority(PRIO_PROCESS, 0, nice) < 0) {
			log_errno("setpriority(2)");
			throw std::system_error(errno, std::system_category());
		}
	}
}

void Job::modify_credentials() {
	if (getuid() != 0)
		return;

	const char* user_name = this->manifest.json["UserName"].get<string>().c_str();

	log_debug("setting credentials: username=%s uid=%d gid=%d",
			user_name, this->uid, this->gid);

	if (initgroups(user_name, this->gid) < 0) {
		log_errno("initgroups(3)");
		throw std::system_error(errno, std::system_category());
	}
	if (setgid(this->gid) < 0) {
		log_errno("setgid(2)");
		throw std::system_error(errno, std::system_category());
	}
#ifndef __GLIBC__
	if (setlogin(user_name) < 0) {
		log_errno("setlogin(2)");
		throw std::system_error(errno, std::system_category());
	}
#endif
	if (setuid(this->uid) < 0) {
		log_errno("setuid");
		throw std::system_error(errno, std::system_category());
	}
}


/* Add the standard set of environment variables that most programs expect.
 * See: http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap08.html
 * TODO: should cache these getenv() calls, so we don't do this dance for every
 * job invocation.
 */
static void
add_standard_environment_variables(vector<string>& env)
{
	static const char *keys[] = { 
		"DISPLAY",
		/* Locale-related variables */
		"LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MESSAGES", "LC_MONETARY",
		"LC_NUMERIC", "LC_TIME", "NLSPATH", "LANG",
		/* Misc */
		"TZ",
		NULL };
	const char **key = NULL, *envp = NULL;

	for (key = keys; *key != NULL; key++) {
		if ((envp = getenv(*key))) {
			string elem = string(*key) + '=' + envp;
			env.push_back(elem);
		}
	}
}

void Job::setup_environment()
{
	nlohmann::json env = this->manifest.json["EnvironmentVariables"];
	map<string,string> default_env;

	/* KLUDGE: when running as root, assume we are a system daemon and avoid adding any
	 * 	session-related variables.
	 * This is why we need a proper Domain variable for each job.
	 *
	 * The removal of these variables conforms to daemon(8) behavior on FreeBSD.
	 */
	if (getuid() == 0) {
		default_env = {
			{ "HOME", "/" },
			{ "PATH", "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin:/usr/local/sbin" },
			{ "PWD", "/" },
		};
	} else {
		default_env = {
			{ "HOME", this->home_directory },
			{ "PATH", "/usr/bin:/bin:/usr/local/bin" },
			{ "PWD", "/" },
			/* Not present in root environment */
			{ "LOGNAME", this->manifest.json["UserName"].get<string>() },
			{ "USER", this->manifest.json["UserName"].get<string>() },
			{ "SHELL", this->shell },
			{ "TMPDIR", "/tmp" },
		};
	}

	// FIXME: Does not actually override
	/* Follow the crontab(5) convention of overriding LOGNAME and USER
	 * and providing a default value for HOME, PATH, and SHELL */
	for (auto& iter : default_env) {
		const string& key = iter.first;
		const string& val = iter.second;

		if (env.find(key) == env.end()) {
			log_debug("setting default value for %s", key.c_str());
			this->environment.push_back(key + '=' + val);
		}
	}

	for (nlohmann::json::iterator it = env.begin(); it != env.end(); ++it) {
		string keyval = it.key() + '=' + it.value().get<string>();
		this->environment.push_back(keyval);
	}

	add_standard_environment_variables(this->environment);

	//FIXME: port the socket code
#if 0
	struct job_manifest_socket *jms;
	size_t offset = 0;

	SLIST_FOREACH(jms, &job->jm->sockets, entry) {
		job_manifest_socket_export(jms, env, offset++);
	}
	if (offset > 0) {
		if (asprintf(&buf, "LISTEN_FDS=%zu", offset) < 0) goto err_out;
		if (cvec_push(env, buf) < 0) goto err_out;
		free(buf);
		buf = NULL;

		if (asprintf(&buf, "LISTEN_PID=%d", getpid()) < 0) goto err_out;
		if (cvec_push(env, buf) < 0) goto err_out;
		free(buf);
		buf = NULL;
	}
#endif
}

void Job::exec()
{
	int rv;

	char* envp[this->environment.size() + 1];
	for (size_t i = 0; i < this->environment.size(); i++) {
		log_debug("setenv: %s", this->environment[i].c_str());
		envp[i] = (char*) this->environment[i].c_str();
	}
	envp[this->environment.size()] = nullptr;

	vector<string> json_argv = this->manifest.json["Program"].get<vector<string>>();

	char* argv[json_argv.size() + 1];
	for (size_t i = 0; i < json_argv.size(); i++) {
		argv[i] = (char*) json_argv[i].c_str();
	}
	argv[json_argv.size()] = nullptr;

	if (this->manifest.json["EnableGlobbing"].get<bool>()) {
		log_warning("Globbing is not implemented yet");
		//TODO: globbing
	}

	const char* path = json_argv[0].c_str();

#if 0
	log_debug("path: %s", path);
	log_debug("argv[]:");
	for (char **item = argv; *item; item++) {
		log_debug(" - arg: %s", *item);
	}
	log_debug("envp[]:");
	for (char **item = envp; *item; item++) {
		log_debug(" - env: %s", *item);
	}
#endif

	this->redirect_stdio();

	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		//FIXME: need to reopen stderr to something useful; by default it is /dev/null
		std::cerr << "ERROR: open(2) failed for " << path << '\n';
		exit(241);
	}

#if HAVE_CAPSICUM
	cap_rights_t rights;

	if (cap_rights_limit(fd, cap_rights_init(&rights, CAP_READ, CAP_FEXECVE)) < 0) {
		//FIXME: need to reopen stderr to something useful; by default it is /dev/null
		std::cerr << "Unable to limit capability rights";
		exit(242);
	}

	this->enterCapabilityMode();
	rv = fexecve(fd, argv, envp);
#else
	rv = execve(path, argv, envp);
#endif
	if (rv < 0) {
		//FIXME: need to reopen stderr to something useful; by default it is /dev/null
		std::cerr << "ERROR: execve(2) failed for " << path << '\n';
		exit(243);
    	}
}

void Job::redirect_stdio() {
	const char *path;
	int fd;

	string stdin_path = this->manifest.json["StandardInPath"];
	string stdout_path = this->manifest.json["StandardOutPath"];
	string stderr_path = this->manifest.json["StandardErrorPath"];

	path = stdin_path.c_str();
	log_debug("setting stdin path to %s", path);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		log_errno("open(2) of %s", path);
		throw std::system_error(errno, std::system_category());
	}
	if (dup2(fd, STDIN_FILENO) < 0) {
		log_errno("dup2(2) of stdin");
		int saved_errno = errno;
		(void) close(fd);
		throw std::system_error(saved_errno, std::system_category());
	}
	if (close(fd) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	path = stdout_path.c_str();
	log_debug("setting stdout path to %s", path);
	fd = open(path, O_CREAT | O_WRONLY, 0600);
	if (fd < 0) {
		log_errno("open(2) of %s", path);
		throw std::system_error(errno, std::system_category());
	}
	if (dup2(fd, STDOUT_FILENO) < 0) {
		log_errno("dup2(2) of stdout");
		int saved_errno = errno;
		(void) close(fd);
		throw std::system_error(saved_errno, std::system_category());
	}
	if (close(fd) < 0) {
		throw std::system_error(errno, std::system_category());
	}

	path = stderr_path.c_str();
	log_debug("setting stderr path to %s", path);
	fd = open(path, O_CREAT | O_WRONLY, 0600);
	if (fd < 0) {
		log_errno("open(2) of %s", path);
		throw std::system_error(errno, std::system_category());
	}
	if (dup2(fd, STDERR_FILENO) < 0) {
		log_errno("dup2(2) of stderr");
		int saved_errno = errno;
		(void) close(fd);
		throw std::system_error(saved_errno, std::system_category());
	}
	if (close(fd) < 0) {
		throw std::system_error(errno, std::system_category());
	}
}

void Job::createCapsicumLoaderDescriptors()
{
	int fd;
	string fdset;

	// TODO: refactor into loop

	fd = open("/lib", O_RDONLY);
	if (fd < 0)
		throw std::system_error(errno, std::system_category());
	fdset = std::to_string(fd);

	fd = open("/libexec", O_RDONLY);
	if (fd < 0)
		throw std::system_error(errno, std::system_category());
	fdset += ':' + std::to_string(fd);

	fd = open("/usr/lib", O_RDONLY);
	if (fd < 0)
		throw std::system_error(errno, std::system_category());
	fdset += ':' + std::to_string(fd);

	fd = open("/usr/local/lib", O_RDONLY);
	if (fd < 0)
		throw std::system_error(errno, std::system_category());
	fdset += ':' + std::to_string(fd);

	// TODO: run cap_rights_set() on descriptors

	this->environment.push_back("LD_LIBRARY_PATH_FDS=" + fdset);
	log_debug("Capsicum loader fdset: %s", fdset.c_str());
}

int
reset_signal_handlers()
{
	extern const int launchd_signals[];
	int i;

	/* TODO: convert everything to use sigaction instead of signal()
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
		if (sigaction(launchd_signals[i], &sa, NULL) < 0) {
			log_errno("sigaction(2)");
			return -1;
		}
	*/

	for (i = 0; launchd_signals[i] != 0; i++) {
		if (signal(launchd_signals[i], SIG_DFL) == SIG_ERR)
			err(1, "signal(2): %d", launchd_signals[i]);
	}

	return 0;
}

void Job::start_child_process()
{
#ifdef __FreeBSD__
	// TODO: reenable this
#if 0
	if (job->jm->jail_name) {
		log_debug("entering jail %s", job->jm->jail_name);
		/* XXX-FIXME: hardcoded to JID #1, should lookup the JID from name */
		if (jail_attach(1) < 0) {
			log_errno("jail_attach(2)");
			return -1;
		}
	}
#endif
#endif

	if (!this->manager->isNoFork() && setsid() < 0) {
		log_errno("setsid");
		throw std::system_error(errno, std::system_category());
	}

	if (reset_signal_handlers() < 0) {
		log_error("unable to reset signal handlers");
		throw std::system_error(errno, std::system_category());
	}

	this->apply_resource_limits();

	string cwd = this->manifest.json["WorkingDirectory"];
	log_debug("setting working directory to %s", cwd.c_str());
	if (chdir(cwd.c_str()) < 0) {
		log_error("chdir(2) to %s", cwd.c_str());
		throw std::system_error(errno, std::system_category());
	}

	/* TODO: deprecate the root_directory logic in favor of chroot_jail */
	const char *rootdir = this->manifest.json["RootDirectory"].get<string>().c_str();
	if (rootdir != nullptr && getuid() == 0) {
		if (chroot(rootdir) < 0) {
			log_error("unable to chroot to %s", rootdir);
			throw std::system_error(errno, std::system_category());
		}
	}

#if 0
	//TODO
	if (job->jm->chroot_jail && getuid() == 0) {
		if (chroot_jail_context_handler(job->jm->chroot_jail) < 0) {
			log_error("unable to chroot to %s", job->jm->root_directory);
			goto err_out;
		}
	}
#endif

	this->modify_credentials();

	//FIXME: convert string to to mode_t
	//(void) umask(job->jm->umask);

	this->setup_environment();
	this->createDescriptors();

	if (this->useCapsicum()) {
		this->createCapsicumLoaderDescriptors();
		capsicum_resources_acquire(this->manifest.json, this->descriptors);
	}

	this->exec();
}

job_t job_new(job_manifest_t jm)
{
	job_t j;

	j = (job_t) calloc(1, sizeof(*j));
	if (!j) return NULL;
	j->jm = jm;
	j->state = JOB_STATE_DEFINED;
	if (jm->start_interval > 0) {
		j->schedule = JOB_SCHEDULE_PERIODIC;
	} else if (jm->start_calendar_interval) {
		j->schedule = JOB_SCHEDULE_CALENDAR;
	} else {
		j->schedule = JOB_SCHEDULE_NONE;
	}
	return (j);
}

void job_free(job_t job)
{
	if (job == NULL) return;
	//XXX-FIXME-LEAK: manifest never freed
	free(job->jm);
	free(job);
}

void Job::load() {
	//TODO: sockets
	//TODO: schedule and timer
	this->setState(JOB_STATE_LOADED);
	log_debug("loaded %s", this->getLabel().c_str());
}

#if 0
// FIXME: port to new job_load
int job_load(job_t job)
{
	struct job_manifest_socket *jms;

	/* TODO: This is the place to setup on-demand watches for the following keys:
			WatchPaths
			QueueDirectories
	*/
	if (!SLIST_EMPTY(&job->jm->sockets)) {
		SLIST_FOREACH(jms, &job->jm->sockets, entry) {
			if (job_manifest_socket_open(job, jms) < 0) {
				log_error("failed to open socket");
				return (-1);
			}
		}
		log_debug("job %s sockets created", job->jm->label);
		job->state = JOB_STATE_WAITING;
		return (0);
	}

	if (job->schedule == JOB_SCHEDULE_PERIODIC) {
		if (timer_register_job(job) < 0) {
			log_error("failed to register the timer for job");
			return -1;
		}
	} else if (job->schedule == JOB_SCHEDULE_CALENDAR) {
		if (calendar_register_job(job) < 0) {
			log_error("failed to register the calendar job");
			return -1;
		}
	}

	job->state = JOB_STATE_LOADED;
	log_debug("loaded %s", job->jm->label);
	job_dump(job);
	return (0);
}
#endif

void Job::unload()
{
	const pid_t pid = this->jobStatus.getPid();

	if (this->state == JOB_STATE_RUNNING) {
		log_debug("sending SIGTERM to process group %d", pid);
		if (kill(-1 * pid, SIGTERM) < 0) {
			log_errno("killpg(2) of pid %d", pid);
			/* not sure how to handle the error, we still want to clean up */
		}
		this->setState(JOB_STATE_KILLED);
		//TODO: start a timer to send a SIGKILL if it doesn't die gracefully
	} else {
		//TODO: update the timer interval in timer.c?
		this->setState(JOB_STATE_DEFINED);
	}

#if 0
	keepalive_remove_job(job);
	if (job->jm->datasets)
		dataset_list_unload_handler(job->jm->datasets);
	if (job->jm->chroot_jail)
		chroot_jail_unload_handler(job->jm->chroot_jail);
#endif
}

void Job::acquire_resources() {
	log_debug("TODO");
#if 0
	static int job_acquire_resources(job_t job)
	{
		if (job->jm->datasets && dataset_list_load_handler(job->jm->datasets) < 0) {
			log_error("unable to create datasets");
			return -1;
		}
		if (job->jm->chroot_jail && chroot_jail_load_handler(job->jm->chroot_jail) < 0) {
			log_error("unable to create chroot(2) jail");
			return -1;
		}
		return 0;
	}
#endif
}

void Job::lookup_credentials() {
	struct passwd *pwent;
	struct group *grent;

	string user = this->manifest.json["UserName"];
	if ((pwent = getpwnam(user.c_str())) == NULL) {
		log_errno("getpwnam");
		throw std::system_error(errno, std::system_category());
	}
	this->uid = pwent->pw_uid;
	this->home_directory = std::string(pwent->pw_dir);
	this->shell = std::string(pwent->pw_shell);

	string group = this->manifest.json["GroupName"];
	if ((grent = getgrnam(group.c_str())) == NULL) {
		log_errno("getgrnam");
		throw std::system_error(errno, std::system_category());
	}
	this->gid = grent->gr_gid;
}

void Job::run() {
	pid_t pid;
	this->acquire_resources();
	this->lookup_credentials();

	// This is useful for debugging errors that prevent exec()
	if (this->manager->isNoFork()) {
		pid = 0;
	} else {
		pid = fork();
	}

	if (pid < 0) {
		log_errno("fork(2)");
		throw std::system_error(errno, std::system_category());
	} else if (pid == 0) {
		try {
			this->manager->forkHandler();
			this->start_child_process();
		} catch(const std::system_error& e) {
			//FIXME: log_error("child caught exception: errno=%d (%s)", e.code(), e.what());
			log_error("child caught system_error: errno=%s", e.what());
			exit(124);
		} catch (const std::exception& e) {
			log_error("child caught exception: %s", e.what());
			exit(124);
		}
	} else {
		this->jobStatus.setPid(pid);
		log_debug("job %s started with pid %d", this->label.c_str(), pid);
		this->setState(JOB_STATE_RUNNING);
		this->restart_after = 0;
		manager->createProcessEventWatch(pid);
		// FIXME: close descriptors that the master process no longer needs
#if 0
		SLIST_FOREACH(jms, &job->jm->sockets, entry) {
			job_manifest_socket_close(jms);
		}
#endif
	}
}

void Job::clearFault()
{
	if (this->isFaulted()) {
		log_info("cleared faulted job: %s", this->getLabel().c_str());
		this->jobProperty.setFaulted(libjob::JobProperty::JOB_FAULT_STATE_NONE, "");
		this->setState(JOB_STATE_LOADED);
		if (this->isRunnable()) {
			this->run();
		}
	} else {
		log_debug("tried to clear a job that was not in a faulted state");
	}
}

void Job::createDescriptors()
{
	if (manifest.json.find("CreateDescriptors") != manifest.json.end()) {
		log_debug("creating descriptors");
		nlohmann::json o = manifest.json["CreateDescriptors"];
		for (nlohmann::json::iterator it = o.begin(); it != o.end(); ++it) {
			int fd = create_descriptor_for(it.value());
			descriptors[it.key()] = fd;

			// Push this as an environment variable
			string key = "JOB_DESCRIPTOR_" + it.key();
			string kv = key + '=' + std::to_string(fd);
			this->environment.push_back(kv);
			log_debug("setting %s", kv.c_str());
		}
	}
}

void Job::enterCapabilityMode()
{
#if HAVE_CAPSICUM
	if (manifest.json.find("CapsicumRights") != manifest.json.end()) {
		cap_enter();
		log_debug("entered capability mode");
	}
#endif
}

bool Job::useCapsicum()
{
	return (manifest.json.find("CapsicumRights") != manifest.json.end());
}
