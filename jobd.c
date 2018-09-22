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
#include <limits.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/procctl.h>
#endif /* __FreeBSD__ */
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#else
#include <sys/event.h>
#include <sys/queue.h>
#endif /* __linux__ */
#include <sys/wait.h>
#include <unistd.h>

#include "array.h"
#include "config.h"
#include "database.h"
#include "logger.h"
#include "job.h"
#include "ipc.h"
#include "pidfile.h"
#include "tsort.h"

static void sigchld_handler(int);
static void sigalrm_handler(int);
static void shutdown_handler(int);
static void reload_configuration(int);

/* Max length of a job ID. Equivalent to FILE_MAX */
#define JOB_ID_MAX 255

static struct job *scheduler_lock = NULL;
static struct pidfh *pidfile_fh;

static const struct signal_handler {
	int signum;
	void (*handler)(int);
} signal_handlers[] = {
	{ SIGCHLD, &sigchld_handler },
	{ SIGHUP, &reload_configuration },
	{ SIGINT, &shutdown_handler },
	{ SIGTERM, &shutdown_handler },
	{ 0, NULL}
};

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

static bool jobd_is_system_manager = false;
static bool jobd_is_shutting_down = false;
static volatile sig_atomic_t sigalrm_flag = 0;

static void daemonize(void);
static struct job_list all_jobs = LIST_HEAD_INITIALIZER(all_jobs);

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

static int
unspool(void)
{
	struct job_list tmpjobs;

	LIST_INIT(&tmpjobs);

	if (job_db_select_all(&tmpjobs) < 0)
		return (-1);
	
	if (topological_sort(&all_jobs, &tmpjobs) < 0) {
		return (-1);
	}

	return (0);
}

