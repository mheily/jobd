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

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "database.h"
#include "logger.h"
#include "memory.h"
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
	} else {
        if (!toml_rtob(raw, &rv)) {
            *result = rv;
        } else {
            *result = 2; // Not really helpful
            return printlog(LOG_ERR, "error converting to bool");
        }
    }
    return 0;
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
			return 0;
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

	return 0;

err:
	*result = NULL;
	return (-1);
}


static int
parse_uid(uid_t *result, toml_table_t *tab, const char *key, const uid_t default_value)
{
	struct passwd *pwd;
	const char *raw;
	char CLEANUP_STR *buf = NULL;

	*result = -1;

	raw = toml_raw_in(tab, key);
	if (!raw) {
		*result = default_value;
	} else {
        if (toml_rtos(raw, &buf))
            return printlog(LOG_ERR, "invalid value for %s", key);

        pwd = getpwnam(buf);
        if (!pwd)
            return printlog(LOG_ERR, "user not found: %s", buf);
        *result = pwd->pw_uid;
    }

	return 0;
}

static int
uid_to_name(char **name, uid_t uid)
{
	struct passwd *pwd = getpwuid(uid);
	if (!pwd)
        return printlog(LOG_ERR, "getpwuid(3) of %d: %s", uid, strerror(errno));
    *name = strdup(pwd->pw_name);
    if (*name == NULL)
        return printlog(LOG_ERR, "strdup(3): %s", strerror(errno));
    return 0;
}

static int
parse_array_of_strings(struct string_array *result, toml_table_t *tab, const char *top_key)
{
	toml_array_t* arr;
	char *val;
	const char *raw;
	int i;

	arr = toml_array_in(tab, top_key);
	if (!arr)
		return 0;

	for (i = 0; (raw = toml_raw_at(arr, i)) != 0; i++) {
		if (!raw || toml_rtos(raw, &val))
			return printlog(LOG_ERR, "error parsing %s element %d", top_key, i);
//FIXME: leak if strdup fail
        if (string_array_push_back(result, strdup(val)) < 0)
            return -1;
	}

	return 0;
}

static int
parse_dict_of_strings(struct string_array *result, toml_table_t *tab, const char *top_key)
{
    toml_table_t* subtab;
    const char *key;
    char *val;
    const char *raw;
    int i;

    subtab = toml_table_in(tab, top_key);
    if (!subtab) {
        return 0;
    }

    for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
        raw = toml_raw_in(subtab, key);
        if (!raw || toml_rtos(raw, &val))
            return printlog(LOG_ERR, "error parsing %s", key);

        if (string_array_push_back(result, strdup(key)) < 0)
            return -1;
        if (string_array_push_back(result, strdup(val)) < 0) {
            //FIXME: need to pop the last element off
            return -1;
        }
    }
    return 0;
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
	if (!subtab)
	    return 0;
		
	for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
		raw = toml_raw_in(subtab, key);
		if (!raw || toml_rtos(raw, &val))
			return printlog(LOG_ERR, "error parsing %s", key);
		if (asprintf(&keyval, "%s=%s", key, val) < 0) {
			printlog(LOG_ERR, "asprintf: %s", strerror(errno));
			free(val);
		}
		free(val);
//fixme several leaks here
		if (string_array_push_back(job->environment_variables, keyval) < 0)
			return (-1);
	}

	return 0;
}

