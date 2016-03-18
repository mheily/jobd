/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
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
#include <sys/wait.h>

#include "log.h"
#include "manifest.h"
#include "job.h"
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

int test_simple() {
	job_manifest_t jm;
	job_t job;
	int result;

	jm = job_manifest_new();
	if (job_manifest_read(jm, "job.json") < 0)
		return -1;

	job = job_new(jm);
	if (!job) return -1;

	if (job_load(job) < 0)
		return -1;

	if (job_run(job) < 0)
		return -1;

	result = system("grep -q 'hello world' /tmp/jobtest.out");
	if (WEXITSTATUS(result) != 0) {
		printf("job output not correct; exit status %d\n", result);
		return -1;
	}

	return 0;
}

int main() {
	test_run(test_simple);
}
