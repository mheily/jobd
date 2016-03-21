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

#ifndef __EVENT_LOOP_H
#define __EVENT_LOOP_H

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../config.h" //WORKAROUND

/* The maximum number of kernel timers that can be created */
#define EVL_TIMER_MAX 32

/* Maximum signal number; see <signal.h> to verify this is high enough. */
#define EVL_SIGNAL_MAX 32

#if HAVE_SYS_EVENT_H
#include <sys/event.h>
#endif
#if HAVE_SYS_EPOLL_H
#include <sys/epoll.h> 
#include <sys/signalfd.h>
#endif

typedef enum {
#if HAVE_SYS_EVENT_H
	EVL_FILT_READ = EVFILT_READ,
	EVL_FILT_WRITE = EVFILT_WRITE,
	EVL_FILT_SIGNAL = EVFILT_SIGNAL,
	EVL_FILT_TIMER = EVFILT_TIMER,
#else
	EVL_FILT_READ = 0,
	EVL_FILT_WRITE = 1,
	EVL_FILT_SIGNAL = 2,
	EVL_FILT_TIMER = 3,
	EVL_FILT_MAX = 10,	/* Unused, and padded for future growth */
#endif
} evl_filter_t;

struct evl_proxy {
#if HAVE_SYS_EVENT_H
        int kqfd;
#elif HAVE_SYS_EPOLL_H
        int epfd;
	int filters[EVL_FILT_MAX];
 	sigset_t sig_mask; /* The current mask of sig_fd */
        void *sig_udata[EVL_SIGNAL_MAX]; /* Map between signal and udata */
        void *timer_udata[EVL_TIMER_MAX]; /* Map between timer ident and udata */
#else
#error Unimplemented
#endif
};

struct evl_event {
	evl_filter_t filter;
	int ident;
	void *udata;
};

static inline int
evl_proxy_init(struct evl_proxy *evp)
{
#if HAVE_SYS_EVENT_H
        evp->kqfd = kqueue();
        if (evp->kqfd < 0)
                return -1;

#elif HAVE_SYS_EPOLL_H
        evp->epfd = epoll_create(10);
        if (evp->epfd < 0)
                return -1;
	
        /* Clear arrays */
	memset(&evp->filters, -1, sizeof(evp->filters));
	memset(&evp->timer_udata, 0, sizeof(evp->timer_udata));
	memset(&evp->sig_udata, 0, sizeof(evp->sig_udata));

	/* Setup signal handling */
	sigemptyset(&evp->sig_mask);
	evp->filters[EVL_FILT_SIGNAL] = signalfd(-1, &evp->sig_mask, 0);
	if (evp->filters[EVL_FILT_SIGNAL] < 0)
		return -2;


	/* Setup EVFILT_READ and EVFILT_WRITE */
	evp->filters[EVL_FILT_READ] = epoll_create(10);
	evp->filters[EVL_FILT_WRITE] = epoll_create(10);
	if (evp->filters[EVL_FILT_READ] < 0 ||
			evp->filters[EVL_FILT_READ] < 0
			) {
		return -3;
	}

	/* FIXME: add all the filters to the epollfd */

	/* TODO: error handling that unwinds and close()'s things */
#else
#error Unimplemented
#endif
        return 0;
}

static inline int
evl_proxy_descriptor(struct evl_proxy *evp)
{
#if HAVE_SYS_EVENT_H
        return evp->kqfd;
#elif HAVE_SYS_EPOLL_H
        return evp->epfd;
#else
#error Unimplemented
        return -1;
#endif
}

/* FIXME: should be renamed to *_socket() because linux can't handle regular files */
static inline int
evl_watch_fd(struct evl_proxy *evp, evl_filter_t filter, int fd, void *udata)
{
	if (filter != EVL_FILT_READ && filter != EVL_FILT_WRITE)
		return -3;

#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, fd, filter, EV_ADD, 0, 0, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0)
		return -1;

#elif HAVE_SYS_EPOLL_H
	struct epoll_event ev;

	if (filter == EVL_FILT_READ)
		ev.data.events = EPOLLIN | EPOLLRDHUP;
	else
		ev.data.events = EPOLLOUT;

	ev.data.ptr = udata;
	/* FIXME: linux can't return both fd AND udata, but kqueue can */
	// maybe malloc() a struct, then free() when copying out?
	// requires ONESHOT behavior as standard

	if (epoll_ctl(evp->filters[filter], EPOLL_CTL_ADD, fd, &event) < 0)
		return -3;

	return -2;
#else
#error Unimplemented
#endif

        return 0;
}