int
parse_job(struct job_parser *jpr)
{
	struct job * const j = jpr->job;
	char *buf;
	toml_table_t * const tab = jpr->tab;

#define goto_err(why) do { printlog(LOG_ERR, "error parsing "#why); goto err; } while (0)

	if (parse_string(&j->id, tab, "name", ""))
		goto_err("name");
	if (parse_string(&j->description, tab, "description", ""))
		goto_err("description");
	if (parse_array_of_strings(j->after, tab, "after"))
		goto_err("after");
	if (parse_array_of_strings(j->before, tab, "before"))
		goto_err("before");
	if (parse_bool(&j->wait_flag, tab, "wait", false))
		goto_err("wait");
	if (parse_environment_variables(j, tab))
		goto_err("environment");
	if (parse_string(&j->group_name, tab, "group", ""))
		goto_err("group");
	if (parse_bool(&j->init_groups, tab, "init_groups", true))
		goto_err("init_groups");
	if (parse_bool(&j->keep_alive, tab, "keep_alive", false))
		goto_err("keep_alive");
	if (parse_string(&j->title, tab, "title", j->id))
		goto_err("title");
	if (parse_dict_of_strings(j->methods, tab, "methods"))
	    goto_err("methods");
	if (parse_string(&j->root_directory, tab, "root_directory", "/"))
		goto_err("root_directory");
	if (parse_string(&j->standard_error_path, tab, "stderr", "/dev/null"))
		goto_err("standard_error_path");
	if (parse_string(&j->standard_in_path, tab, "stdin", "/dev/null"))
		goto_err("standard_in_path");
	if (parse_string(&j->standard_out_path, tab, "stdout", "/dev/null"))
		goto_err("standard_out_path");

	if (parse_string(&buf, tab, "type", ""))
		goto_err("type");
	if (!buf)
		j->job_type = JOB_TYPE_UNKNOWN;
	else if (!strcmp(buf, "task"))
		j->job_type = JOB_TYPE_TASK;
	else if (!strcmp(buf, "service"))
		j->job_type = JOB_TYPE_SERVICE;
	else
		j->job_type = JOB_TYPE_UNKNOWN;
	free(buf);
	buf = NULL;
	if (j->job_type == JOB_TYPE_UNKNOWN)
		goto_err("type-to-value");

	if (parse_string(&j->umask_str, tab, "umask", "0077"))
		goto_err("umask");
	sscanf(j->umask_str, "%hi", (unsigned short *) &j->umask);

	if (parse_uid(&j->uid, tab, "user", getuid()))
		goto_err("user");
	if (uid_to_name(&j->user_name, j->uid))
		goto_err("user_name");

	if (parse_string(&j->working_directory, tab, "cwd", "/"))
		goto_err("working_directory");

	return (0);

#undef goto_err

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
	FILE CLEANUP_FILE *fh = NULL;
	char errbuf[256];

	fh = fopen(path, "r");
	if (!fh)
		return printlog(LOG_ERR, "fopen(3) of %s: %s", path, strerror(errno));

	jpr->tab = toml_parse_file(fh, errbuf, sizeof(errbuf));
	if (!jpr->tab)
		return printlog(LOG_ERR, "error parsing %s: %s", path, (char*) &errbuf);

	if (parse_job(jpr) < 0)
		return printlog(LOG_ERR, "parse_job() failed");

	if (jpr->job->id[0] == '\0' && generate_job_name(jpr->job, path) < 0)
	    return printlog(LOG_ERR, "unable to generate job name");

	return 0;
}

int job_parser_new(struct job_parser **result)
{
    struct job_parser CLEANUP_JOB_PARSER *parser = NULL;
    *result = NULL;
    parser = calloc(1, sizeof(struct job_parser));
    if (!parser)
        return printlog(LOG_ERR, "calloc: %s", strerror(errno));
    parser->job = job_new();
    if (!parser->job)
        return printlog(LOG_ERR, "alloc error");

    LOCAL_MOVE(result, parser);

    return 0;
}

void job_parser_destroy(struct job_parser **jpr)
{
	if (*jpr) {
		toml_free((*jpr)->tab);
		job_free((*jpr)->job);
		free(*jpr);
		*jpr = NULL;
	}
}

struct job *job_parser_get_job(struct job_parser *jpr)
{
    return jpr->job;
}

static int
job_db_insert_depends(const struct job *job)
{
	const char *sql = "INSERT INTO job_depends "
					  "  (before_job_id, after_job_id) "
					  "VALUES "
					  "  (?,?)";
	uint32_t i;
	sqlite3_stmt CLEANUP_STMT *stmt = NULL;
	int rv;

	for (i = 0; i < string_array_len(job->before); i++) {
		stmt = NULL;
		rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 1, job->id, -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 2, string_array_data(job->before)[i], -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_step(stmt) == SQLITE_DONE;
		if (!rv)
			return (-1);
	}

	for (i = 0; i < string_array_len(job->after); i++) {
		stmt = NULL;
		rv = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 1, string_array_data(job->after)[i], -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_bind_text(stmt, 2, job->id, -1, SQLITE_STATIC) == SQLITE_OK &&
			 sqlite3_step(stmt) == SQLITE_DONE;
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
		sqlite3_stmt CLEANUP_STMT *stmt = NULL;
		const char *sql =
			"INSERT INTO job_methods "
			"(job_id, name, script) "
			"VALUES (?, ?, ?)";

        success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
                sqlite3_bind_int64(stmt, 1, jpr->job->row_id) == SQLITE_OK &&
                sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC) == SQLITE_OK &&
				sqlite3_bind_text(stmt, 3, val, -1, SQLITE_STATIC) == SQLITE_OK &&
            	sqlite3_step(stmt) == SQLITE_DONE;

		free(val);

		if (!success)
			return (-1);
	}

	return (0);
}

