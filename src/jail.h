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

#include <limits.h>

#include "manifest.h"

/** The configuration for a jail */
struct jail_config {
	char *name;	/** The jailname, not to be confused with hostname */
	char *hostname; /** The hostname inside of the jail */
	char *package; /** Package to install in the jail; FIXME: should be an array */
	char *release; /** The release version of FreeBSD */
	char *machine; /** The machine name (from uname -m) */

	/* Internal variables, not to be exposed to the user */
	char base_txz_path[PATH_MAX]; /** Path to the base.txz used to install the jail */
	char *config_file; /** Path to the jail(8) configuration file */
	char *rootdir; /** The root of the jail filesystem heirarchy */
	int jid; /** The jail ID number */
};
typedef struct jail_config *jail_config_t;

int jail_opts_init();

struct jail_config * jail_config_new();
void jail_config_free(struct jail_config *jc);

int jail_config_set_name(jail_config_t jc, const char *name);
int jail_config_set_hostname(jail_config_t jc, const char *hostname);
int jail_config_set_machine(jail_config_t jc, const char *machine);
int jail_config_set_release(jail_config_t jc, const char *release);

int jail_create(jail_config_t jc);
bool jail_is_running(jail_config_t jc);
bool jail_is_installed(jail_config_t jc);
int jail_stop(jail_config_t jc);
int jail_destroy(jail_config_t jc);

/** Setup the jail when a job is loaded */
int jail_job_load(job_manifest_t manifest);

/** Stop the jail when a job is unloaded */
int jail_job_unload(job_manifest_t manifest);

int jail_parse_manifest(job_manifest_t manifest, const ucl_object_t *obj);
void jail_manifest_free(struct jail_config *jail_options);

#endif /* !_RELAUNCHD_JAIL_H_ */
