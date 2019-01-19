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
#include <errno.h>
#include <libgen.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "job.h"
#include "logger.h"
#include "parser.h"
#include "database.h"

static char *progname;

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-v] [-f path] import|init\n", progname);
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int c;
	char *command = NULL;
	char *f_flag = NULL;

	if (logger_init(NULL) < 0)
		errx(1, "unable to initialize logging");
	if (db_init() < 0)
		errx(1, "unable to initialize database functions");

    progname = basename(argv[0]);
  	while ((c = getopt (argc, argv, "f:hv")) != -1) {
		switch (c) {
			case 'f':
				f_flag = strdup(optarg);
				if (!f_flag)
					err(1, "strdup");
				break;
            case 'h':
                usage();
                break;
			case 'v': 
				logger_set_verbose(1);
				break;
			default:
			    usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	command = argv[0];
	if (!strcmp(command, "init")) {
		if (db_create(NULL, NULL) < 0)
			errx(1, "unable to create the database");
		exit(EXIT_SUCCESS);
	}

	if (db_open(NULL, 0) < 0)
		errx(1, "unable to open the database");

	if (!strcmp(command, "import")) {
		if (parser_import(f_flag ? f_flag : "/dev/stdin") < 0)
			exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
