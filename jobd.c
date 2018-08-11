/*
 * Copyright (c) 2018 Mark Heily <mark@heily.com>
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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include "queue.h"
#else
#include <sys/event.h>
#include <sys/queue.h>
#endif /* __linux__ */
#include <sys/wait.h>
#include <unistd.h>

#include "toml.h"

static void sigchld_handler(int);
static void sigalrm_handler(int);
static void shutdown_handler(int);
static void reload_configuration(int);

/* Max length of a job ID. Equivalent to FILE_MAX */
#define JOB_ID_MAX 255

#define printlog(level, format,...) do {				\
	dprintf(logfd, "%d %s(%s:%d): "format"\n",		\
level, __func__, __FILE__, __LINE__, ## __VA_ARGS__); \
} while (0)

const static struct signal_handler {
	int signum;
	void (*handler)(int);
} signal_handlers[] = {
	{ SIGCHLD, &sigchld_handler },
	{ SIGHUP, &reload_configuration },
	{ SIGINT, &shutdown_handler },
	{ SIGTERM, &shutdown_handler },
	{ 0, NULL}
};

enum job_state {
	JOB_STATE_UNKNOWN,
	JOB_STATE_STARTING,
	JOB_STATE_RUNNING,
	JOB_STATE_STOPPING,
	JOB_STATE_STOPPED,
	JOB_STATE_ERROR
};

static struct config {
	char *configdir;
	char *socketpath;
	uint32_t shutdown_timeout;
} config;

#ifdef __linux__
static struct {
	int epfd;
	int signalfd;
} eventfds;

typedef struct epoll_event event_t;
#else
static int kqfd = -1;
typedef struct kevent event_t;
#endif

static int dequeue_signal(event_t *);

struct ipc_request {
	enum {
		IPC_REQUEST_UNDEFINED,
		IPC_REQUEST_START,
		IPC_REQUEST_STOP,
		IPC_REQUEST_MAX, /* Not a real opcode, just setting the maximum number of codes */
	} opcode;
	char job_id[JOB_ID_MAX + 1];
};

struct ipc_response {
	enum {
		IPC_RESPONSE_OK,
		IPC_RESPONSE_ERROR,
		IPC_RESPONSE_NOT_FOUND,
		IPC_RESPONSE_INVALID_STATE,
	} retcode;
};

static struct sockaddr_un ipc_server_addr;
static int ipc_sockfd = -1;
static bool jobd_is_shutting_down = false;
static volatile sig_atomic_t sigalrm_flag = 0;

static void daemonize(void);

struct job {
	LIST_ENTRY(job) entries;
	pid_t pid;
	enum job_state state;
	bool exited;
	int last_exit_status, term_signal;
	size_t incoming_edges;

	/* Items below here are parsed from the manifest */
    char **before, **after;
    char *id;
    char *description;
    bool enable, enable_globbing;
    char **environment_variables;
    gid_t gid;
    bool init_groups;
    bool keep_alive;
    char *title;
    char **argv;
    char *root_directory;
    char *standard_error_path;
    char *standard_in_path;
    char *standard_out_path;
    mode_t umask;
    uid_t uid;
	char *user_name;
    char *working_directory;
};

static int logfd = STDOUT_FILENO;
static LIST_HEAD(job_list, job) all_jobs = LIST_HEAD_INITIALIZER(all_jobs);

static void job_free(struct job *);
static struct job *find_job_by_id(const struct job_list *jobs, const char *id);
static void schedule(void);

static void
daemonize(void)
{
    int fd;

    switch (fork())
    {
    case -1:
        abort();
    case 0:
        break;
    default:
        _exit(0);
    }

    switch (fork())
    {
    case -1:
        abort();
    case 0:
        break;
    default:
        _exit(0);
    }

    if (setsid() == -1)
        abort();

    (void)chdir("/");

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1)
    {
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > 2)
            (void)close(fd);
    }
}

static void
usage(void) 
{
	printf("todo\n");
}

static bool
string_array_contains(char **haystack, const char *needle)
{
	char **p;
	if (haystack) {
		for (p = haystack; *p; p++) {
			if (!strcmp(*p, needle))
				return (true);
		}
	}
	return (false);
}

static void
string_array_free(char **strarr)
{
	char **p;

	if (strarr) {
		for (p = strarr; *p; p++) {
			free(*p);
		}
		free(strarr);
	}
}

static void 
job_free(struct job *job)
{

	if (job) {
		string_array_free(job->after);
		string_array_free(job->before);
		free(job->description);
		string_array_free(job->environment_variables);
		free(job->id);
		free(job->title);
		string_array_free(job->argv);
		free(job->root_directory);
		free(job->standard_error_path);
		free(job->standard_in_path);
		free(job->standard_out_path);
		free(job->working_directory);
		free(job->user_name);
		free(job);
	}
}

