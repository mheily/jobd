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

#ifndef JOBD_JOB_TABLE_H
#define JOBD_JOB_TABLE_H

#include <sys/queue.h>
#include <sys/types.h>

enum terminfo {
    TERMINFO_NEVER_RAN, // has never ran
    TERMINFO_SIGNAL, // caught signal
    TERMINFO_EXIT, // called exit()
};

struct job_table_entry {
    struct job *jte_job;
    pid_t pid;
    struct {
        enum terminfo ti_event;
        int ti_data;
        time_t ti_timestamp;
    } terminfo;
    LIST_ENTRY(job_table_entry) jte_ent;
};

int job_table_init();
int job_table_insert(struct job *job);
#endif //JOBD_JOB_TABLE_H