static void
schedule(void)
{
	struct job *job;

	printlog(LOG_DEBUG, "scheduling jobs");
	
	if (scheduler_lock) {
		printlog(LOG_DEBUG, "will not run scheduler; an exclusive job is in progress");
		return;
	}

	LIST_FOREACH(job, &all_jobs, entries) {
		switch (job->state) {
			case JOB_STATE_UNKNOWN:
				job->state = JOB_STATE_STOPPED;
				if (job->enable) {
					job_start(job);
					if (job->exclusive && job->state == JOB_STATE_RUNNING) {
						printlog(LOG_DEBUG, "scheduler locked by %s", job->id);
						scheduler_lock = job;
						return;
					}
				}
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
				if (scheduler_lock == job) {
					printlog(LOG_DEBUG, "unlocking the scheduler");
					scheduler_lock = NULL;
					schedule();
				}
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

	/* This has the effect of reversing the list, causing jobs to be
	   stopped in the reverse order that they were started. */
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
		if (job_stop(job) < 0) {
			printlog(LOG_ERR, "unable to stop job: %s", job->id);
			continue;
		}
		if (job->state == JOB_STATE_STOPPING) {
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
		}
		LIST_REMOVE(job, entries);
		job_free(job);
	}

	if (pidfile_fh)
		pidfile_remove(pidfile_fh);

	if (signum == SIGINT) {
		exit(EXIT_FAILURE);
	} else if (signum == SIGTERM) {
		exit(EXIT_SUCCESS);
	}
}

static void
reload_configuration(int signum __attribute__((unused)))
{
	unspool();
	schedule();
}

static void
ipc_server_handler(event_t *ev __attribute__((unused)))
{
	ssize_t bytes;
	struct sockaddr_un client_addr;
	socklen_t len;
	int sfd;
	struct ipc_request req;
	struct ipc_response res;
	int (*jump_table[IPC_REQUEST_MAX])(struct job *) = {
		NULL,
		&job_start,
		&job_stop,
		&job_enable,
		&job_disable,
	};
	
	sfd = ipc_get_sockfd();
	len = sizeof(struct sockaddr_un);
	bytes = recvfrom(sfd, &req, sizeof(req), 0, (struct sockaddr *) &client_addr, &len);
    if (bytes < 0) {
		err(1, "recvfrom(2)");
	}

	printlog(LOG_DEBUG, "got IPC request; opcode=%d job_id=%s", req.opcode, req.job_id);
	if (req.opcode > 0 && req.opcode < IPC_REQUEST_MAX) {
		struct job *job = job_list_lookup(&all_jobs, req.job_id);
		if (job) {
			res.retcode = (*jump_table[req.opcode])(job);
		} else {
			res.retcode = IPC_RESPONSE_NOT_FOUND;
		}
	}
    
	printlog(LOG_DEBUG, "sending IPC response; retcode=%d", res.retcode);
	if (sendto(sfd, &res, sizeof(res), 0, (struct sockaddr*) &client_addr, len) < 0) {
		err(1, "sendto(2)");
	}
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
	if (epoll_ctl(eventfds.epfd, EPOLL_CTL_ADD, ipc_get_sockfd(), &ev) < 0)
		err(1, "epoll_ctl(2)");
#else
	struct kevent kev;

	if ((kqfd = kqueue()) < 0)
		err(1, "kqueue(2)");
	if (fcntl(kqfd, F_SETFD, FD_CLOEXEC) < 0)
		err(1, "fcntl(2)");

	EV_SET(&kev, ipc_get_sockfd(), EVFILT_READ, EV_ADD, 0, 0,
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

	(void) ev;
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
		if (rv < 0) {
			if (errno == EINTR) {
				printlog(LOG_ERR, "unexpected wakeup from unhandled signal");
				continue;
			} else {
				printlog(LOG_ERR, "%s", strerror(errno));
				//crash? return (-1);				
			}
		} else if (rv == 0) {
			printlog(LOG_DEBUG, "spurious wakeup");
			continue;
		} else {
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

/* TODO: for each job, spawn a dedicated subreaper process.
   Right now, this only serves the purpose of documenting orphan
   processes in the logs */
static void
become_a_subreaper(void)
{
#if defined(__FreeBSD__)
	if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, 0) < 0)
		printlog(LOG_ERR, "system call failed: %s", strerror(errno));
#elif defined(__linux__)
	if (prctl(PR_SET_CHILD_SUBREAPER, 1) < 0)
		printlog(LOG_ERR, "system call failed: %s", strerror(errno));
#else
	printlog(LOG_WARNING, "subreaper feature is not implemented");
#endif
}

static void
create_pid_file(void)
{
	char path[PATH_MAX];
	pid_t otherpid;
	int rv;

	rv = snprintf((char *)&path, sizeof(path), "%s/jobd.pid", compile_time_option.runstatedir);
	if (rv >= (int)sizeof(path)) {
		printlog(LOG_ERR, "unable to create pidfile; buffer too small");
		abort();
	}

	pidfile_fh = pidfile_open(path, 0600, &otherpid);
	if (pidfile_fh == NULL) {
		if (errno == EEXIST) {
			printlog(LOG_ERR, "daemon already running, pid: %jd.\n", (intmax_t) otherpid);
		} else {
			printlog(LOG_ERR, "cannot open or create pidfile: %s\n", path);
		}
		exit(EXIT_FAILURE);
	}
	
	printlog(LOG_DEBUG, "created pidfile %s", path);
}


//todo: move to sysmgr.c
int
mount_runstatedir(void)
{
#ifdef __FreeBSD__
	char errmsg[255];

	struct iovec iov[8] = {
		{.iov_base = "fstype", .iov_len = sizeof("fstype")},
		{.iov_base = "tmpfs", .iov_len = sizeof("tmpfs")},
		{.iov_base = "from", .iov_len = sizeof("from")},
		{.iov_base = "tmpfs", .iov_len = sizeof("tmpfs")},
		{.iov_base = "errmsg", .iov_len = sizeof("errmsg")},
		{.iov_base = &errmsg, .iov_len = sizeof(errmsg)},
		{.iov_base = "fspath", .iov_len = sizeof("fspath")},
		{.iov_base = __DECONST(void *, compile_time_option.runstatedir),
		 .iov_len = (strlen(compile_time_option.runstatedir) + 1)}};

	if (kldload("/boot/kernel/tmpfs.ko") < 0) {
		if (errno != EEXIST) {
			printlog(LOG_ERR, "kldload(2): %s", strerror(errno));
			return (-1);
		}
	}

	if (nmount(iov, 8, 0) < 0) {
		printlog(LOG_ERR, "nmount(2): %s: %s", strerror(errno), (char*)errmsg);
		return (-1);
	}

#else
	//todo linux stuff
#endif
	return (0);
}

int
main(int argc, char *argv[])
{
	int c, daemon, verbose;

	/* TODO: pass this in as a command line option or env var instead. */
	jobd_is_system_manager = ((getppid() == 1) && (getuid() == 0));

	if (logger_init() < 0) {
		errx(1, "logger_init");
	}
	if (jobd_is_system_manager) {
		mount_runstatedir();
	}
	if (db_init() < 0) {
		printlog(LOG_ERR, "unable to initialize the database routines");
		exit(EXIT_FAILURE);
	}
	if (db_open(NULL, DB_OPEN_CREATE_VOLATILE) < 0) { 
		printlog(LOG_ERR, "unable to open the database");
		exit(EXIT_FAILURE);
	}
	if (ipc_init(NULL) < 0) {
		printlog(LOG_ERR, "ipc_init() failed");
		exit(EXIT_FAILURE);
	}

	if (getppid() == 1)
		verbose = 1;		/* TEMPORARY: for debugging */
	else
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
				fputs("unrecognized command option", stderr);
				usage();
				exit(EXIT_FAILURE);
				break;
		}
	}

	create_pid_file();

	if (daemon)
        daemonize();
	
	pidfile_write(pidfile_fh);

	logger_set_verbose(verbose);
	if (ipc_bind() < 0) {
		printlog(LOG_ERR, "IPC bind failed");
		//FIXME: IPC is broken here, maybe try to fix it later?
	}
	become_a_subreaper();
	create_event_queue();
	register_signal_handlers();
	(void)kill(getpid(), SIGHUP);

	for (;;) {
		dispatch_event();
	}
	/* NOTREACHED */
}