static int
parse_bool(bool *result, toml_table_t *tab, const char *key, bool default_value)
{
	const char *raw;
	int rv;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtob(raw, &rv)) {
		*result = rv;
		return (0);	
	} else {
 		return (-1);
	}
}

static int
parse_string(char **result, toml_table_t *tab, const char *key, const char *default_value)
{
	const char *raw;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		if (default_value) {
			*result = strdup(default_value);
			if (!*result) {
				printlog(LOG_ERR, "strdup(3): %s", strerror(errno));
				goto err;
			}
			return (0);
		} else {
			printlog(LOG_ERR, "no value provided for %s and no default", key);
			goto err;
		}
	}
	
	if (toml_rtos(raw, result)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = NULL;
		return (-1);
	}

	return (0);

err:
	*result = NULL;
	return (-1);
}

static int
parse_gid(gid_t *result, toml_table_t *tab, const char *key, const gid_t default_value)
{
	struct group *grp;
	const char *raw;
	char *buf;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtos(raw, &buf)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = -1;
		return (-1);
	}

	grp = getgrnam(buf);
	if (grp) {
		*result = grp->gr_gid;
		free(buf);
		return (0);
	} else {
		printlog(LOG_ERR, "group not found: %s", buf);
		free(buf);
		return (-1);
	}
}

static int
parse_uid(uid_t *result, toml_table_t *tab, const char *key, const uid_t default_value)
{
	struct passwd *pwd;
	const char *raw;
	char *buf;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtos(raw, &buf)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = -1;
		return (-1);
	}

	pwd = getpwnam(buf);
	if (pwd) {
		*result = pwd->pw_uid;
		free(buf);
		return (0);
	} else {
		printlog(LOG_ERR, "user not found: %s", buf);
		free(buf);
		return (-1);
	}
}

static int
uid_to_name(char **name, uid_t uid)
{
	struct passwd *pwd;

	pwd = getpwuid(uid);
	if (pwd) {
		*name = strdup(pwd->pw_name);
		if (*name == NULL) {
			printlog(LOG_ERR, "strdup(3): %s", strerror(errno));
			return (-1);	
		} else {
			return (0);
		}
	} else {
		printlog(LOG_ERR, "user not found for uid %d", uid);
		return (-1);
	}
}

static int
parse_program(struct job *j, toml_table_t *tab)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	size_t nelem;
	int i;

	arr = toml_array_in(tab, "Program");
	if (!arr) {
		return (0);
	}

	for (nelem = 0; (raw = toml_raw_at(arr, nelem)) != 0; nelem++) {}

	j->argv = calloc(nelem + 1, sizeof(char *));
	if (!j->argv) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}

	raw = toml_raw_at(arr, 0);
	if (!raw) {
		printlog(LOG_ERR, "empty Program array");
		return (-1);
	}
	if (toml_rtos(raw, &j->argv[0])) {
		printlog(LOG_ERR, "parse error parsing Program element 0");
	}
	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing Program element %d", i);
			return (-1);
		} else {
			j->argv[i] = val;
		}
	}
	j->argv[i] = NULL;

	return (0);
}

//TODO: refactor parse_program to use the func below
static int
parse_array_of_strings(char ***result, toml_table_t *tab, const char *top_key)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	size_t nelem;
	int i;
	char **strarr;

	arr = toml_array_in(tab, top_key);
	if (!arr) {
		*result = calloc(1, sizeof(char *));
		if (*result) {
			return (0);
		} else {
			printlog(LOG_ERR, "calloc: %s", strerror(errno));
			return (-1);
		}
	}

	for (nelem = 0; (raw = toml_raw_at(arr, nelem)) != 0; nelem++) {}

	strarr = calloc(nelem + 1, sizeof(char *));
	if (!strarr) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}

	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s element %d", top_key, i);
			//FIXME: free strarr
			return (-1);
		} else {
			strarr[i] = val;
		}
	}

	*result = strarr;

	return (0);
}

