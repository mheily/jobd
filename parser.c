/*
 * Copyright (c) 2018 Mark Heily <mark@heily.com>
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

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "logger.h"
#include "toml.h"
#include "array.h"
#include "job.h"

static int
parse_bool(bool *result, toml_table_t *tab, const char *key, bool default_value)
{
	const char *raw;
	int rv;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtob(raw, &rv)) {
		*result = rv;
		return (0);	
	} else {
 		return (-1);
	}
}

static int
parse_string(char **result, toml_table_t *tab, const char *key, const char *default_value)
{
	const char *raw;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		if (default_value) {
			*result = strdup(default_value);
			if (!*result) {
				printlog(LOG_ERR, "strdup(3): %s", strerror(errno));
				goto err;
			}
			return (0);
		} else {
			printlog(LOG_ERR, "no value provided for %s and no default", key);
			goto err;
		}
	}
	
	if (toml_rtos(raw, result)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = NULL;
		return (-1);
	}

	return (0);

err:
	*result = NULL;
	return (-1);
}

static int
parse_gid(gid_t *result, toml_table_t *tab, const char *key, const gid_t default_value)
{
	struct group *grp;
	const char *raw;
	char *buf;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtos(raw, &buf)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = -1;
		return (-1);
	}

	grp = getgrnam(buf);
	if (grp) {
		*result = grp->gr_gid;
		free(buf);
		return (0);
	} else {
		printlog(LOG_ERR, "group not found: %s", buf);
		free(buf);
		return (-1);
	}
}

static int
parse_uid(uid_t *result, toml_table_t *tab, const char *key, const uid_t default_value)
{
	struct passwd *pwd;
	const char *raw;
	char *buf;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
		return (0);
	}
	
	if (toml_rtos(raw, &buf)) {
		printlog(LOG_ERR, "invalid value for %s", key);
		*result = -1;
		return (-1);
	}

	pwd = getpwnam(buf);
	if (pwd) {
		*result = pwd->pw_uid;
		free(buf);
		return (0);
	} else {
		printlog(LOG_ERR, "user not found: %s", buf);
		free(buf);
		return (-1);
	}
}

static int
uid_to_name(char **name, uid_t uid)
{
	struct passwd *pwd;

	pwd = getpwuid(uid);
	if (pwd) {
		*name = strdup(pwd->pw_name);
		if (*name == NULL) {
			printlog(LOG_ERR, "strdup(3): %s", strerror(errno));
			return (-1);	
		} else {
			return (0);
		}
	} else {
		printlog(LOG_ERR, "user not found for uid %d", uid);
		return (-1);
	}
}

static int
parse_program(struct job *j, toml_table_t *tab)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	size_t nelem;
	int i;

	arr = toml_array_in(tab, "Program");
	if (!arr) {
		return (0);
	}

	for (nelem = 0; (raw = toml_raw_at(arr, nelem)) != 0; nelem++) {}

	j->argv = calloc(nelem + 1, sizeof(char *));
	if (!j->argv) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}

	raw = toml_raw_at(arr, 0);
	if (!raw) {
		printlog(LOG_ERR, "empty Program array");
		return (-1);
	}
	if (toml_rtos(raw, &j->argv[0])) {
		printlog(LOG_ERR, "parse error parsing Program element 0");
	}
	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing Program element %d", i);
			return (-1);
		} else {
			j->argv[i] = val;
		}
	}
	j->argv[i] = NULL;

	return (0);
}

//TODO: refactor parse_program to use the func below
static int
parse_array_of_strings(char ***result, toml_table_t *tab, const char *top_key)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	size_t nelem;
	int i;
	char **strarr;

	arr = toml_array_in(tab, top_key);
	if (!arr) {
		*result = calloc(1, sizeof(char *));
		if (*result) {
			return (0);
		} else {
			printlog(LOG_ERR, "calloc: %s", strerror(errno));
			return (-1);
		}
	}

	for (nelem = 0; (raw = toml_raw_at(arr, nelem)) != 0; nelem++) {}

	strarr = calloc(nelem + 1, sizeof(char *));
	if (!strarr) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}

	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s element %d", top_key, i);
			//FIXME: free strarr
			return (-1);
		} else {
			strarr[i] = val;
		}
	}

	*result = strarr;

	return (0);
}

static int
parse_environment_variables(struct job *j, toml_table_t *tab)
{
	toml_table_t* subtab;
	const char *key;
	char *val, *keyval;
	const char *raw;
	size_t nkeys;
	int i;

	subtab = toml_table_in(tab, "EnvironmentVariables");
	if (!subtab) {
		j->environment_variables = malloc(sizeof(char *));
		j->environment_variables[0] = NULL;
		return (0);
	}

	for (nkeys = 0; (key = toml_key_in(subtab, nkeys)) != 0; nkeys++) {}

	j->environment_variables = calloc(nkeys + 1, sizeof(char *));
	if (!j->environment_variables) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}
		
	for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
		raw = toml_raw_in(subtab, key);
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s", key);
			j->environment_variables[i] = NULL;
			return (-1);
		}
		if (asprintf(&keyval, "%s=%s", key, val) < 0) {
			printlog(LOG_ERR, "asprintf: %s", strerror(errno));
			free(val);
		}
		free(val);

		j->environment_variables[i] = keyval;
	}
	j->environment_variables[i] = NULL;

	return (0);
}

int
parse_job_file(struct job **result, const char *path, const char *id)
{
	FILE *fh;
	char errbuf[256];
	toml_table_t *tab = NULL;
	char *buf;
	struct job *j;

	j = calloc(1, sizeof(*j));
	if (!j) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}
		
	fh = fopen(path, "r");
	if (!fh) {
		printlog(LOG_ERR, "fopen(3) of %s: %s", path, strerror(errno));
		goto err;
	}

	tab = toml_parse_file(fh, errbuf, sizeof(errbuf));
	(void) fclose(fh);
	if (!tab) {
		printlog(LOG_ERR, "failed to parse %s", path);
		goto err;
	}

	j->id = strdup(id);
	if (parse_string(&j->description, tab, "Description", ""))
		goto err;
	if (parse_array_of_strings(&j->after, tab, "After"))
		goto err;
	if (parse_array_of_strings(&j->before, tab, "Before"))
		goto err;
	if (parse_bool(&j->enable, tab, "Enable", true))
		goto err;
	if (parse_bool(&j->enable_globbing, tab, "EnableGlobbing", false))
		goto err;
	if (parse_environment_variables(j, tab))
		goto err;
	if (parse_gid(&j->gid, tab, "Group", getgid()))
		goto err;
	if (parse_bool(&j->init_groups, tab, "InitGroups", true))
		goto err;
	if (parse_bool(&j->keep_alive, tab, "KeepAlive", false))
		goto err;
	if (parse_string(&j->title, tab, "Title", id))
		goto err;
	if (parse_program(j, tab))
		goto err;
	if (parse_string(&j->root_directory, tab, "RootDirectory", "/"))
		goto err;
	if (parse_string(&j->standard_error_path, tab, "StandardErrorPath", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_in_path, tab, "StandardInPath", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_out_path, tab, "StandardOutPath", "/dev/null"))
		goto err;
	
	if (parse_string(&buf, tab, "Umask", "0077"))
		goto err;
	sscanf(buf, "%hi", (unsigned short *) &j->umask);
	free(buf);

	if (parse_uid(&j->uid, tab, "User", getuid()))
		goto err;
	if (uid_to_name(&j->user_name, j->uid))
		goto err;

	if (parse_string(&j->working_directory, tab, "WorkingDirectory", "/"))
		goto err;

	toml_free(tab);
	*result = j;
	return (0);

err:
	toml_free(tab);
	job_free(j);
	return (-1);
}