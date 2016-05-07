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

#include "chroot.h"
#include "calendar.h"
#include "dataset.h"
#include "job.h"
#include <libjob/logger.h>
#include "manager.h"
#include "socket.h"
#include "timer.h"
#include "util.h"


static int start_child_process(const Job& job);
static int job_acquire_resources(job_t job);
extern void keepalive_remove_job(struct job *job);

int reset_signal_handlers();

static void job_dump(job_t job) {
	log_debug("job dump: label=%s state=%d", job->jm->label, job->state);
}

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
static int
add_standard_environment_variables(cvec_t env)
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
	char *buf = NULL;

	for (key = keys; *key != NULL; key++) {
		if ((envp = getenv(*key))) {
			if (asprintf(&buf, "%s=%s", *key, envp) < 0) goto err_out;
			if (cvec_push(env, buf) < 0) goto err_out;
		}
	}

	return 0;

err_out:
	free(buf);
	return -1;
}

static inline cvec_t setup_environment_variables(const job_t job, const struct passwd *pwent)
{
	struct job_manifest_socket *jms;
	cvec_t env = NULL;
	char *curp, *buf = NULL;
	char *logname_var = NULL, *user_var = NULL;
	unsigned int i, uid;
	bool found[] = { false, false, false, false, false, false, false };
	size_t offset = 0;

	env = cvec_new();
	if (!env) goto err_out;

	if (asprintf(&logname_var, "LOGNAME=%s", job->jm->user_name) < 0) goto err_out;
	if (asprintf(&user_var, "USER=%s", job->jm->user_name) < 0) goto err_out;

	if (!job->jm->environment_variables)
		goto job_has_no_environment;

	/* Convert the flat array into an array of key=value pairs */
	/* Follow the crontab(5) convention of overriding LOGNAME and USER
	 * and providing a default value for HOME, PATH, and SHELL */
	log_debug("job %s has %zu env vars\n", job->jm->label, cvec_length(job->jm->environment_variables));
	for (i = 0; i < cvec_length(job->jm->environment_variables); i += 2) {
		curp = cvec_get(job->jm->environment_variables, i);
		log_debug("evaluating %s", curp);
		if (strcmp(curp, "LOGNAME") == 0) {
			found[0] = true;
			if (cvec_push(env, logname_var) < 0) goto err_out;
		} else if (strcmp(curp, "USER") == 0) {
			found[1] = true;
			if (cvec_push(env, user_var) < 0) goto err_out;
		} else if (strcmp(curp, "HOME") == 0) {
			found[2] = true;
		} else if (strcmp(curp, "PATH") == 0) {
			found[3] = true;
		} else if (strcmp(curp, "SHELL") == 0) {
			found[4] = true;
		} else if (strcmp(curp, "TMPDIR") == 0) {
			found[5] = true;
		} else if (strcmp(curp, "PWD") == 0) {
			found[6] = true;
		} else {
			char *keypair;
			char *value;
			value = cvec_get(job->jm->environment_variables, i + 1);
			if (!value)
				goto err_out;
			if (asprintf(&keypair, "%s=%s", curp, value) < 0)
				goto err_out;
			if (cvec_push(env, keypair) < 0) {
				free(keypair);
				goto err_out;
			}
			log_debug("set keypair: %s", keypair);
			free(keypair);
		}
	}

	/* TODO: refactor this to avoid goto and split out root env v.s. non-root env*/
job_has_no_environment:

	/* KLUDGE: when running as root, assume we are a system daemon and avoid adding any
	 * 	session-related variables.
	 * This is why we need a proper Domain variable for each job.
	 *
	 * The removal of these variables conforms to daemon(8) behavior on FreeBSD.
	 */
	uid = getuid();

	if (uid && !found[0]) {
		if (cvec_push(env, logname_var) < 0) goto err_out;
	}
	if (uid && !found[1]) {
		if (cvec_push(env, user_var) < 0) goto err_out;
	}
	if (!found[2]) {
		if (uid == 0) {
			if (cvec_push(env, "HOME=/") < 0) goto err_out;
		} else {
			if (asprintf(&buf, "HOME=%s", pwent->pw_dir) < 0) goto err_out;
			if (cvec_push(env, buf) < 0) goto err_out;
			free(buf);
			buf = NULL;
		}
	}
	if (!found[3]) {
		const char *path;

		if (uid == 0) {
			path = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin:/usr/local/sbin";
		} else {
			path = "PATH=/usr/bin:/bin:/usr/local/bin";
		}
		if (cvec_push(env, path) < 0) goto err_out;
	}
	if (uid && !found[4]) {
		if (asprintf(&buf, "SHELL=%s", pwent->pw_shell) < 0) goto err_out;
		if (cvec_push(env, buf) < 0) goto err_out;
		free(buf);
		buf = NULL;
	}
	if (uid && !found[5]) {
		if (cvec_push(env, "TMPDIR=/tmp") < 0) goto err_out;
	}
	if (!found[6]) {
		if (cvec_push(env, "PWD=/") < 0) goto err_out;
	}

	if (add_standard_environment_variables(env) < 0)
		goto err_out;

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

	return (env);

err_out:
	free(logname_var);
	free(user_var);
	free(buf);
	cvec_free(env);
	return NULL;
}

