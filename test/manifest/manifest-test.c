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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "../../src/log.h"
#include "../../src/manifest.h"

//FIXME: copypaste from jmtest.c
#define run(test) do { \
	printf("%-24s", ""#test); \
	if (test() != 0) { \
		puts("FAILED"); \
		exit(1); \
	} else { \
		puts("passed"); \
	} \
} while(0)

#define fail(_message) do { \
	printf("FAIL: %s\n", _message); \
	return -1; \
} while (0)

static int test_allocator()
{
	job_manifest_t m = NULL;

	m = job_manifest_new();
	assert(m);
	assert(!m->label);
	assert(!m->user_name);
	assert(!m->group_name);
	assert(!m->program);
//	assert(m->program_arguments);
//	assert(cvec_length(m->program_arguments) == 0);
	assert(!m->working_directory);
	assert(!m->root_directory);
	assert(!m->jail_name);
//	assert(m->environment_variables);
//	assert(cvec_length(m->environment_variables) == 0);
//	assert(m->watch_paths);
//	assert(cvec_length(m->watch_paths) == 0);
//	assert(m->queue_directories);
//	assert(cvec_length(m->queue_directories) == 0);
 	assert(!m->stdin_path);
 	assert(!m->stdout_path);
 	assert(!m->stderr_path);
	assert(m->refcount == 0);
        assert(m->exit_timeout == 20);
        assert(m->throttle_interval == 10);
        assert(m->init_groups);
	job_manifest_free(m);
	return 0;
}

static int test_parse_label()
{
	job_manifest_t m;
	char *good_label = strdup(
		"{\n"
		" \"Label\": \"hello.world\",\n"
		" \"Program\": \"/bin/sh\"\n"
		"}\n");

	m = job_manifest_new();
	assert(job_manifest_parse(m, (u_char *)good_label, strlen(good_label) + 1) == 0);
	job_manifest_free(m);

#if TODO
	char *bad_label = strdup(
		"{\n"
		" \"Label\": \"do not want spaces\",\n"
		" \"Program\": \"/bin/sh\"\n"
		"}\n");
	m = job_manifest_new();
	assert(job_manifest_parse(m, bad_label, strlen(bad_label) + 1) != 0);
	job_manifest_free(m);
	//FIXME: this causes a crash: free(bad_label);
#endif

	return 0;
}

/** Ensure that either Program or ProgramArguments is required */
static int test_parse_program()
{
	job_manifest_t m;
	char *manifest = strdup(
		"{\n"
		" \"Label\": \"hello.world\"\n"
		"}\n");
	m = job_manifest_new();
	assert(job_manifest_parse(m, (u_char *)manifest, strlen(manifest) + 1) != 0);
	job_manifest_free(m);

	manifest = strdup("{\"Label\": \"a\", \"Program\": \"/bin/true\"}");
	m = job_manifest_new();
	assert(job_manifest_parse(m, (u_char *)manifest, strlen(manifest) + 1) == 0);
	job_manifest_free(m);

	manifest = strdup("{\"Label\": \"a\", \"ProgramArguments\": [\"/bin/true\"]}");
	m = job_manifest_new();
	assert(job_manifest_parse(m, (u_char *)manifest, strlen(manifest) + 1) == 0);
	job_manifest_free(m);

	manifest = strdup("{\"Label\": \"a\", \"Program\": \"/a\","
 		"\"ProgramArguments\": [\"/b\"]}");
	m = job_manifest_new();
	assert(job_manifest_parse(m, (u_char *)manifest, strlen(manifest) + 1) == 0);
	assert(strcmp(m->program, "/a") == 0);
	assert(cvec_length(m->program_arguments) == 2);
	assert(!strcmp(cvec_get(m->program_arguments, 0), "/a"));
	assert(!strcmp(cvec_get(m->program_arguments, 1), "/b"));
	job_manifest_free(m);

	return 0;
}

int main()
{
	run(test_allocator);
	run(test_parse_label);
	run(test_parse_program);
}
