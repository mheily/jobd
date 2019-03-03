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

#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#ifdef __linux__
typedef struct epoll_event event_t;
#else
typedef struct kevent event_t;
#endif

struct signal_handler {
    int signum;
    void (*handler)(int);
};

struct event_loop_options {
    int daemon;
    const struct signal_handler *signal_handlers;
};

void dispatch_event(void);
int event_loop_init(struct event_loop_options elopt);
int event_loop_register_callback(int fd, int (*func_ptr)(event_t *));

#endif /* _EVENT_LOOP_H */