static inline int
exec_job(const job_t job, const struct passwd *pwent) 
{
	int rv;
	char *path;
	char **argv, **envp;
	cvec_t final_env;

	final_env = setup_environment_variables(job, pwent);
	if (final_env == NULL) {
		log_error("unable to set environment vars");
		return (-1);
	}
	envp = cvec_to_array(final_env);

	argv = cvec_to_array(job->jm->program_arguments);
	if (job->jm->program) {
		path = job->jm->program;
	} else {
		path = argv[0];
	}
	if (job->jm->enable_globbing) {
		//TODO: globbing
	}
	log_debug("exec: %s", path);
#if DEBUG
	log_debug("argv[]:");
	for (char **item = argv; *item; item++) {
		log_debug(" - arg: %s", *item);
	}
	log_debug("envp[]:");
	for (char **item = envp; *item; item++) {
		log_debug(" - env: %s", *item);
	}
#endif

	closelog();

	rv = execve(path, argv, envp);
	if (rv < 0) {
		log_errno("execve(2)");
		goto err_out;
    	}
	log_notice("executed job");

	cvec_free(final_env);
	return (0);

err_out:
	cvec_free(final_env);
	return -1;
}

void Job::redirect_stdio() {
	int fd;

	// TODO: simplify this by using .get<string>().c_str
	const char *stdin_path = this->manifest.json["StandardInPath"].get<string>().c_str();
	const char *stdout_path = this->manifest.json["StandardOutPath"].get<string>().c_str();
	const char *stderr_path = this->manifest.json["StandardErrorPath"].get<string>().c_str();

	log_debug("setting stdin path to %s", stdin_path);
	fd = open(stdin_path, O_RDONLY);
	if (fd < 0) {
		log_errno("open(2) of %s", stdin_path);
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

	log_debug("setting stdout path to %s", stdout_path);
	fd = open(stdout_path, O_CREAT | O_WRONLY, 0600);
	if (fd < 0) {
		log_errno("open(2) of %s", stdout_path);
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

	log_debug("setting stderr path to %s", stderr_path);
	fd = open(stderr_path, O_CREAT | O_WRONLY, 0600);
	if (fd < 0) {
		log_errno("open(2) of %s", stderr_path);
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

#ifndef NOFORK
	if (setsid() < 0) {
		log_errno("setsid");
		goto err_out;
	}
#endif
	if (reset_signal_handlers() < 0) {
		log_error("unable to reset signal handlers");
		goto err_out;
	}

	this->apply_resource_limits();

	const char *cwd = this->manifest.json["WorkingDirectory"].get<string>().c_str();
	if (chdir(cwd) < 0) {
		log_error("unable to chdir to %s", cwd);
		goto err_out;
	}

	/* TODO: deprecate the root_directory logic in favor of chroot_jail */
	const char *rootdir = this->manifest.json["RootDirectory"].get<string>().c_str();
	if (rootdir != nullptr && getuid() == 0) {
		if (chroot(rootdir) < 0) {
			log_error("unable to chroot to %s", rootdir);
			goto err_out;
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

	this->redirect_stdio();

	if (exec_job(job, pwent) < 0) {
		log_error("exec_job() failed");
		goto err_out;
	}

	return (0);

err_out:
	log_error("job %s failed to start; see previous log message for details", job->jm->label);
	return (-1);
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

int job_unload(job_t job)
{
	if (job->state == JOB_STATE_RUNNING) {
		log_debug("sending SIGTERM to process group %d", job->pid);
		if (kill(-1 * job->pid, SIGTERM) < 0) {
			log_errno("killpg(2) of pid %d", job->pid);
			/* not sure how to handle the error, we still want to clean up */
		}
		job->state = JOB_STATE_KILLED;
		//TODO: start a timer to send a SIGKILL if it doesn't die gracefully
	} else {
		//TODO: update the timer interval in timer.c?
		job->state = JOB_STATE_DEFINED;
	}

	keepalive_remove_job(job);
	if (job->jm->datasets)
		dataset_list_unload_handler(job->jm->datasets);
	if (job->jm->chroot_jail)
		chroot_jail_unload_handler(job->jm->chroot_jail);

	return 0;
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

	string group = this->manifest.json["GroupName"];
	if ((grent = getgrnam(group.c_str())) == NULL) {
		log_errno("getgrnam");
		throw std::system_error(errno, std::system_category());
	}
	this->gid = grent->gr_gid;
}

void Job::run() {
	this->acquire_resources();
	this->lookup_credentials();

	this->pid = fork();
	if (this->pid < 0) {
		log_errno("fork(2)");
		throw std::system_error(errno, std::system_category());
	} else if (this->pid == 0) {
		if (start_child_process(this) < 0) {
			//TODO: report failures to the parent
			exit(124);
		}
	} else {
		manager_pid_event_add(this->pid);
		log_debug("job %s started with pid %d", this->label.c_str(), this->pid);
		this->setState(JOB_STATE_RUNNING);
		// FIXME: close descriptors that the master process no longer needs
#if 0
		SLIST_FOREACH(jms, &job->jm->sockets, entry) {
			job_manifest_socket_close(jms);
		}
#endif
	}
}
