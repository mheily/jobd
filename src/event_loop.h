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

#if HAVE_SYS_EVENT_H
#include <sys/event.h>
#endif
#if HAVE_SYS_EPOLL_H
#include <sys/epoll.h> 
#include <sys/signalfd.h>
/* Maximum signal number */
#define EVL_SIGNAL_MAX 32
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
	
	memset(&evp->filters, -1, sizeof(evp->filters));

	/* Setup signal handling */
	sigemptyset(&evp->sig_mask);
	evp->filters[EVL_FILT_SIGNAL] = signalfd(-1, &evp->sig_mask, 0);
	if (evp->filters[EVL_FILT_SIGNAL] < 0) {
		(void)close(evp->epfd);
		return -2;
	}
	memset(&evp->sig_udata, 0, sizeof(evp->sig_udata));
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

static inline int
evl_read(struct evl_proxy *evp, int fd, void *udata)
{
#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0)
		return -1;
#elif HAVE_SYS_EPOLL_H
	return -2;
#else
#error Unimplemented
#endif

        return 0;
}

static inline int
evl_signal(struct evl_proxy *evp, int signo, void *udata)
{
#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, signo, EVFILT_SIGNAL, EV_ADD, 0, 0, udata);
	if (kevent(evp->kqfd, &kev, 1, NULL, 0, NULL) < 0)
		return -1;
	if (signal(signo, SIG_IGN) == SIG_ERR)
		return -1;
#elif HAVE_SYS_EPOLL_H
	if (signo <= 0 || signo >= EVL_SIGNAL_MAX)
		return -1;
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

/* period: milliseconds */
static inline int
evl_timer_start(struct evl_proxy *evp, int period, int ident, void *udata)
{
#if HAVE_SYS_EVENT_H
	struct kevent kev;

	EV_SET(&kev, ident, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period, udata);
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

			sz = read(evp->filters[EVL_FILT_SIGNAL], &fdsi, sizeof(fdsi));
			if (sz != sizeof(fdsi))
				return -3;
			result->ident = fdsi.ssi_signo;
			result->udata = evp->sig_udata[result->ident];
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

#endif /* __EVENT_LOOP_H */
