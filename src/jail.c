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

#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <sys/jail.h>
#include "/usr/include/jail.h"

#include "config.h"
#include "log.h"
#include "jail.h"
#include "util.h"

static void get_host_release(void);
static int fetch_distfiles(const char *machine, const char *release);
static void set_base_txz_path(char (*path)[PATH_MAX], const char *release, const char *machine);
static int bootstrap_pkg(const jail_config_t cfg);

/* Global options for jails */
static struct {
	bool initialized;
	char *jail_prefix;	/** Top level directory where jails live */

	/* Information about the jail host */
	char *host_release;
	char *host_machine;
} jail_opts = {
	false,
	NULL,
	NULL
};

static int fetch_distfiles(const char *machine, const char *release)
{
	char outfile[PATH_MAX];
	char *cmd = NULL, *uri = NULL;

	if (asprintf(&uri,
			"%s/pub/FreeBSD/releases/%s/%s/base.txz",
			"ftp://ftp.freebsd.org", /* TODO: make configurable */
			machine, release) < 0) {
		log_errno("asprintf(3)");
		goto err_out;
	}
	set_base_txz_path(&outfile, release, machine);
	log_debug("downloading %s", uri);

	if (asprintf(&cmd, "fetch -q %s -o %s",
			uri, outfile) < 0) {
		goto err_out;
	}
	if (system(cmd) < 0) {
		log_error("failed to fetch base.txz from %s", uri);
		goto err_out;
	}

	free(cmd);
	free(uri);
	return 0;

err_out:
	free(cmd);
	free(uri);
	return -1;
}

static void jail_opts_shutdown()
{
	free(jail_opts.jail_prefix);
}

int jail_opts_init()
{
	char path[PATH_MAX];

	atexit(jail_opts_shutdown);

	/* Create a cache directory for downloaded files */
	path_sprintf(&path, "/var/cache/launchd");
	mkdir_idempotent(path, 0755);

	if (asprintf(&jail_opts.jail_prefix, "%s", "/usr/launchd-jails") < 0)
		err(1, "asprintf(3)");

	if (access(jail_opts.jail_prefix, F_OK) < 0) {
		if (mkdir(jail_opts.jail_prefix, 0700) < 0) {
			log_errno("mkdir(2) of %s", jail_opts.jail_prefix);
			return -1;
		}
	}

	get_host_release();

	jail_opts.initialized = true;

	return 0;
}

jail_config_t jail_config_new()
{
	jail_config_t jc;

	if (!jail_opts.initialized && jail_opts_init() < 0)
		return NULL;

	jc = calloc(1, sizeof(*jc));
	return (jc);
}

int jail_config_set_name(jail_config_t jc, const char *name)
{
	char *p;

	if (jc->name != NULL) {
		log_error("name is already set");
		return -1;
	}
	if (strlen(name) >= MAXHOSTNAMELEN) {
		log_error("name too long");
		return -1;
	}
	for (p = (char *)name; *p != '\0'; p++) {
		if (isalnum(*p) || *p == '_' || *p == '-') {
			// valid character
		} else {
			log_error("invalid character `%c' in jail name", *p);
			return -1;
		}
	}

	jc->name = strdup(name);
	if (!jc->name) {
		log_errno("strdup(3)");
		return -1;
	}

	if (asprintf(&jc->config_file, "%s/%s.conf", jail_opts.jail_prefix, jc->name) < 0) {
		log_errno("asprintf(3)");
		return -1;
	}

	if (asprintf(&jc->rootdir, "%s/%s", jail_opts.jail_prefix, jc->name) < 0) {
		log_errno("asprintf(3)");
		return -1;
	}

	return 0;
}

int jail_config_set_hostname(jail_config_t jc, const char *hostname) {
	jc->hostname = strdup(hostname);
	if (!jc->hostname) {
		log_errno("strdup(3)");
		return -1;
	}
	return 0;
}


int jail_config_set_release(jail_config_t jc, const char *release) {
	jc->release = strdup(release);
	if (!jc->release) {
		log_errno("strdup(3)");
		return -1;
	}
	return 0;
}