static int
parse_environment_variables(struct job *j, toml_table_t *tab)
{
	toml_table_t* subtab;
	const char *key;
	char *val, *keyval;
	const char *raw;
	size_t nkeys;
	int i;

	subtab = toml_table_in(tab, "EnvironmentVariables");
	if (!subtab) {
		j->environment_variables = malloc(sizeof(char *));
		j->environment_variables[0] = NULL;
		return (0);
	}

	for (nkeys = 0; (key = toml_key_in(subtab, nkeys)) != 0; nkeys++) {}

	j->environment_variables = calloc(nkeys + 1, sizeof(char *));
	if (!j->environment_variables) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}
		
	for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
		raw = toml_raw_in(subtab, key);
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s", key);
			j->environment_variables[i] = NULL;
			return (-1);
		}
		if (asprintf(&keyval, "%s=%s", key, val) < 0) {
			printlog(LOG_ERR, "asprintf: %s", strerror(errno));
			free(val);
		}
		free(val);

		j->environment_variables[i] = keyval;
	}
	j->environment_variables[i] = NULL;

	return (0);
}

static int
parse_job_file(struct job **result, const char *path, const char *id)
{
	FILE *fh;
	char errbuf[256];
	toml_table_t *tab = NULL;
	char *buf;
	struct job *j;

	j = calloc(1, sizeof(*j));
	if (!j) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}
		
	fh = fopen(path, "r");
	if (!fh) {
		printlog(LOG_ERR, "fopen(3) of %s: %s", path, strerror(errno));
		goto err;
	}

	tab = toml_parse_file(fh, errbuf, sizeof(errbuf));
	(void) fclose(fh);
	if (!tab) {
		printlog(LOG_ERR, "failed to parse %s", path);
		goto err;
	}

	j->id = strdup(id);
	if (parse_string(&j->description, tab, "Description", ""))
		goto err;
	if (parse_array_of_strings(&j->after, tab, "After"))
		goto err;
	if (parse_array_of_strings(&j->before, tab, "Before"))
		goto err;
	if (parse_bool(&j->enable, tab, "Enable", true))
		goto err;
	if (parse_bool(&j->enable_globbing, tab, "EnableGlobbing", false))
		goto err;
	if (parse_environment_variables(j, tab))
		goto err;
	if (parse_gid(&j->gid, tab, "Group", getgid()))
		goto err;
	if (parse_bool(&j->init_groups, tab, "InitGroups", true))
		goto err;
	if (parse_bool(&j->keep_alive, tab, "KeepAlive", false))
		goto err;
	if (parse_string(&j->title, tab, "Title", id))
		goto err;
	if (parse_program(j, tab))
		goto err;
	if (parse_string(&j->root_directory, tab, "RootDirectory", "/"))
		goto err;
	if (parse_string(&j->standard_error_path, tab, "StandardErrorPath", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_in_path, tab, "StandardInPath", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_out_path, tab, "StandardOutPath", "/dev/null"))
		goto err;
	
	if (parse_string(&buf, tab, "Umask", "0077"))
		goto err;
	sscanf(buf, "%hi", (unsigned short *) &j->umask);
	free(buf);

	if (parse_uid(&j->uid, tab, "User", getuid()))
		goto err;
	if (uid_to_name(&j->user_name, j->uid))
		goto err;

	if (parse_string(&j->working_directory, tab, "WorkingDirectory", "/"))
		goto err;

	toml_free(tab);
	*result = j;
	return (0);

err:
	toml_free(tab);
	job_free(j);
	return (-1);
}

static struct job *
find_job_by_id(const struct job_list *jobs, const char *id)
{
	struct job *job;

	LIST_FOREACH(job, jobs, entries) {
		if (!strcmp(id, job->id)) {
			return job;
		}
	}
	return (NULL);
}

