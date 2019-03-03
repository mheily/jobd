/*
 * Copyright (c) 2019 Mark Heily <mark@heily.com>
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

#include <err.h>
#include <errno.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <memory.h>
#include <signal.h>

#else
#include <sys/event.h>
#include <sys/queue.h>
#endif /* __linux__ */

#include "event_loop.h"
#include "logger.h"

#ifdef __linux__
static struct {
    int epfd;
    int signalfd;
} eventfds;

#else
static int kqfd = -1;
#endif

static int dequeue_signal(event_t *);

static struct event_loop_options elopt;

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

    for (sh = &elopt.signal_handlers[0]; sh->signum; sh++) {
        if (sh->signum == signum) {
            printlog(LOG_DEBUG, "caught signal %d", signum);
            sh->handler(signum);
            return (0);
        }
    }
    return printlog(LOG_ERR, "caught unhandled signal: %d", signum);
}

static int
create_event_queue(void)
{
#ifdef __linux__
    sigset_t mask;
    struct epoll_event ev;

    if ((eventfds.epfd = epoll_create1(EPOLL_CLOEXEC)) < 0)
        return printlog(LOG_ERR, "epoll_create(2): %s", strerror(errno));

    sigemptyset(&mask);

    eventfds.signalfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (eventfds.signalfd < 0)
        return printlog(LOG_ERR, "signalfd(2): %s", strerror(errno));

    ev.events = EPOLLIN;
    ev.data.ptr = &dequeue_signal;
    if (epoll_ctl(eventfds.epfd, EPOLL_CTL_ADD, eventfds.signalfd, &ev) < 0)
        return printlog(LOG_ERR, "epoll_ctl(2): %s", strerror(errno));

#else
    struct kevent kev;

    if ((kqfd = kqueue()) < 0)
        return printlog(LOG_ERR, "kqueue(2): %s", strerror(errno));

    if (fcntl(kqfd, F_SETFD, FD_CLOEXEC) < 0)
                return printlog(LOG_ERR, "fcntl(2): %s", strerror(errno));

#endif

    return 0;
}

static int
register_signal_handlers(void)
{
    const struct signal_handler *sh;

    /* Special case: disable SA_RESTART on alarms */
    for (sh = &elopt.signal_handlers[0]; sh->signum; sh++) {
        if (sh->signum == SIGALRM) {
            struct sigaction sa;

            sa.sa_handler = sh->handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            if (sigaction(SIGALRM, &sa, NULL) < 0)
                return printlog(LOG_ERR, "sigaction(2): %s", strerror(errno));
        }
    }

#ifdef __linux__
    sigset_t mask;

    sigemptyset(&mask);
    for (sh = &elopt.signal_handlers[0]; sh->signum; sh++) {
        sigaddset(&mask, sh->signum);
    }
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        return printlog(LOG_ERR, "sigprocmask(2): %s", strerror(errno));

    eventfds.signalfd = signalfd(eventfds.signalfd, &mask, SFD_CLOEXEC);
    if (eventfds.signalfd < 0)
        return printlog(LOG_ERR, "signalfd(2): %s", strerror(errno));

#else
    struct kevent kev;

    for (sh = &signal_handlers[0]; sh->signum; sh++) {
        if (signal(sh->signum, (sh->signum == SIGCHLD ? SIG_DFL : SIG_IGN)) == SIG_ERR)
            return printlog(LOG_ERR, "signal(2): %d: %s", sh->signum, strerror(errno));

        EV_SET(&kev, sh->signum, EVFILT_SIGNAL, EV_ADD, 0, 0,
                (void *)&dequeue_signal);
        if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
                    return printlog(LOG_ERR, "kevent(2): %s", strerror(errno));

    }
#endif

    return 0;
}

int event_loop_init(struct event_loop_options opts)
{
    memcpy(&elopt, &opts, sizeof(elopt));

    if (create_event_queue() < 0)
        return printlog(LOG_ERR, "unable to create the event queue");

    if (register_signal_handlers() < 0)
        return printlog(LOG_ERR, "unable to register signal handlers");

    return 0;
}

int event_loop_register_callback(int fd, int (*func_ptr)(event_t *))
{
#ifdef __linux__
    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.ptr = func_ptr;
    if (epoll_ctl(eventfds.epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return printlog(LOG_ERR, "epoll_ctl(2): %s", strerror(errno));
#else
    struct kevent kev;

    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0,
            (void *)func_ptr);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        return printlog(LOG_ERR, "kqueue(2): %s", strerror(errno));
#endif
    return 0;
}

void
dispatch_event(void)
{
    void (*cb)(event_t *);
    event_t ev;
    int rv;

    for (;;) {
        printlog(LOG_DEBUG, "waiting for the next event");
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