int jail_config_set_machine(jail_config_t jc, const char *machine) {
	jc->machine = strdup(machine);
	if (!jc->machine) {
		log_errno("strdup(3)");
		return -1;
	}
	return 0;
}

void jail_config_free(jail_config_t jc)
{
	if (jc == NULL)
		return;
	free(jc->name);
	free(jc->hostname);
	free(jc->package);
	free(jc->config_file);
	free(jc->release);
	free(jc->machine);
	free(jc->rootdir);
	free(jc);
}

static int _jail_write_config(jail_config_t jc)
{
	int retval = -1;
	FILE *f;

	f = fopen(jc->config_file, "w");
	if (!f) {
		log_errno("fopen() of %s", jc->config_file);
		goto out;
	}

	if (fprintf(f,
			"# Automatically generated -- do not edit\n"
			"exec.start = \"/bin/sh /etc/rc\";\n"
			"exec.stop = \"/bin/sh /etc/rc.shutdown\";\n"
			"exec.clean;\n"
			"mount.devfs;\n"
			"\n"
			"%s {\n"
			"host.hostname = \"%s.local\";\n"
			"path = \"%s\";\n"
			"interface = \"lo0\";\n"
			"ip4.addr = 127.128.0.1;\n"
			"}\n",
			jc->name,
			jc->name,
			jc->rootdir) < 0) {
		log_errno("fprintf()");
		goto out;
	}

	if (fclose(f) <0 ) {
		log_errno("fclose");
		goto out;
	}

	retval = 0;

out:
	return retval;
}

int jail_create(jail_config_t jc)
{
	int retval = -1;
	char *cmd = NULL;

	set_base_txz_path(&(jc->base_txz_path), jc->release, jc->machine);
	if (access(jc->base_txz_path, F_OK) < 0) {
		if (fetch_distfiles(jc->machine, jc->release) < 0)
			return -1;
	}

	if (access(jc->rootdir, F_OK) == 0) {
		log_error("refusing to create jail; %s already exists", jc->rootdir);
		goto out;
	}

	if (mkdir(jc->rootdir, 0700) < 0) {
		log_errno("mkdir(2) of `%s", jc->rootdir);
		goto out;
	}

	if (asprintf(&cmd, "tar -xf %s -C %s", jc->base_txz_path, jc->rootdir) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}

	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	free(cmd);
	if (asprintf(&cmd, "cp /etc/resolv.conf /etc/localtime %s", jc->rootdir) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}

	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	/* TODO: set the hostname, which we don't know yet */

	if (_jail_write_config(jc) < 0) {
		log_error("unable to write the config file");
		goto out;
	}

	free(cmd);
	if (asprintf(&cmd, "jail -f %s -q -c", jc->config_file) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}

	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	if (bootstrap_pkg(jc) < 0) {
		log_error("unable to bootstrap pkg");
		goto out;
	}

	retval = 0;

out:
	free(cmd);
	return retval;
}

int jail_stop(jail_config_t jc)
{
		char *cmd = NULL;

		log_debug("stopping jail `%s'", jc->name);

		/* TODO: Capture the output of this and write to the logfile */
		if (asprintf(&cmd, "jail -f %s -r %s >/dev/null 2>&1", jc->config_file, jc->name) < 0) {
			log_errno("asprintf(3)");
			return -1;
		}

		if (system(cmd) < 0) {
			log_errno("command failed: %s", cmd);
			free(cmd);
			return -1;
		}

		free(cmd);
		return 0;
}

int jail_destroy(jail_config_t jc)
{
	int retval = -1;
	char *cmd = NULL;

	/* TODO: Capture the output of this and write to the logfile */
	if (asprintf(&cmd, "jail -f %s -r %s >/dev/null 2>&1", jc->config_file, jc->name) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}
	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	free(cmd);
	if (asprintf(&cmd, "chflags -R noschg %s", jc->rootdir) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}
	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	free(cmd);
	if (asprintf(&cmd, "rm -rf %s", jc->rootdir) < 0) {
		log_errno("asprintf(3)");
		goto out;
	}
	if (system(cmd) < 0) {
		log_error("command failed: %s", cmd);
		goto out;
	}

	if (unlink(jc->config_file) < 0) {
		log_error("unlink of %s", jc->config_file);
		goto out;
	}

	retval = 0;

out:
	free(cmd);
	return retval;
}

