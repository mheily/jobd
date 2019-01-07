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
#include <stdlib.h>

#include "config.h"
#include "database.h"
#include "job.h"
#include "logger.h"

static void
usage(void) 
{
	printf("todo\n");
}

void print_header(const char *str, const char *specifier)
{
	char buf[512];
	sprintf((char *) buf, "%s%s%s", "\033[1m\033[4m", specifier, "\033[0m");
	printf(buf, str);
}

static int
renderer(void *unused, int cols, char **values, char **names)
{
	int i;
	static int print_headers = 1;
	const char *specifiers[] = {"%-24s", "%-16s", "%-16s"};

	//printf("%s",(char *)unused);
	(void) unused;

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
print_all_properties(void)
{
	int rv;
	char *sql = "SELECT job_name AS Job, name AS Property, value AS Value FROM properties_view ORDER BY job_name";
	char *err_msg = NULL;

	rv = sqlite3_exec(dbh, sql, renderer, "some stuff", &err_msg);
	if (rv != SQLITE_OK) {
		printlog(LOG_ERR, "Database error: %s", err_msg);
		free(err_msg);
		return (-1);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int c;
	int a_flag = 0;

	while ((c = getopt(argc, argv, "a")) != -1) {
		switch (c) {
		case 'a':
				a_flag = 1;
				break;
		default:
				fputs("unrecognized command option", stderr);
				usage();
				exit(EXIT_FAILURE);
				break;
		}
	}

	if (logger_init(NULL) < 0)
		errx(1, "logger_init");
	
	if (db_init() < 0)
		errx(1, "logger_init");

	if (db_open(NULL, DB_OPEN_WITH_VIEWS))
		errx(1, "db_open");

	if (a_flag) {
		if (print_all_properties() < 0)
			exit(EXIT_FAILURE);
		else
			exit(EXIT_SUCCESS);
	}


	errx(1, "TODO -- implement me");

	exit(EXIT_SUCCESS);
}
