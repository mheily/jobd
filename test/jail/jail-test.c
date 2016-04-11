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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "../../src/log.h"
#include "../../src/jail.h"
#include "../../src/package.h"

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

static int test_jail_init() {
	assert(jail_opts_init() == 0);
	return 0;
}

static int test_jail_config()
{
	jail_config_t jco = NULL;

	jco = jail_config_new();
	assert(jco);
	assert(jail_config_set_name(jco, "test_jailname") == 0);
	assert(jail_config_set_release(jco, "10.3-RELEASE") == 0);
	assert(jail_config_set_machine(jco, "amd64") == 0);
	if (jail_is_installed(jco)) {
		assert(jail_destroy(jco) == 0);
	}
	assert(jail_is_installed(jco) == false);
	assert(jail_is_running(jco) == false);
	return 0;
}

static int test_jail_create_and_destroy()
{
	jail_config_t jco = NULL;

	jco = jail_config_new();
	jail_config_set_name(jco, "test_jailname");
	jail_config_set_release(jco, "10.3-RELEASE");
	jail_config_set_machine(jco, "amd64");
	assert(jail_create(jco) == 0);
	assert(jail_is_installed(jco) == true);
	assert(jail_is_running(jco) == true);
	assert(jail_destroy(jco) == 0);
	assert(jail_is_installed(jco) == false);
	assert(jail_is_running(jco) == false);

	return 0;
}

static int test_jail_manifest() {
		job_manifest_t jm;
		int rv;

		jm = job_manifest_new();
		rv = job_manifest_read(jm, "jail-test.json");
		assert (rv == 0);
		job_manifest_free(jm);
		return 0;
}

static int test_jail_load_and_unload() {
		job_manifest_t jm;
		int rv;

		jm = job_manifest_new();
		rv = job_manifest_read(jm, "jail-test.json");
		assert (jail_job_load(jm) == 0);
		assert(package_install(jm) == 0);
		assert (jail_job_unload(jm) == 0);
		jail_destroy(jm->jail_options);
		job_manifest_free(jm);
		return 0;
}

int main()
{
	log_freopen(stdout);
	run(test_jail_init);
	run(test_jail_manifest);
	run(test_jail_config);
	run(test_jail_create_and_destroy);
	run(test_jail_load_and_unload);
}
