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

#ifndef _RELAUNCHD_JAIL_H_
#define _RELAUNCHD_JAIL_H_

/** The configuration for a jail */
struct jail_config {
	char *name;	/** The jailname, not to be confused with hostname */
	char *package; /** Package to install in the jail; FIXME: should be an array */

	/* Internal variables, not to be exposed to the user */
	char *config_file; /** Path to the jail(8) configuration file */
	char *rootdir; /** The root of the jail filesystem heirarchy */
};
typedef struct jail_config *jail_config_t;

int jail_opts_init();

jail_config_t jail_config_new();
void jail_config_free(jail_config_t jc);

int jail_config_set_name(jail_config_t jc, const char *name);

int jail_create(jail_config_t jc);
bool jail_is_running(jail_config_t jc);
bool jail_is_installed(jail_config_t jc);
int jail_destroy(jail_config_t jc);

#endif /* !_RELAUNCHD_JAIL_H_ */
