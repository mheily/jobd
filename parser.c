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
#include <libgen.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "database.h"
#include "logger.h"
#include "toml.h"
#include "array.h"
#include "job.h"
#include "parser.h"

struct job_parser {
    struct job *job;
    toml_table_t *tab;
};

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
	
	if (!toml_rtob(raw, &rv)) {
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
parse_gid(gid_t *result, const char *group_name)
{
	struct group *grp;

	if (!group_name || group_name[0] == '\0') {
		*result = getgid();
		return (0);
	}

	grp = getgrnam(group_name);
	if (grp) {
		*result = grp->gr_gid;
		return (0);
	} else {
		printlog(LOG_ERR, "group not found: %s", group_name);
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
parse_array_of_strings(struct string_array *result, toml_table_t *tab, const char *top_key)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	int i;

	arr = toml_array_in(tab, top_key);
	if (!arr) {
		return (0);
	}

	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s element %d", top_key, i);
			return (-1);
		} else {
			if (string_array_push_back(result, strdup(val)) < 0)
				return (-1);
		}
	}

	return (0);
}

static int
parse_environment_variables(struct job *job, toml_table_t *tab)
{
	toml_table_t* subtab;
	const char *key;
	char *val, *keyval;
	const char *raw;
	int i;

	subtab = toml_table_in(tab, "environment");
	if (!subtab) {
		return (0);
	}
		
	for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
		raw = toml_raw_in(subtab, key);
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s", key);
			return (-1);
		}
		if (asprintf(&keyval, "%s=%s", key, val) < 0) {
			printlog(LOG_ERR, "asprintf: %s", strerror(errno));
			free(val);
		}
		free(val);

		if (string_array_push_back(job->environment_variables, keyval) < 0)
			return (-1);
	}

	return (0);
}

int
parse_job(struct job_parser *jpr)
{
	struct job * const j = jpr->job;
	toml_table_t * const tab = jpr->tab;

	if (parse_string(&j->id, tab, "name", ""))
		goto err;
	if (parse_string(&j->command, tab, "command", "/bin/true"))
		goto err;
	if (parse_string(&j->description, tab, "description", ""))
		goto err;
	if (parse_array_of_strings(j->after, tab, "after"))
		goto err;
	if (parse_array_of_strings(j->before, tab, "before"))
		goto err;
	if (parse_bool(&j->enable, tab, "enable", true))
		goto err;
	if (parse_environment_variables(j, tab))
		goto err;
	if (parse_string(&j->group_name, tab, "group", ""))
		goto err;		
	if (parse_gid(&j->gid, j->group_name))
		goto err;
	if (parse_bool(&j->init_groups, tab, "init_groups", true))
		goto err;
	if (parse_bool(&j->keep_alive, tab, "keep_alive", false))
		goto err;
	if (parse_string(&j->title, tab, "title", j->id))
		goto err;
	if (parse_string(&j->root_directory, tab, "root_directory", "/"))
		goto err;
	if (parse_string(&j->standard_error_path, tab, "stderr", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_in_path, tab, "stdin", "/dev/null"))
		goto err;
	if (parse_string(&j->standard_out_path, tab, "stdout", "/dev/null"))
		goto err;
	
	if (parse_string(&j->umask_str, tab, "umask", "0077"))
		goto err;
	sscanf(j->umask_str, "%hi", (unsigned short *) &j->umask);

	if (parse_uid(&j->uid, tab, "user", getuid()))
		goto err;
	if (uid_to_name(&j->user_name, j->uid))
		goto err;

	if (parse_string(&j->working_directory, tab, "cwd", "/"))
		goto err;

	return (0);

err:
	return (-1);
}

static int
generate_job_name(struct job *job, const char *path)
{
	char *buf, *res;

	if (!strcmp(path, "/dev/stdin"))
		return (-1);
	// TODO: generate a random name instead

	buf = res = NULL;
	buf = strdup(path);
	if (!buf)
		goto os_err;
	res = strdup(basename(buf));
	if (!res)
		goto os_err;
	char *c = strrchr(res, '.');
	if (c)
		*c = '\0';

	free(buf);
	free(job->id);
	job->id = res;

	return (0);

os_err:
	free(buf);
	free(res);
	printlog(LOG_ERR, "OS error: %s", strerror(errno));
	return (-1);
}

