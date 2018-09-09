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

static void
usage(void) 
{
	printf("todo\n");
}

int
main(int argc, char *argv[])
{
	char *command = argv[1];
	int rv;

	if (logger_init() < 0)
		errx(1, "logger_init");

	if (db_init() < 0)
		errx(1, "unable to initialize the database routines");

	if (db_open(NULL, false) < 0)
		errx(1, "unable to open the database");

	if (ipc_init(NULL) < 0)
		errx(1, "ipc_init");

	if (ipc_connect() < 0)
		errx(1, "ipc_connect");

	(void) argc;
	if (!command)
		errx(1, "command expected");

	if (!strcmp(command, "start")) {
		rv = ipc_client_request(IPC_REQUEST_START, argv[2]);
	} else if (!strcmp(command, "stop")) {
		rv = ipc_client_request(IPC_REQUEST_STOP, argv[2]);
	} else if (!strcmp(command, "status")) {
		rv = ipc_client_request(IPC_REQUEST_STATUS, NULL);
	} else if (!strcmp(command, "restart")) {
		ipc_client_request(IPC_REQUEST_STOP, argv[2]);//ERRCHECK
		rv = ipc_client_request(IPC_REQUEST_START, argv[2]);
	} else if (!strcmp(command, "help")) {
		usage();
		rv = IPC_RESPONSE_OK;
	} else {
		printlog(LOG_ERR, "unrecognized command: %s", command);
		usage();
		exit(EXIT_FAILURE);
	}

	if (rv != IPC_RESPONSE_OK) {
		fprintf(stderr, "ERROR: Request failed with retcode %d\n", rv);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}