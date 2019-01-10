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
static int H_flag;

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-aH]\n", progname);
    fprintf(stderr, "         or\n");
    fprintf(stderr, "       %s job property[=value]\n", progname);
    exit(EXIT_FAILURE);
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

	(void) unused;

	if (print_headers && !H_flag) {
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

    progname = basename(argv[0]);
    while ((c = getopt(argc, argv, "aHh")) != -1) {
        switch (c) {
            case 'a':
                a_flag = 1;
                break;
            case 'H':
                H_flag = 1;
                break;
            case 'h':
                usage();
                break;
            default:
                usage();
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0 || argc > 1) {
        usage();
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

    /* Determine if we are getting or setting a property */
    char *delim = strchr(argv[0], '=');
    char *key = argv[0];
    char *val;
    if (delim) {
        *delim = '\0';
        val = delim + 1;
    } else {
        val = NULL;
    }

    /* Parse the label and property name */
    char *label = key;
    char *property;
    delim = strrchr(key, '.');
    if (delim) {
        *delim = '\0';
        property = delim + 1;
    } else {
        errx(1, "invalid property name");
    }

    /* Lookup the job ID */
    char *result = NULL;
    int64_t jid;
    if (job_get_id(&jid, label) < 0)
        errx(1, "database lookup error");
    if (!jid)
        errx(1, "job not found: %s", label);

    /* Get or set the value of the property */
    if (val) {
        // FIXME - should probably use IPC to have jobd make the actual change
        // or notify jobd after we make the change.
        if (job_set_property(jid, property, val) < 0)
            errx(1, "error setting property");
    } else {
        if (job_get_property(&result, property, jid) < 0)
            errx(1, "error getting property");
        if (!result)
            errx(1, "property does not exist");
        puts(result);
    }

    exit(EXIT_SUCCESS);
}
