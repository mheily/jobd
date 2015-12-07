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

#include "ucl.h"
#include "manifest.h"
#include "cvec.h"
#include "log.h"

#include <errno.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const uint32_t DEFAULT_EXIT_TIMEOUT = 20;
static const uint32_t DEFAULT_THROTTLE_INTERVAL = 10;

typedef struct manifest_item_parser {
	const char *key;
	int (*parser)(job_manifest_t, const ucl_object_t *);
} manifest_item_parser_t;

static int parse_not_implemented(job_manifest_t manifest, const ucl_object_t *obj)
{
	return 0;
}

static int parse_label(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->label = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_user_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->user_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_group_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->group_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_program(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->program = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_jail_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->jail_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_working_directory(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->working_directory = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_root_directory(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->root_directory = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_standard_in_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->stdin_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_standard_out_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->stdout_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_standard_error_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_STRING)
		return -1;
	return (manifest->stderr_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int parse_enable_globbing(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_BOOLEAN)
		return -1;
	manifest->enable_globbing = ucl_object_toboolean(obj);
	return 0;
}

static int parse_run_at_load(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_BOOLEAN)
		return -1;
	manifest->run_at_load = ucl_object_toboolean(obj);
	return 0;
}

static int parse_start_on_mount(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_BOOLEAN)
		return -1;
	manifest->start_on_mount = ucl_object_toboolean(obj);
	return 0;
}

static int parse_init_groups(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_BOOLEAN)
		return -1;
	manifest->init_groups = ucl_object_toboolean(obj);
	return 0;
}

static int parse_abandon_process_group(job_manifest_t manifest, const ucl_object_t *obj)
{
	if (ucl_object_type(obj) != UCL_BOOLEAN)
		return -1;
	manifest->abandon_process_group = ucl_object_toboolean(obj);
	return 0;
}

static int parse_environment_variables(job_manifest_t manifest, const ucl_object_t *obj)
{
	const ucl_object_t *tmp = NULL;
	ucl_object_iter_t it = NULL;
	cvec_t vector = NULL;

	if (ucl_object_type(obj) != UCL_OBJECT)
		return -1;

	vector = cvec_new();
	if (!vector)
	{
		log_error("failed to allocate vector");
		return -1;
	}

	while ((tmp = ucl_iterate_object (obj, &it, true)))
	{
		if (ucl_object_type(tmp) != UCL_STRING)
		{
			log_error("unexpected object type while parsing environment variables");
			cvec_free(vector);
			return -1;
		}

		log_debug("adding environment variable: %s=%s", ucl_object_key(tmp), ucl_object_tostring(tmp));
		cvec_push(vector, ucl_object_key(tmp));
		cvec_push(vector, ucl_object_tostring(tmp));
	}

	if (cvec_length(vector) % 2 != 0)
	{
		log_error("parsed an odd number of environment variables");
		cvec_free(vector);
		return -1;
	}

	if (manifest->environment_variables)
		cvec_free(manifest->environment_variables);
	
	manifest->environment_variables = vector;
	return 0;
}