int
parse_job_file(struct job_parser *jpr, const char *path)
{
	FILE *fh;
	char errbuf[256];

	jpr->job = job_new();
	if (!jpr->job) {
		printlog(LOG_ERR, "calloc: %s", strerror(errno));
		return (-1);
	}
		
	fh = fopen(path, "r");
	if (!fh) {
		printlog(LOG_ERR, "fopen(3) of %s: %s", path, strerror(errno));
		goto err;
	}

	jpr->tab = toml_parse_file(fh, errbuf, sizeof(errbuf));
	if (!jpr->tab) {
		printlog(LOG_ERR, "error parsing %s: %s", path, (char*) &errbuf);
		goto err;
	}
	(void) fclose(fh);

	if (parse_job(jpr) < 0)
		goto err;

	if (jpr->job->id[0] == '\0') {
		if (generate_job_name(jpr->job, path) < 0)
			goto err;
	}

	return (0);

err:
	job_parser_free(jpr);
	return (-1);
}

struct job_parser * job_parser_new(void)
{
	return calloc(1, sizeof(struct job_parser));
}

void job_parser_free(struct job_parser *jpr)
{
	if (jpr) {
		toml_free(jpr->tab);
		job_free(jpr->job);
		free(jpr);
	}
}

static int
job_db_insert_depends(const struct job *job)
{
	const char *sql = "INSERT INTO job_depends "
					  "  (before_job_id, after_job_id) "
					  "VALUES "
					  "  (?,?)";
	uint32_t i;
	sqlite3_stmt *stmt;
	int rv;

	for (i = 0; i < string_array_len(job->before); i++) {
		stmt = NULL;
		rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 1, job->id, -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 2, string_array_data(job->before)[i], -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		if (!rv)
			return (-1);
	}

	for (i = 0; i < string_array_len(job->after); i++) {
		stmt = NULL;
		rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 1, string_array_data(job->after)[i], -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 2, job->id, -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		if (!rv)
			return (-1);
	}
	
	return (0);
}

static int
job_db_insert_methods(struct job_parser *jpr)
{
	toml_table_t* subtab;
	const char *key;
	char *val;
	const char *raw;
	int i;

	subtab = toml_table_in(jpr->tab, "methods");
	if (!subtab) {
		return (0);
	}
		
	for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
		raw = toml_raw_in(subtab, key);
		if (!raw || toml_rtos(raw, &val)) {
			printlog(LOG_ERR, "error parsing %s", key);
			return (-1);
		}

		int success;
		sqlite3_stmt *stmt;
		const char *sql =
			"INSERT INTO job_methods "
			"(job_id, name, script) "
			"VALUES (?, ?, ?)";

        success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
                sqlite3_bind_int64(stmt, 1, jpr->job->row_id) == SQLITE_OK &&
                sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC) == SQLITE_OK &&
				sqlite3_bind_text(stmt, 3, val, -1, SQLITE_STATIC) == SQLITE_OK &&
            	sqlite3_step(stmt) == SQLITE_DONE;

        sqlite3_finalize(stmt);
		free(val);

		if (!success)
			return (-1);
	}

	return (0);
}

int
job_db_insert(struct job_parser *jpr)
{
	int rv;
	sqlite3_stmt *stmt;
	struct job *job = jpr->job;

	const char *sql = "INSERT INTO jobs (job_id, description, gid, init_groups,"
		"keep_alive, root_directory, standard_error_path,"
		"standard_in_path, standard_out_path, umask, user_name,"
		"working_directory, enable, command) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

	rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 1, job->id, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 2, job->description, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 3, job->group_name, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_int(stmt, 4, job->init_groups) == SQLITE_OK &&
		 sqlite3_bind_int(stmt, 5, job->keep_alive) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 6, job->root_directory, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 7, job->standard_error_path, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 8, job->standard_in_path, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 9, job->standard_out_path, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 10, job->umask_str, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 11, job->user_name, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 12, job->working_directory, -1, SQLITE_STATIC) == SQLITE_OK &&
		 sqlite3_bind_int(stmt, 13, job->enable) == SQLITE_OK &&
		 sqlite3_bind_text(stmt, 14, job->command, -1, SQLITE_STATIC) == SQLITE_OK;

	if (!rv || sqlite3_step(stmt) != SQLITE_DONE) {
		printlog(LOG_ERR, "error importing %s", job->id);
		sqlite3_finalize(stmt);
		return (-1);
  	}
	jpr->job->row_id = sqlite3_last_insert_rowid(dbh);
	sqlite3_finalize(stmt);

	if (job_db_insert_depends(job) < 0) {
		printlog(LOG_ERR, "error importing %s dependencies", job->id);
		return (-1);
	}

	if (job_db_insert_methods(jpr) < 0) {
		printlog(LOG_ERR, "error importing %s methods", job->id);
		return (-1);
	}

	return (0);
}