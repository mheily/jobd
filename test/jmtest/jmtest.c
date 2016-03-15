/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
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

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "manifest.h"
#include "cvec.h"

#define test_run(test) do { \
	printf("%-24s", ""#test); \
	if (test() != 0) { \
		puts("FAILED"); \
		exit(1); \
	} else { \
		puts("passed"); \
	} \
} while(0)

void test_cvec() {
	cvec_t cv;
	int i;

	cv = cvec_new();
	cvec_push(cv, strdup("hello"));
	cvec_push(cv, strdup("world"));
	for (i = 0; i < cv->length; i++) {
		printf("%d = %s\n", i, cv->items[i]);
	}
	cvec_free(cv);
	printf("cvec ok\n");
}

void test_sockets() {
	job_manifest_t jm;

	jm = job_manifest_new();
	printf("testing sockets: ");
	fflush(stdout);
	if (job_manifest_read(jm, "fixtures/sockets.json") == 0) {
		puts("ok");
	} else {
		puts("FAILED");
		exit(1);
	}
}

/* This used to be a more complex function named get_file_extension()
 * but has been replaced with strrchr().
 */
char * get_file_extension(const char *filename);
int test_get_file_extension() {
	int retval = 0;
	char *res;

	res = strrchr("foo.json", '.');
	if (strcmp(res, ".json") != 0) { printf("fail 1: %s", res); retval = -1; }

	res = strrchr("foo", '.');
	if (res)  { puts("fail 2"); retval = -1; }

	res = strrchr("foo.", '.');
	if (strcmp(res, ".") != 0)  { puts("fail 3"); retval = -1; }

	return retval;
}

int test_start_interval() {
	job_manifest_t jm;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/start_interval.json") < 0)
		return -1;

	return 0;
}

int test_start_calendar_interval() {
	job_manifest_t jm;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/start_calendar_interval.json") < 0)
		return -1;

	return 0;
}

int test_env() {
	job_manifest_t jm;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/env.json") < 0) return -1;
	return 0;
}

int test_env2() {
	job_manifest_t jm;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/env2.json") < 0) return -1;
	return 0;
}

int test_umask() {
	job_manifest_t jm;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/test.json") < 0) return -1;
	if (jm->umask != 63) return -1;
	return 0;
}

int main() {
	job_manifest_t jm;

	test_run(test_env);
	test_run(test_env2);
	test_run(test_start_interval);
	test_run(test_start_calendar_interval);
	test_run(test_get_file_extension);
	test_run(test_umask);
	test_cvec();
	test_sockets();
}