/* This sorting algorithm is not efficient, but is fairly simple. */
static int
topological_sort(struct job_list *dest, struct job_list *src)
{
	struct job *cur, *tmp, *tail;
	char **id_p;

	/* Find all incoming edges and keep track of how many each node has */
	LIST_FOREACH(cur, src, entries) {
		LIST_FOREACH(tmp, src, entries) {
			if (cur != tmp && string_array_contains(cur->after, tmp->id)) {
				printlog(LOG_DEBUG, "edge from %s to %s", tmp->id, cur->id);
				cur->incoming_edges++;
			}
		}
	}
	LIST_FOREACH(cur, src, entries) {
		LIST_FOREACH(tmp, src, entries) {
			if (cur != tmp && string_array_contains(cur->before, tmp->id)) {
				printlog(LOG_DEBUG, "edge from %s to %s", cur->id, tmp->id);
				tmp->incoming_edges++;
			}
		}
	}

	/* Iteratively remove nodes with zero incoming edges */
	tail = NULL;
	while (!LIST_EMPTY(src)) {
		cur = NULL;
		LIST_FOREACH(tmp, src, entries) {
			if (tmp->incoming_edges == 0) {
				cur = tmp;
				break;
			}
		}

		if (cur) {
			/* Update edge counts to reflect the removal of <cur> */
			for (id_p = cur->before; *id_p; id_p++) {
				tmp = find_job_by_id(src, *id_p);
				if (tmp) {
					printlog(LOG_DEBUG, "removing edge from %s to %s", cur->id, tmp->id);
					tmp->incoming_edges--;
				}
			}
			LIST_FOREACH(tmp, src, entries) {
				if (cur != tmp && string_array_contains(tmp->after, cur->id)) {
					printlog(LOG_DEBUG, "removing edge from %s to %s", cur->id, tmp->id);
					tmp->incoming_edges--;
				}
			}

			/* Remove <cur> and place it on the sorted destination list */
			LIST_REMOVE(cur, entries);
			if (tail) {
				LIST_INSERT_AFTER(tail, cur, entries);
			} else {
				LIST_INSERT_HEAD(dest, cur, entries);
			}
			tail = cur;
			continue;
		} else {
			/* Any leftover nodes are part of a cycle. */
			LIST_FOREACH_SAFE(cur, src, entries, tmp) {
				LIST_REMOVE(cur, entries);
				printlog(LOG_WARNING, "job %s is part of a cycle", cur->id);
				cur->state = JOB_STATE_ERROR;
				LIST_INSERT_AFTER(tail, cur, entries);
				tail = cur;
			}
		}
	}

	return (0);
}

static int
unspool(const char *configdir)
{
	struct job_list tmpjobs;
	DIR	*dirp;
	struct dirent *entry;
	struct job *job;
	char *path;

	LIST_INIT(&tmpjobs);

	if ((dirp = opendir(configdir)) == NULL)
		err(1, "opendir(3) of %s", configdir);

	while (dirp) {
        errno = 0;
        entry = readdir(dirp);
        if (errno != 0)
            err(1, "readdir(3)");
		if (!entry)
            break;
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		if (asprintf(&path, "%s/%s", configdir, entry->d_name) < 0)
			err(1, "asprintf");
		printlog(LOG_DEBUG, "parsing %s", path);
		if (parse_job_file(&job, path, entry->d_name) != 0) {
			printlog(LOG_ERR, "error parsing %s", path);
			free(path);
			continue;
		}
		free(path);

		LIST_INSERT_HEAD(&tmpjobs, job, entries);
	}
	if (closedir(dirp) < 0) {
		err(1, "closedir(3)");
	}

	if (topological_sort(&all_jobs, &tmpjobs) < 0) {
		errx(1, "topological_sort() failed");
	}

	return (0);
}

static int
redirect_file_descriptor(int oldfd, const char *path, int flags, mode_t mode)
{
	int newfd;

	newfd = open(path, flags, mode);
	if (newfd < 0) {
		printlog(LOG_ERR, "open(2) of %s: %s", path, strerror(errno));
		return (-1);
	}
	if (dup2(newfd, oldfd) < 0) {
		printlog(LOG_ERR, "dup2(2): %s", strerror(errno));
		(void) close(newfd);
		return (-1);
	}
	if (close(newfd) < 0) {
		printlog(LOG_ERR, "close(2): %s", strerror(errno));
		return (-1);
	}

	return (0);
}

