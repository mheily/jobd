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

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"
#include "database.h"
#include "logger.h"
#include "ipc.h"

static char *progname;

static void
usage(void)
{
    fprintf(stderr, "usage: %s job method\n", progname);
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    char *job_id, *command;
    int c, rv;

    progname = basename(argv[0]);
    while ((c = getopt(argc, argv, "h")) != -1) {
        switch (c) {
            case 'h':
                usage();
                break;
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 0) {
        usage();
    }

    if (logger_init(NULL) < 0)
        errx(1, "logger_init");

    if (ipc_init(NULL) < 0)
        errx(1, "ipc_init");

    if (ipc_connect() < 0)
        errx(1, "ipc_connect");

    job_id = argv[0];
    command = argv[1];

    rv = ipc_client_request(job_id, command);
    if (rv != IPC_RESPONSE_OK) {
        fprintf(stderr, "ERROR: Request failed with retcode %d\n", rv);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