static int
_toml_raw_to_sqlite_value(char **result, int *datatype, const char *raw)
{
    if (!strncmp(raw, "true", 4)) {
        *datatype = PROPERTY_TYPE_BOOL;
        *result = strdup("1");
    } else if (!strncmp(raw, "false", 5)) {
        *datatype = PROPERTY_TYPE_BOOL;
        *result = strdup("0");
    } else if (*raw == '\'' || *raw == '"') {
        *datatype = PROPERTY_TYPE_STRING;
        if (toml_rtos(raw, result))
            return printlog(LOG_ERR, "error converting raw value to string");
    } else {
        *result = NULL;
        *datatype = PROPERTY_TYPE_INVALID;
        return printlog(LOG_ERR, "unable to determine datatype");
    }
    return 0;
}

static int
job_db_insert_properties(struct job_parser *jpr)
{
    toml_table_t *subtab;
    const char *key;
    char *val;
    const char *raw;
    int i, datatype;

    subtab = toml_table_in(jpr->tab, "properties");
    if (!subtab)
    	goto insert_default_values;

    for (i = 0; (key = toml_key_in(subtab, i)) != 0; i++) {
        raw = toml_raw_in(subtab, key);
        if (!raw) {
            printlog(LOG_ERR, "error parsing `%s' into raw", key);
            return (-1);
        }
        if (_toml_raw_to_sqlite_value(&val, &datatype, raw) < 0)
            return (-1);

        int success;
        sqlite3_stmt CLEANUP_STMT *stmt = NULL;
        const char *sql =
                "INSERT INTO properties "
                "(job_id, datatype_id, name, default_value, current_value) "
                "VALUES (?, ?, ?, ?, ?)";

        success = sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) == SQLITE_OK &&
                  sqlite3_bind_int64(stmt, 1, jpr->job->row_id) == SQLITE_OK &&
                  sqlite3_bind_int(stmt, 2, datatype) == SQLITE_OK &&
                  sqlite3_bind_text(stmt, 3, key, -1, SQLITE_STATIC) == SQLITE_OK &&
                  sqlite3_bind_text(stmt, 4, val, -1, SQLITE_STATIC) == SQLITE_OK &&
                  sqlite3_bind_text(stmt, 5, val, -1, SQLITE_STATIC) == SQLITE_OK &&
                  sqlite3_step(stmt) == SQLITE_DONE;

        free(val);

        if (!success)
            return (-1);
    }

insert_default_values:
    if (!subtab || toml_raw_in(subtab, "enabled") == 0) {
		sqlite3_stmt CLEANUP_STMT *stmt = NULL;
		const char *sql =
				"INSERT INTO properties "
				"(job_id, datatype_id, name, default_value, current_value) "
				"VALUES (?, (SELECT id FROM datatypes WHERE name = 'boolean'), 'enabled', 1, 1)";
		if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
			return db_error;
		if (sqlite3_bind_int64(stmt, 1, jpr->job->row_id) != SQLITE_OK)
			return db_error;
		if (sqlite3_step(stmt) != SQLITE_DONE)
			return db_error;
	}

    return (0);
}

int job_db_insert_state(struct job_parser *jpr)
{
    static const char * disabled_str = "disabled";
    static const char * enabled_str = "pending";
    const char *state_str;

    toml_table_t *subtab = toml_table_in(jpr->tab, "properties");
    if (subtab) {
        const char *raw = toml_raw_in(subtab, "enabled");
        int datatype;
        if (raw) {
            char CLEANUP_STR *buf = NULL;
            if (_toml_raw_to_sqlite_value(&buf, &datatype, raw) < 0)
                return -1;
            if (datatype != PROPERTY_TYPE_BOOL)
                return -2;
            state_str = strcmp(buf, "0") == 0 ? disabled_str : enabled_str;
        } else {
            state_str = enabled_str;
        }
    } else {
        state_str = enabled_str;
    }

    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql =
            "INSERT INTO jobs_current_states "
            "(job_id, job_state_id) VALUES (?,(SELECT id FROM job_states WHERE name = ?))";

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, jpr->job->row_id) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, 2, state_str, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;

    return 0;
}


