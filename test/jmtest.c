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

int main() {
	job_manifest_t jm;

	if (getenv("DEBUG")) {
		logfile = stdout;
	} else {
		log_open("jmtest.log");
	}
	test_cvec();
	test_sockets();
	exit(0);

	//FIXME: refactor this
	jm = job_manifest_new();
	if (job_manifest_read(jm, "fixtures/test.json") == 0) {
printf("program arguments:\n");
		cvec_debug(jm->program_arguments);
		printf("environment:\n");
		cvec_debug(jm->environment_variables);
		printf("whee");
	} else {
		printf("failed");
		exit(1);
	}

}