int jail_restart(struct jail_config *jc)
{
	char cmd[PATH_MAX];

	log_debug("restarting jail `%s'", jc->name);

	path_sprintf(&cmd, "jail -f %s -q -rc", jc->config_file);
	if (system(cmd) < 0) {
		log_errno("command failed: %s", cmd);
		return -1;
	}

	return 0;
}

bool jail_is_installed(jail_config_t jc)
{
	return (access(jc->rootdir, F_OK) == 0);
}

bool jail_is_running(jail_config_t jc)
{
	return (jail_getid(jc->name) >= 0);
}

int jail_job_load(job_manifest_t manifest)
{
	struct jail_config * const cfg = manifest->jail_options;

	if (!jail_is_installed(cfg)) {
		if (jail_create(cfg) < 0) {
			log_error("jail_create()");
			return -1;
		}
	}
	if (!jail_is_running(cfg)) {
		if (jail_restart(cfg) < 0) {
			log_error("jail_restart()");
			return -1;
		}
	}

	cfg->jid = jail_getid(cfg->name);

	return 0;
}

int jail_job_unload(job_manifest_t manifest)
{
	struct jail_config * const cfg = manifest->jail_options;

	if (jail_is_running(cfg)) {
		if (jail_stop(cfg) < 0) {
			log_error("jail_stop()");
			return -1;
		}
	}

	cfg->jid = -1;

	return 0;
}

int jail_parse_manifest(job_manifest_t manifest, const ucl_object_t *obj)
{
	ucl_object_iter_t it;
	const ucl_object_t *cur;
	struct jail_config * jconf;
	int result;

	jconf = calloc(1, sizeof(*jconf));
	if (!jconf) {
		log_errno("calloc(3)");
		return -1;
	}
	manifest->jail_options = jconf;

	it = ucl_object_iterate_new(obj);

	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		const char *key = ucl_object_key(cur);
		const char *val = ucl_object_tostring(cur);
		log_debug("key=%s val=%s", key, val);

		if (!strcmp(key, "Name")) {
			result = jail_config_set_name(jconf, val);
		} else if (!strcmp(key, "Hostname")) {
			result = jail_config_set_hostname(jconf, val);
		} else if (!strcmp(key, "Machine")) {
			result = jail_config_set_machine(jconf, val);
		} else if (!strcmp(key, "Release")) {
			result = jail_config_set_release(jconf, val);
		} else {
			log_error("Syntax error: unknown key: %s", key);
			ucl_object_iterate_free(it);
			return -1;
		}

		if (result < 0) {
			log_error("jail_parse_manifest()");
			return -1;
		}
	}

	ucl_object_iterate_free(it);

	return 0;
}

static void
get_host_release(void)
{
	struct utsname uts;
	char short_version[5];

	if (uname(&uts) < 0) {
		err(1, "uname(3)");
	}

	strncpy((char *)&short_version, uts.release, 4);
	short_version[4] = '\0';
	jail_opts.host_release = strdup((char *)&short_version);
	jail_opts.host_machine = strdup(uts.machine);
	if (!jail_opts.host_release || !jail_opts.host_machine)
		err(1, "memory allocation failed");
}

static void
set_base_txz_path(char (*path)[PATH_MAX], const char *release, const char *machine)
{
	path_sprintf(path, "%s/%s_%s_base.txz",
			"/var/cache/launchd", release, machine);
}

/* Inject pkg into the jail and bootstrap it */
static int bootstrap_pkg(const jail_config_t cfg)
{
	char buf[COMMAND_MAX];

	if (run_system(&buf, "cp /etc/resolv.conf %s/etc", cfg->rootdir) < 0) {
		log_error("unable to create resolv.conf");
		return -1;
	}

	if (run_system(&buf, "env ASSUME_ALWAYS_YES=YES pkg --chroot %s bootstrap -f", cfg->rootdir) < 0) {
		log_error("unable to fetch pkg");
		return -1;
	}

	return 0;
}
