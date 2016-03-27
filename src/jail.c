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

/* Global options for jails */
static struct {
	bool initialized;
	char *base_txz_path;
	char *jail_prefix;	/** Top level directory where jails live */
} jail_opts = {
	false,
	NULL,
	NULL
};

static int fetch_distfiles()
{
	struct utsname uts;
	char short_version[5];
	char *cmd = NULL, *uri = NULL;

	if (uname(&uts) < 0) {
		log_errno("uname(3)");
		return -1;
	}

	strncpy((char *)&short_version, uts.release, 4);
	short_version[4] = '\0';

	if (asprintf(&uri,
			"%s/pub/FreeBSD/releases/%s/%s/%s-RELEASE/base.txz",
			"ftp://ftp.freebsd.org", /* TODO: make configurable */
			uts.machine, uts.machine, short_version) < 0) {
		log_errno("asprintf(3)");
		goto err_out;
	}
	log_debug("downloading %s", uri);

	if (asprintf(&cmd, "fetch -q %s -o %s", uri, jail_opts.base_txz_path) < 0) {
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
	free(jail_opts.base_txz_path);
	free(jail_opts.jail_prefix);
}

int jail_opts_init()
{
	atexit(jail_opts_shutdown);

	if (asprintf(&jail_opts.base_txz_path, "%s/base.txz", CACHEDIR) < 0)
		err(1, "asprintf(3)");
	if (asprintf(&jail_opts.jail_prefix, "%s", "/usr/launchd-jails") < 0)
		err(1, "asprintf(3)");

	if (access(jail_opts.jail_prefix, F_OK) < 0) {
		if (mkdir(jail_opts.jail_prefix, 0700) < 0) {
			log_errno("mkdir(2)");
			return -1;
		}
	}

	if (access(jail_opts.base_txz_path, F_OK) < 0) {
		if (fetch_distfiles() < 0)
			return -1;
	}

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
		if (isalnum(*p) || *p == '_') {
			// valid character
		} else {
			log_error("invalid character in jail name");
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

void jail_config_free(jail_config_t jc)
{
	if (jc == NULL)
		return;
	free(jc->name);
	free(jc->package);
	free(jc->config_file);
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

	if (access(jc->rootdir, F_OK) == 0) {
		log_error("refusing to create jail; %s already exists", jc->rootdir);
		goto out;
	}

	if (mkdir(jc->rootdir, 0700) < 0) {
		log_errno("mkdir(2)");
		goto out;
	}

	if (asprintf(&cmd, "tar -xf %s -C %s", jail_opts.base_txz_path, jc->rootdir) < 0) {
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

	retval = 0;

out:
	free(cmd);
	return retval;
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

bool jail_is_installed(jail_config_t jc)
{
	return (access(jc->rootdir, F_OK) == 0);
}

bool jail_is_running(jail_config_t jc)
{
	return (jail_getid(jc->name) >= 0);
}