manifest_item_parser_t manifest_parser_map[] = {
	{ "Label", parse_label },
	{ "Disabled", parse_not_implemented },
	{ "UserName", parse_user_name },
	{ "GroupName", parse_group_name },
	{ "inetdCompatibility", parse_not_implemented },
	{ "Program", parse_program },
	{ "ProgramArguments", parse_not_implemented }, /* TODO */
	{ "EnableGlobbing", parse_enable_globbing },
	{ "KeepAlive", parse_not_implemented },
	{ "RunAtLoad", parse_run_at_load },
	{ "WorkingDirectory", parse_working_directory },
	{ "RootDirectory", parse_root_directory },
	{ "EnvironmentVariables", parse_environment_variables },
	{ "Umask", parse_not_implemented },
	{ "TimeOut", parse_not_implemented },
	{ "ExitTimeOut", parse_not_implemented },
	{ "ThrottleInterval", parse_not_implemented },
	{ "InitGroups", parse_init_groups },
	{ "WatchPaths", parse_not_implemented }, /* TODO */
	{ "QueueDirectories", parse_not_implemented }, /* TODO */
	{ "StartOnMount", parse_start_on_mount },
	{ "StartInterval", parse_not_implemented }, /* TODO */
	{ "StartCalendarInterval", parse_not_implemented },
	{ "StandardInPath", parse_standard_in_path },
	{ "StandardOutPath", parse_standard_out_path },
	{ "StandardErrorPath", parse_standard_error_path },
	{ "Debug", parse_not_implemented },
	{ "WaitForDebugger", parse_not_implemented },
	{ "SoftResourceLimits", parse_not_implemented },
	{ "HardResourceLimits", parse_not_implemented },
	{ "Nice", parse_not_implemented },
	{ "AbandonProcessGroup", parse_not_implemented },
	{ "HopefullyExitsFirst", parse_not_implemented },
	{ "HopefullyExitsLast", parse_not_implemented },
	{ "LowPriorityIO", parse_not_implemented },
	{ "LaunchOnlyOnce", parse_not_implemented },
	{ "Sockets", parse_not_implemented }, /* TODO */

	/* XXX-experimental */
	{ "JailName", parse_jail_name },
	{ NULL, NULL },
};


/* Ensure that the User and Group keys are set appropriately */
static inline int job_manifest_set_credentials(job_manifest_t jm)
{
	uid_t uid;
	struct passwd *pwent;
	struct group *grent;

	uid = getuid();
	if (uid == 0) {
		if (!jm->user_name) jm->user_name = strdup("root");
		if (!jm->group_name) jm->group_name = strdup("wheel");
	} else {
		free(jm->user_name);
		if ((pwent = getpwuid(uid)) == NULL) return -1;
		jm->user_name = strdup(pwent->pw_name);

		if ((grent = getgrgid(pwent->pw_gid)) == NULL) return -1;
		free(jm->group_name);
		jm->group_name = strdup(grent->gr_name);
	}
	jm->init_groups = true; /* TODO: deprecate this key entirely */
	if (!jm->user_name || !jm->group_name) return -1;
	return (0);
}

/* Validate the semantic correctness of the manifest */
static int job_manifest_validate(job_manifest_t jm)
{
	size_t i;
	cvec_t new_argv = NULL;

	/* Require Program or ProgramArguments */
	if (!jm->program && cvec_length(jm->program_arguments) == 0) {
		log_error("job %s does not set `program' or `program_arguments`", jm->label);
		return (-1);
	}

	/* If both Program and ProgramArguments are used, consolidate them into one array */
	if (jm->program && cvec_length(jm->program_arguments) > 0) {
		new_argv = cvec_new();
		if (!new_argv) goto err_out;
		if (cvec_push(new_argv, jm->program) < 0) goto err_out;
		for (i = 0; i < cvec_length(jm->program_arguments); i++) {
			if (cvec_push(new_argv, cvec_get(jm->program_arguments, i)) < 0) goto err_out;
		}
		cvec_free(jm->program_arguments);
		jm->program_arguments = new_argv;
		cvec_debug(new_argv);
	}

	/* By convention, argv[0] == Program */
	if (jm->program && cvec_length(jm->program_arguments) == 0) {
		if (cvec_push(jm->program_arguments, jm->program) < 0) goto err_out;
	}

	if (job_manifest_set_credentials(jm) < 0) {
		log_error("unable to set credentials");
		return (-1);
	}
	return (0);

 err_out:
	free(new_argv);
	return (-1);
}

