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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../log.h"
#include "../jail.h"

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

int test_jail_config() {
	jail_config_t jc;

	jc = jail_config_new();
	if (jail_config_set_name(jc, "foo") < 0)
		fail("set name 1");
	if (jail_config_set_name(jc, "illegal name") == 0)
		fail("set name 2");
	jail_config_free(jc);

	return 0;
}

int test_jail_create_and_destroy() {
	jail_config_t jc;

	jc = jail_config_new();
	jail_config_set_name(jc, "thttpd");
	jc->package = strdup("thttpd");
	if (jail_create(jc) < 0)
		fail("jail_create");
	if (jail_destroy(jc) < 0)
		fail("jail_destroy");
	jail_config_free(jc);

	return 0;
}

int main()
{
	log_open("jail-test.log");
	run(test_jail_config);
	run(test_jail_create_and_destroy);
}