int
job_db_insert(struct job_parser *jpr)
{
    int rv;
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    struct job *job = jpr->job;

    const char *sql = "INSERT INTO jobs (job_id, description, gid, init_groups, "
                      "keep_alive, root_directory, standard_error_path, "
                      "standard_in_path, standard_out_path, umask, user_name, "
                      "working_directory, wait, job_type_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

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
         sqlite3_bind_int(stmt, 13, job->wait_flag) == SQLITE_OK &&
         sqlite3_bind_int(stmt, 14, job->job_type) == SQLITE_OK;

    if (!rv || sqlite3_step(stmt) != SQLITE_DONE)
        return printlog(LOG_ERR, "error importing %s", job->id);

    jpr->job->row_id = sqlite3_last_insert_rowid(dbh);

    if (job_db_insert_depends(job) < 0)
        return printlog(LOG_ERR, "error importing %s dependencies", job->id);

    if (job_db_insert_methods(jpr) < 0)
        return printlog(LOG_ERR, "error importing %s methods", job->id);

    if (job_db_insert_properties(jpr) < 0)
        return printlog(LOG_ERR, "error importing %s properties", job->id);

    if (job_db_insert_state(jpr) < 0)
        return printlog(LOG_ERR, "error setting initial state of %s", job->id);

    return 0;
}

static int
import_from_file(const char *path)
{
	struct job_parser CLEANUP_JOB_PARSER *jpr = NULL;


	if (job_parser_new(&jpr) < 0)
		return printlog(LOG_ERR, "allocation failed");

	printlog(LOG_DEBUG, "importing job from manifest at %s", path);
	if (parse_job_file(jpr, path) != 0)
		return printlog(LOG_ERR, "error parsing %s", path);

	if (job_db_insert(jpr) < 0)
		abort();

	return 0;
}

static int
import_from_directory(const char *configdir)
{
	DIR *dirp;
	struct dirent *entry;
	char *path;
	int rv = 0;

	printlog(LOG_DEBUG, "importing all jobs in directory: %s", configdir);
	if ((dirp = opendir(configdir)) == NULL)
		return printlog(LOG_ERR, "opendir(3) of %s", configdir);

	while (dirp) {
		errno = 0;
		entry = readdir(dirp);
		if (errno != 0)
			return printlog(LOG_ERR, "readdir(3): %s", strerror(errno));
		if (!entry)
			break;
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		char *extension = strrchr(entry->d_name, '.');
		if (!extension || strcmp(extension, ".toml"))
			continue;
		if (asprintf(&path, "%s/%s", configdir, entry->d_name) < 0)
			return printlog(LOG_ERR, "asprintf(3): %s", strerror(errno));
		printlog(LOG_DEBUG, "parsing %s", path);
		if (import_from_file(path) < 0) {
			printlog(LOG_ERR, "error parsing %s", path);
			free(path);
			rv = -1;
			continue;
		}
		free(path);
	}
	if (closedir(dirp) < 0)
		return printlog(LOG_ERR, "closedir(3): %s", strerror(errno));

	return rv;
}

int
parser_import(const char *path)
{
    char default_path[PATH_MAX];
    int rv;
    struct stat sb;

    if (!path) {
        int rv = snprintf((char *) &default_path, sizeof(default_path),
                          "%s/%s/manifests", compile_time_option.datarootdir, compile_time_option.project_name);
        if (rv >= (int) sizeof(default_path) || rv < 0)
            return printlog(LOG_ERR, "snprintf failed");
        path = (char *) &default_path;
    }

    rv = stat(path, &sb);
    if (rv < 0)
        return printlog(LOG_ERR, "stat(2) of %s: %s", path, strerror(errno));

    if (db_exec(dbh, "BEGIN TRANSACTION") < 0)
        return db_error;

    if (S_ISDIR(sb.st_mode))
        rv = import_from_directory(path);
    else
        rv = import_from_file(path);

    if (rv == 0) {
        if (db_exec(dbh, "COMMIT") < 0)
            return -1;
        else
            return 0;
    } else {
        (void) db_exec(dbh, "ROLLBACK");
        return -1;
    }
}