static inline int
evl_signal(struct evl_proxy *evp, int signo, void *udata)
{
	if (signo <= 0 || signo >= EVL_SIGNAL_MAX)
		return -1;

#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, signo, EVFILT_SIGNAL, EV_ADD, 0, 0, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0)
		return -1;
	if (signal(signo, SIG_IGN) == SIG_ERR)
		return -1;

#elif HAVE_SYS_EPOLL_H
	sigaddset(&evp->sig_mask, signo);
	if (signalfd(evp->filters[EVL_FILT_SIGNAL], &evp->sig_mask, 0) < 0) {
		return -2;
	}
	evp->sig_udata[signo] = udata;

#else
#error Unimplemented
	return -1;
#endif

        return 0;
}

//TODO: get rid of this.. better to delete the timer, or make it ONESHOT
static inline int
evl_timer_stop(struct evl_proxy *evp, int ident, void *udata)
{
#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, ident, EVFILT_TIMER, EV_ADD | EV_DISABLE, 0, 0, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0) {
		//log_errno("kevent(2)");
		return -1;
	}
#elif HAVE_SYS_EPOLL_H
	return -2;
#else
#error Unimplemented
#endif

        return 0;
}

/* Convert milliseconds into seconds+nanoseconds */
static inline void
convert_msec_to_itimerspec(struct itimerspec *dst, int src, int oneshot)
{
    time_t sec, nsec;

    sec = src / 1000;
    nsec = (src % 1000) * 1000000;

    /* Set the interval */
    if (oneshot) {
        dst->it_interval.tv_sec = 0;
        dst->it_interval.tv_nsec = 0;
    } else {
        dst->it_interval.tv_sec = sec;
        dst->it_interval.tv_nsec = nsec;
    }

    /* Set the initial expiration */
    dst->it_value.tv_sec = sec;
    dst->it_value.tv_nsec = nsec;
}

/* period: milliseconds */
static inline int
evl_timer_start(struct evl_proxy *evp, int period, int ident, void *udata)
{
	if (ident < 0 || ident >= EVL_TIMER_MAX)
		return -3;

#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0) {
		//log_errno("kevent(2)");
		return -1;
	}
#elif HAVE_SYS_EPOLL_H
	struct epoll_event ev;
	struct itimerspec ts;
	int tfd;

	tfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (tfd < 0)
		return -1;

	convert_msec_to_itimerspec(&ts, period, 0); //TODO: oneshot support
	if (timerfd_settime(tfd, 0, &ts, NULL) < 0) {
		close(tfd);
		return -2;
	}

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = udata;
	if (epoll_ctl(filter_epfd(filt), EPOLL_CTL_ADD, tfd, &ev) < 0) {
		close(tfd);
		return -3;
	}

#else
#error Unimplemented
	return -1;
#endif

        return 0;
}

static inline int
evl_wait(struct evl_event *result, const struct evl_proxy *evp)
{
#if HAVE_SYS_EVENT_H
	struct kevent kev;

	for (;;) {
		if (kevent(evp->kqfd, NULL, 0, &kev, 1, NULL) < 1) {
			if (errno == EINTR) {
				continue;
			} else {
				//log_errno("kevent");
				return -1;
			}
		}
	}

	result->udata = (void *)kev.udata;
	result->ident = kev.ident;
	result->filter = kev.filter;

#elif HAVE_SYS_EPOLL_H
	struct epoll_event ev;

	for (;;) {
		if (epoll_wait(evp->epfd, &ev, 1, -1) < 1) {
			if (errno == EINTR) {
				continue;
			} else {
				//log_errno("epoll_wait");
				return -1;
			}
		}
	}

	result->filter = ev.data.fd;

	switch (result->filter) {
		case EVL_FILT_SIGNAL:
			{
			struct signalfd_siginfo fdsi;
			ssize_t sz;

			//FIXME: handle eintr
			sz = read(evp->filters[EVL_FILT_SIGNAL], &fdsi, sizeof(fdsi));
			if (sz != sizeof(fdsi))
				return -3;
			result->ident = fdsi.ssi_signo;
			result->udata = evp->sig_udata[result->ident];
			}
			break;

		case EVL_FILT_READ:
		case EVL_FILT_WRITE:
			{
			struct epoll_event ev;
			ssize_t sz;

			//FIXME: handle eintr
			sz = read(evp->filters[result->filter], &ev, sizeof(ev));
			if (sz != sizeof(ev))
				return -4;

#if 0
			//TODO: return error conditions
			if (ev->events & EPOLLRDHUP || ev->events & EPOLLHUP)
			        dst->flags |= EV_EOF;
			if (ev->events & EPOLLERR)
			        dst->fflags = 1; /* FIXME: Return the actual socket error */
#endif
			}

			//FIXME: hard to get this: result->ident = ???
			result->udata = ev.data.ptr;
			break;

		case EVL_FILT_TIMER:
			{
				/* TODO: read timerfd */
				return -5;
			}
			break;

		default:
			return -2;
	}
#else
#error Unimplemented
	return -1;
#endif

        return 0;
}

/* TODO: function to delete kevents by looking up their udata, since Linux
 * only stores udata and not ident */

#endif /* __EVENT_LOOP_H */