job_manifest_t job_manifest_new()
{
	job_manifest_t jm;

	jm = calloc(1, sizeof(*jm));
	
	if (!jm)
		return (NULL);
	
	jm->exit_timeout = DEFAULT_EXIT_TIMEOUT;
	jm->throttle_interval = DEFAULT_THROTTLE_INTERVAL;
	jm->init_groups = true;
	
	if (!(jm->program_arguments = cvec_new()))
		goto err_out;
	
	if (!(jm->watch_paths = cvec_new()))
		goto err_out;
	
	if (!(jm->queue_directories = cvec_new()))
		goto err_out;
	
	if (!(jm->environment_variables = cvec_new()))
		goto err_out;
	
	SLIST_INIT(&jm->sockets);
	
	return (jm);

 err_out:
	job_manifest_free(jm);
	return (NULL);
}

void job_manifest_free(job_manifest_t jm)
{
	struct job_manifest_socket *jms, *jms_tmp;

	if (jm == NULL)
		return;
	free(jm->json_buf);
	free(jm->label);
	free(jm->user_name);
	free(jm->group_name);
	cvec_free(jm->program_arguments);
	cvec_free(jm->watch_paths);
	cvec_free(jm->queue_directories);
	cvec_free(jm->environment_variables);
	free(jm->working_directory);
	free(jm->root_directory);
	free(jm->stdin_path);
	free(jm->stdout_path);
	free(jm->stderr_path);
	free(jm->jail_name);
	SLIST_FOREACH_SAFE(jms, &jm->sockets, entry, jms_tmp) {
		SLIST_REMOVE(&jm->sockets, jms, job_manifest_socket, entry);
		job_manifest_socket_free(jms);
	}
	free(jm);
}

int job_manifest_read(job_manifest_t jm, const char *infile)
{
	char *buf = NULL;
	size_t bufsz;
	struct stat sb;
	ssize_t bytes_read;
	FILE *f = NULL;

	if (stat(infile, & sb) != 0) goto err_out;
	if (sb.st_size > 65535) goto err_out;
	bufsz = sb.st_size + 1;
	buf = malloc(bufsz);
	if (buf == NULL) goto err_out;
	memset(buf + bufsz, 0, 1);
	if ((f = fopen(infile, "r")) == NULL) goto err_out;
	bytes_read = fread(buf, 1, sb.st_size, f);
	if (bytes_read != sb.st_size) goto err_out;
	if (fclose(f) != 0) goto err_out;

	return (job_manifest_parse(jm, buf, bufsz));

 err_out:
	if (f) (void)fclose(f);
	free(buf);
	return (-1);
}

/* NOTE: buf will be owned by job_manifest_t for free() purposes */
int job_manifest_parse(job_manifest_t jm, char *buf, size_t bufsize)
{
	int rc = 0;
	struct ucl_parser *parser = NULL;
	ucl_object_t *obj = NULL;
	const ucl_object_t *tmp = NULL;
	ucl_object_iter_t it = NULL;
  
	parser = ucl_parser_new(0);
	ucl_parser_add_chunk(parser, buf, bufsize);

	if (ucl_parser_get_error(parser)) {
		log_error("%s", ucl_parser_get_error(parser));
		rc = -1;
		goto cleanup;
	}
	else {
		obj = ucl_parser_get_object(parser);
	}
  
	jm->json_buf = buf;
	jm->json_size = bufsize;

	while ((tmp = ucl_iterate_object (obj, &it, true)))
	{
		log_debug("parsing key `%s'", ucl_object_key(tmp));
		for (manifest_item_parser_t *item_parser = manifest_parser_map; item_parser->key != NULL; item_parser++)
		{
			if (!strcmp(item_parser->key, ucl_object_key(tmp)))
			{
				log_debug("parsing value `%s'", ucl_object_tostring_forced(tmp));
				if (item_parser->parser(jm, tmp))
				{
					log_error("failed while parsing key `%s' with value: `%s'", ucl_object_key(tmp), ucl_object_tostring_forced(tmp));
					rc = -1;
					goto cleanup;
				}
				break;
			}
		}
	}

cleanup:
    
	if (parser)
		ucl_parser_free(parser);

	if (obj)
		ucl_object_unref(obj);

	return rc;
}
