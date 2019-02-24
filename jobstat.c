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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "database.h"
#include "job.h"
#include "logger.h"

static char *progname;

static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(EXIT_FAILURE);
}

void print_header(const char *str, const char *specifier) {
    char buf[512];
    sprintf((char *)buf, "%s%s%s", "\033[1m\033[4m", specifier, "\033[0m");
	printf(buf, str);
}

static int
renderer(void *unused, int cols, char **values, char **names)
{
	int i;
	static int print_headers = 1;
	const char *specifiers[] = {"%-4s", "%-18s", "%-9s", "%-8s", "%-10s", "%-8s"};

	//printf("%s",(char *)unused);
	(void)unused;

	if (print_headers) {
		for (i = 0; i < cols; i++) {
			print_header(names[i], specifiers[i]);
			if ((i + 1) < cols)
				printf(" ");
			else
				printf("\n");
		}
		print_headers = 0;
	}
	for (i = 0; i < cols; i++) {
		printf(specifiers[i], values[i] ? values[i] : "NULL");
		if ((i + 1) < cols)
			printf(" ");
		else
			printf("\n");
	}
	return (0);
}

int
print_all_jobs(void)
{
	int rv;
	char *sql = "SELECT Id, Label, State, Type, Terminated, Duration FROM job_table_view";
	char *err_msg = NULL;

	rv = sqlite3_exec(dbh, sql, renderer, "some stuff", &err_msg);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "Database error %d: %s", rv, err_msg);
		free(err_msg);
		return (-1);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int c;

    progname = basename(argv[0]);
    while ((c = getopt(argc, argv, "fhv")) != -1) {
        switch (c) {
            case 'f':
                break;
            case 'h':
                usage();
                break;
            case 'v':
                break;
            default:
                usage();
                break;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
    }

	if (logger_init() < 0)
		errx(1, "logger_init");
	logger_add_stderr_appender();
	
	if (db_init() < 0)
		errx(1, "logger_init");

	if (db_open(NULL, 0))
		errx(1, "db_open");

	if (print_all_jobs() < 0)
		exit(EXIT_FAILURE);
		
	exit(EXIT_SUCCESS);
}