static int
start(struct job *job)
{
	sigset_t mask;

	if (job->state != JOB_STATE_STOPPED)
		return (-IPC_RESPONSE_INVALID_STATE);

	job->pid = fork();
	if (job->pid < 0) {
		printlog(LOG_ERR, "fork(2): %s", strerror(errno));
		return (-1);
	} else if (job->pid == 0) {
		(void)setsid();

		sigfillset(&mask);
		(void) sigprocmask(SIG_UNBLOCK, &mask, NULL);

		//TODO: setrlimit
		if (chdir(job->working_directory) < 0) {
			printlog(LOG_ERR, "chdir(2) to %s: %s", job->working_directory, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (getuid() == 0) {
			if (strcmp(job->root_directory, "/") && (chroot(job->root_directory) < 0)) {
				printlog(LOG_ERR, "chroot(2) to %s: %s", job->root_directory, strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (job->init_groups && (initgroups(job->user_name, job->gid) < 0)) {
				printlog(LOG_ERR, "initgroups(3): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (setgid(job->gid) < 0) {
				printlog(LOG_ERR, "setgid(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
#ifndef __GLIBC__
			/* KLUDGE: above is actually a test for BSD */
			if (setlogin(job->user_name) < 0) {
				printlog(LOG_ERR, "setlogin(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
#endif
			if (setuid(job->uid) < 0) {
				printlog(LOG_ERR, "setuid(2): %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		(void) umask(job->umask);

		//TODO this->setup_environment();
		//this->createDescriptors();

		// TODO: deal with globbing

		if (logfd == STDOUT_FILENO) {
			logfd = dup(logfd);
			if (fcntl(logfd, F_SETFD, FD_CLOEXEC) < 0)
				err(1, "fcntl(2)");
		}
		if (redirect_file_descriptor(STDIN_FILENO, job->standard_in_path, O_RDONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDIN");
			exit(EXIT_FAILURE);
		}
		if (redirect_file_descriptor(STDOUT_FILENO, job->standard_out_path, O_CREAT | O_WRONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDOUT");
			exit(EXIT_FAILURE);
		}
		if (redirect_file_descriptor(STDERR_FILENO, job->standard_error_path, O_CREAT | O_WRONLY, 0600) < 0) {
			printlog(LOG_ERR, "unable to redirect STDERR");
			exit(EXIT_FAILURE);
		}
		if (execve(job->argv[0], job->argv, job->environment_variables) < 0) {
			printlog(LOG_ERR, "execve(2): %s", strerror(errno));
			exit(EXIT_FAILURE);
    	}
		/* NOTREACHED */
	} else {
		/*
		job->state = JOB_STATE_STARTING;
		
		TODO: allow the use of a check script that waits for the service to finish initializing.
		*/
		job->state = JOB_STATE_RUNNING;

		//TODO: manager
		printlog(LOG_DEBUG, "job %s started with pid %d", job->id, job->pid);
	}

	return (0);
}

static int
stop(struct job *job)
{
	if (job->state != JOB_STATE_RUNNING)
		return (-IPC_RESPONSE_INVALID_STATE);

	printlog(LOG_DEBUG, "sending SIGTERM to job %s (pid %d)", job->id, job->pid);
	(void) kill(job->pid, SIGTERM); //TODO: errorhandle
	job->state = JOB_STATE_STOPPING;
	//TODO: start a timeout
	return (0);
}

// static int
// restart(struct job *job)
// {
// 	(void)stop(job);
// 	//TODO: wait
// 	return (start(job));
// }

static void
schedule(void)
{
	struct job *job;

	LIST_FOREACH(job, &all_jobs, entries) {
		switch (job->state) {
			case JOB_STATE_UNKNOWN:
				job->state = JOB_STATE_STOPPED;
				if (job->enable)
					start(job);
				break;
			case JOB_STATE_STOPPED:
				break;
			default:
				//err???
				break;
		}
	}
}

static void
reaper(struct job_list *jobs, pid_t pid, int status)
{
	struct job *job;

	printlog(LOG_DEBUG, "reaping PID %d", pid);

	LIST_FOREACH(job, jobs, entries) {
		if (job->pid == pid) {
			if (job->state != JOB_STATE_STOPPING) {
				printlog(LOG_NOTICE, "job %s terminated unexpectedly", job->id);
				//TODO: mark it as errored
			}
			job->state = JOB_STATE_STOPPED;
			job->pid = 0;
			job->exited = true;
			if (WIFEXITED(status)) {
				job->last_exit_status = WEXITSTATUS(status);
				job->term_signal = 0;
			} else if (WIFSIGNALED(status)) {
				job->last_exit_status = -1;
				job->term_signal = WTERMSIG(status);
			} else {
				job->term_signal = -1;
				job->last_exit_status = -1;
				printlog(LOG_ERR, "unhandled exit status");
			}
			printlog(LOG_DEBUG, "job %s (pid %d) exited with status=%d term_signal=%d",
    		    job->id, pid, job->last_exit_status, job->term_signal);

			if (jobd_is_shutting_down) {
				// Do not reschedule. 
			} else {
				// TODO: reschedule
			}

			return;
		}
	}
	printlog(LOG_ERR, "no job associated with pid %d", pid);
}

static void
shutdown_handler(int signum)
{
	struct job_list shutdown_list;
	struct job *job, *tmpjob;
	pid_t pid;
	int status;

	printlog(LOG_NOTICE, "terminating due to signal %d", signum);
    
	jobd_is_shutting_down = true;

	LIST_INIT(&shutdown_list);
	LIST_FOREACH_SAFE(job, &all_jobs, entries, tmpjob) {
		LIST_REMOVE(job, entries);
		if (job->state == JOB_STATE_RUNNING) {
			LIST_INSERT_HEAD(&shutdown_list, job, entries);
		} else {
			job_free(job);
		}	
 	}

	LIST_FOREACH_SAFE(job, &shutdown_list, entries, tmpjob) {
		stop(job);
		pid = wait(&status);
		if (pid > 0) {
			reaper(&shutdown_list, pid, status);
		} else {
			if (errno == EINTR) {
				if (sigalrm_flag) {
					printlog(LOG_ERR, "timeout: one or more jobs failed to terminate");
				} else {
					printlog(LOG_ERR, "caught unhandled signal");
				}
			} else if (errno == ECHILD) {
				printlog(LOG_WARNING, "no remaining children to wait for");
				break;			
			} else {
				printlog(LOG_ERR, "wait(2): %s", strerror(errno));
			}
			exit(EXIT_FAILURE);
		}
		LIST_REMOVE(job, entries);
		job_free(job);
	}

	if (signum == SIGINT) {
		exit(EXIT_FAILURE);
	} else if (signum == SIGTERM) {
		exit(EXIT_SUCCESS);
	}
}

static void
reload_configuration(int signum __attribute__((unused)))
{
	unspool(config.configdir);
	schedule();
}

// static void
// list_jobs(struct ipc_response_header *res)
// {
// 	struct job *job;
// 	char *p;
// 	size_t len;

// 	p = ipc_buf;
// 	LIST_FOREACH(job, &all_jobs, entries) {
// 		len = strlen(job->id);
// 		sprintf(p, "%s\n", job->id);
// 		len += 2; //BOUNDS
// 	}
// 	res->status = 0;
// 	res->message[0] = "\0";
// }

static int
parse_jobd_conf(const char *user_provided_path)
{
	char *path;
	FILE *fh;

	if (user_provided_path) {
		if ((path = strdup(user_provided_path)) == NULL)
			goto enomem;
	} else if (getuid() == 0) {
		if ((path = strdup("/etc/jobd.conf")) == NULL)
			goto enomem;
	} else {
		if (asprintf(&path, "%s/.config/jobd.conf", getenv("HOME")) < 0)
			goto enomem;
	}

	fh = fopen(path, "r");
	if (fh) {
		err(1, "TODO");
	} else {
		if (errno != ENOENT) {
			printlog(LOG_ERR, "fopen(3) of %s: %s", path, strerror(errno));
			goto err;
		}

		/* Apply default configuration settings */
		config.shutdown_timeout = 300;
		if (getuid() == 0) {
			config.configdir = strdup("/etc/job.d");
			config.socketpath = strdup("/var/run/jobd.sock");
		} else {
			if (asprintf(&config.configdir, "%s/.config/job.d", getenv("HOME")) < 0)
				goto enomem;
			if (asprintf(&config.socketpath, "%s/.jobd.sock", getenv("HOME")) < 0)
				goto enomem;
		}
		if (!config.configdir) {
			printlog(LOG_ERR, "allocation failed: %s", strerror(errno));
			goto err;
		}
	}

	free(path);
	return (0);

enomem:
	free(path);
	printlog(LOG_ERR, "allocation failed: %s", strerror(errno));
	return (-1);

err:
	free(path);
	return (-1);
}

static void
ipc_server_handler(event_t *ev __attribute__((unused)))
{
	ssize_t bytes;
	struct sockaddr_un client_addr;
	socklen_t len;
	struct ipc_request req;
	struct ipc_response res;
	int (*jump_table[IPC_REQUEST_MAX])(struct job *) = {
		NULL,
		&start,
		&stop
	};
	
	len = sizeof(struct sockaddr_un);
	bytes = recvfrom(ipc_sockfd, &req, sizeof(req), 0, (struct sockaddr *) &client_addr, &len);
    if (bytes < 0) {
		err(1, "recvfrom(2)");
	}

	printlog(LOG_DEBUG, "got IPC request; opcode=%d job_id=%s", req.opcode, req.job_id);
	if (req.opcode > 0 && req.opcode < IPC_REQUEST_MAX) {
		struct job *job = find_job_by_id(&all_jobs, req.job_id);
		if (job) {
			res.retcode = (*jump_table[req.opcode])(job);
		} else {
			res.retcode = IPC_RESPONSE_NOT_FOUND;
		}
	}
    
	printlog(LOG_DEBUG, "sending IPC response; retcode=%d", res.retcode);
	if (sendto(ipc_sockfd, &res, sizeof(res), 0, (struct sockaddr*) &client_addr, len) < 0) {
		err(1, "sendto(2)");
	}
}

static int
ipc_client_request(int opcode, char *job_id)
{
	ssize_t bytes;
	struct sockaddr_un sa_to, sa_from;
	socklen_t len;
	struct ipc_request req;
	struct ipc_response res;

	memset(&sa_to, 0, sizeof(struct sockaddr_un));
    sa_to.sun_family = AF_UNIX;
	if (strlen(config.socketpath) > sizeof(sa_to.sun_path) - 1)
		errx(1, "socket path is too long");
    strncpy(sa_to.sun_path, config.socketpath, sizeof(sa_to.sun_path) - 1);

	req.opcode = opcode;
	if (job_id) {
		strncpy((char*)&req.job_id, job_id, JOB_ID_MAX);
		req.job_id[JOB_ID_MAX] = '\0';
	} else {
		req.job_id[0] = '\0';
	}
	len = (socklen_t) sizeof(struct sockaddr_un);
	if (sendto(ipc_sockfd, &req, sizeof(req), 0, (struct sockaddr*) &sa_to, len) < 0) {
		err(1, "sendto(2)");
	}
	printlog(LOG_DEBUG, "sent IPC request; opcode=%d job_id=%s", req.opcode, req.job_id);

	len = sizeof(struct sockaddr_un);
	bytes = recvfrom(ipc_sockfd, &res, sizeof(res), 0, (struct sockaddr *) &sa_from, &len);
    if (bytes < 0) {
		err(1, "recvfrom(2)");
	}
	printlog(LOG_DEBUG, "got IPC response; retcode=%d", res.retcode);
	return (res.retcode);
}

static void
create_event_queue(void)
{
#ifdef __linux__
	sigset_t mask;
	struct epoll_event ev;

	if ((eventfds.epfd = epoll_create1(EPOLL_CLOEXEC)) < 0)
		err(1, "epoll_create1(2)");

	sigemptyset(&mask);

	eventfds.signalfd = signalfd(-1, &mask, SFD_CLOEXEC);
	if (eventfds.signalfd < 0)
		err(1, "signalfd(2)");

	ev.events = EPOLLIN;
	ev.data.ptr = &dequeue_signal;
	if (epoll_ctl(eventfds.epfd, EPOLL_CTL_ADD, eventfds.signalfd, &ev) < 0)
		err(1, "epoll_ctl(2)");
	
	ev.events = EPOLLIN;
	ev.data.ptr = &ipc_server_handler;
	if (epoll_ctl(eventfds.epfd, EPOLL_CTL_ADD, ipc_sockfd, &ev) < 0)
		err(1, "epoll_ctl(2)");
#else
	struct kevent kev;

	if ((kqfd = kqueue()) < 0)
		err(1, "kqueue(2)");
	if (fcntl(kqfd, F_SETFD, FD_CLOEXEC) < 0)
		err(1, "fcntl(2)");

	EV_SET(&kev, ipc_sockfd, EVFILT_READ, EV_ADD, 0, 0,
			(void *)&ipc_server_handler);
	if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
		err(1, "kevent(2)");
#endif
}

static void
register_signal_handlers(void)
{
	const struct signal_handler *sh;
	struct sigaction sa;

	/* Special case: disable SA_RESTART on alarms */
	sa.sa_handler = sigalrm_handler;
  	sigemptyset(&sa.sa_mask);
  	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) < 0)
		err(1, "sigaction(2)");

#ifdef __linux__
	sigset_t mask;

	sigemptyset(&mask);
	for (sh = &signal_handlers[0]; sh->signum; sh++) {
		sigaddset(&mask, sh->signum);
	}
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		err(1, "sigprocmask(2)");

	eventfds.signalfd = signalfd(eventfds.signalfd, &mask, SFD_CLOEXEC);
	if (eventfds.signalfd < 0)
		err(1, "signalfd(2)");

#else
	struct kevent kev;

	for (sh = &signal_handlers[0]; sh->signum; sh++) {
		if (signal(sh->signum, (sh->signum == SIGCHLD ? SIG_DFL : SIG_IGN)) == SIG_ERR)
			err(1, "signal(2): %d", sh->signum);

		EV_SET(&kev, sh->signum, EVFILT_SIGNAL, EV_ADD, 0, 0,
				(void *)&dequeue_signal);
		if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
			err(1, "kevent(2)");
	}
#endif
}

static int
dequeue_signal(event_t *ev)
{
	const struct signal_handler *sh;
	int signum;

#ifdef __linux__
	struct signalfd_siginfo fdsi;
	ssize_t sz;

	sz = read(eventfds.signalfd, &fdsi, sizeof(fdsi));
	if (sz != sizeof(fdsi))
		err(1, "invalid read");

	signum = fdsi.ssi_signo;
#else
	signum = ev->ident;
#endif

	for (sh = &signal_handlers[0]; sh->signum; sh++) {
		if (sh->signum == signum) {
			printlog(LOG_DEBUG, "caught signal %d", signum);
			sh->handler(signum);
			return (0);
		}
	}
	printlog(LOG_ERR, "caught unhandled signal: %d", signum);
	return (-1);
}

static void
dispatch_event(void)
{
	void (*cb)(event_t *);
	event_t ev;
	int rv;

	for (;;) {
#ifdef __linux__
		rv = epoll_wait(eventfds.epfd, &ev, 1, -1);
#else
		rv = kevent(kqfd, NULL, 0, &ev, 1, NULL);
#endif
	puts("got event");
		if (rv < 0) {
			if (errno == EINTR) {
				printlog(LOG_ERR, "unexpected wakeup from unhandled signal");
				continue;
			} else {
				printlog(LOG_ERR, "%s", strerror(errno));
				//crash? return (-1);				
			}
		} else if (rv == 0) {
				puts("fuckt");

			continue;
		} else {
							puts("yay");
#ifdef __linux__
		cb = (void (*)(event_t *)) ev.data.ptr;
#else
		cb = (void (*)(event_t *)) ev.udata;
#endif
		(*cb)(&ev);
		}
	}
}

static void
sigalrm_handler(int signum)
{
	(void) signum;
	sigalrm_flag = 1;
}

static void
sigchld_handler(int signum __attribute__((unused)))
{
	int status;
	pid_t pid;

	for (;;) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			reaper(&all_jobs, pid, status);
		} else {
			break;
		}
	}
}

static int
create_ipc_socket(const char *socketpath, int is_server)
{
	int sd;
	struct sockaddr_un saun;

    sd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sd < 0)
		err(1, "socket(2)");

	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_UNIX;
	if (strlen(socketpath) > sizeof(saun.sun_path) - 1)
		errx(1, "socket path is too long");
	strncpy(saun.sun_path, socketpath, sizeof(saun.sun_path) - 1);
		
	if (is_server) {
		memcpy(&ipc_server_addr, &saun, sizeof(ipc_server_addr));

		if (bind(sd, (struct sockaddr *) &saun, sizeof(saun)) < 0) {
			if (errno == EADDRINUSE) {
				//TODO: check for multiple jobd instances
				unlink(socketpath);
				if (bind(sd, (struct sockaddr *) &saun, sizeof(saun)) < 0)
					err(1, "bind(2)");
			} else {
				err(1, "bind(2)");
			}
		}
	} else {
		memset(&saun, 0, sizeof(saun));
    	saun.sun_family = AF_UNIX;
		if (bind(sd, (struct sockaddr *) &saun, sizeof(saun)) < 0)
			err(1, "connect(2)");
	}

	return (sd);
}

void
server_main(int argc, char *argv[])
{
	int c, daemon, verbose;

	verbose = 0;
    daemon = 1;
	while ((c = getopt(argc, argv, "fv")) != -1) {
		switch (c) {
		case 'f':
				daemon = 0;
				break;
		case 'v':
				verbose = 1;
				break;
		default:
				usage();
				break;
		}
	}

	if (daemon)
        daemonize();

    if (verbose)
		errx(1, "TODO");

	create_event_queue();
	register_signal_handlers();
	(void)kill(getpid(), SIGHUP);

	for (;;) {
		dispatch_event();
	}
	/* NOTREACHED */
}

void
client_main(int argc, char *argv[])
{
	char *command = argv[1];
	int rv;

	if (!command)
		errx(1, "command expected");
		
	if (!strcmp(command, "help")) {
		puts("no help yet");
		rv = -1;
	// } else if (!strcmp(command, "list")) {
	// 	ipc_client_request();
	} else if (!strcmp(command, "start")) {
		rv = ipc_client_request(IPC_REQUEST_START, argv[2]);
	} else if (!strcmp(command, "stop")) {
		rv = ipc_client_request(IPC_REQUEST_STOP, argv[2]);
	} else if (!strcmp(command, "restart")) {
		ipc_client_request(IPC_REQUEST_STOP, argv[2]);//ERRCHECK
		rv = ipc_client_request(IPC_REQUEST_START, argv[2]);
	} else {
		printlog(LOG_ERR, "unrecognized command: %s", command);
		errx(1, "invalid command");
	}
	if (rv != IPC_RESPONSE_OK) {
		fprintf(stderr, "ERROR: Request failed with retcode %d\n", rv);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
    int is_server;

    if (parse_jobd_conf(NULL) < 0)
	    err(1, "bad configuration file");

	is_server = !strcmp(basename(argv[0]), "jobd");

	ipc_sockfd = create_ipc_socket(config.socketpath, is_server);

	if (is_server)
		server_main(argc, argv);
	else 
		client_main(argc, argv);
}