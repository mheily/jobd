/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 * Copyright (c) 2015 Steve Gerbino <steve@gerbino.co>
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
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#include <ucl.h>
#include <unistd.h>

#include "cvec.h"
#include "log.h"
#include "manifest.h"

static const uint32_t DEFAULT_EXIT_TIMEOUT = 20;
static const uint32_t DEFAULT_THROTTLE_INTERVAL = 10;

typedef struct job_manifest_item_parser {
	const char *key;
	ucl_type_t object_type;
	int (*parser)(job_manifest_t job_manifest, const ucl_object_t *object);
} job_manifest_item_parser_t;

static bool job_manifest_object_is_type(const ucl_object_t *obj, ucl_type_t object_type);
static int job_manifest_parse(job_manifest_t job_manifest, unsigned char *buf, size_t bufsize);
static int job_manifest_parse_label(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_user_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_group_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_program(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_jail_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_working_directory(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_root_directory(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_standard_in_path(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_standard_out_path(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_standard_error_path(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_enable_globbing(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_run_at_load(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_start_on_mount(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_init_groups(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_abandon_process_group(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_environment_variables(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_program_arguments(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_watch_paths(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_queue_directories(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_start_interval(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_sockets(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_cvec(cvec_t *dst, const ucl_object_t *obj);

static job_manifest_item_parser_t manifest_parser_map[] = {
	{ "Label",                 UCL_STRING,  job_manifest_parse_label },
	{ "UserName",              UCL_STRING,  job_manifest_parse_user_name },
	{ "GroupName",             UCL_STRING,  job_manifest_parse_group_name },
	{ "Program",               UCL_STRING,  job_manifest_parse_program },
	{ "ProgramArguments",      UCL_ARRAY,   job_manifest_parse_program_arguments },
	{ "EnableGlobbing",        UCL_BOOLEAN, job_manifest_parse_enable_globbing },
	{ "RunAtLoad",             UCL_BOOLEAN, job_manifest_parse_run_at_load },
	{ "WorkingDirectory",      UCL_STRING,  job_manifest_parse_working_directory },
	{ "RootDirectory",         UCL_STRING,  job_manifest_parse_root_directory },
	{ "EnvironmentVariables",  UCL_OBJECT,  job_manifest_parse_environment_variables },
	{ "InitGroups",            UCL_BOOLEAN, job_manifest_parse_init_groups },
	{ "WatchPaths",            UCL_ARRAY,   job_manifest_parse_watch_paths },
	{ "QueueDirectories",      UCL_ARRAY,   job_manifest_parse_queue_directories },
	{ "StartOnMount",          UCL_BOOLEAN, job_manifest_parse_start_on_mount },
	{ "StartInterval",         UCL_INT,     job_manifest_parse_start_interval },
	{ "StandardInPath",        UCL_STRING,  job_manifest_parse_standard_in_path },
	{ "StandardOutPath",       UCL_STRING,  job_manifest_parse_standard_out_path },
	{ "StandardErrorPath",     UCL_STRING,  job_manifest_parse_standard_error_path },
	{ "AbandonProcessGroup",   UCL_BOOLEAN, job_manifest_parse_abandon_process_group },
	{ "JailName",              UCL_STRING,  job_manifest_parse_jail_name },
	{ "Sockets",               UCL_OBJECT,  job_manifest_parse_sockets },
	/*
	{ "inetdCompatibility",    SKIP_ITEM,   NULL },
	{ "KeepAlive",             SKIP_ITEM,   NULL },
	{ "Umask",                 SKIP_ITEM,   NULL },
	{ "TimeOut",               SKIP_ITEM,   NULL },
	{ "ExitTimeOut",           SKIP_ITEM,   NULL },
	{ "Disabled",              SKIP_ITEM,   NULL },
	{ "ThrottleInterval",      SKIP_ITEM,   NULL },
	{ "StartCalendarInterval", SKIP_ITEM,   NULL },
	{ "Debug",                 SKIP_ITEM,   NULL },
	{ "WaitForDebugger",       SKIP_ITEM,   NULL },
	{ "SoftResourceLimits",    SKIP_ITEM,   NULL },
	{ "HardResourceLimits",    SKIP_ITEM,   NULL },
	{ "Nice",                  SKIP_ITEM,   NULL },
	{ "HopefullyExitsFirst",   SKIP_ITEM,   NULL },
	{ "HopefullyExitsLast",    SKIP_ITEM,   NULL },
	{ "LowPriorityIO",         SKIP_ITEM,   NULL },
	{ "LaunchOnlyOnce",        SKIP_ITEM,   NULL },
	*/
	{ NULL,                    UCL_NULL,    NULL },
};

static bool job_manifest_object_is_type(const ucl_object_t *obj, ucl_type_t type)
{
	if (ucl_object_type(obj) != type)
		return false;
	return true;
}

static int job_manifest_parse_cvec(cvec_t *dst, const ucl_object_t *obj)
{
	const ucl_object_t *tmp = NULL;
	ucl_object_iter_t it = NULL;
	cvec_t vector = NULL;

	vector = cvec_new();
	if (!vector)
	{
		log_error("unable to allocate vector");
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

		log_debug("adding array element: %s", ucl_object_tostring(tmp));
		cvec_push(vector, (char*)ucl_object_tostring(tmp));
	}

	if (dst)
		cvec_free(*dst);
	
	*dst = vector;
	return 0;
}

static int job_manifest_parse_label(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->label = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_user_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->user_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_group_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->group_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_program(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->program = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_jail_name(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->jail_name = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_working_directory(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->working_directory = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_root_directory(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->root_directory = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_standard_in_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->stdin_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_standard_out_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->stdout_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_standard_error_path(job_manifest_t manifest, const ucl_object_t *obj)
{
	return (manifest->stderr_path = strdup(ucl_object_tostring(obj))) ? 0 : -1;
}

static int job_manifest_parse_enable_globbing(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->enable_globbing = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_parse_run_at_load(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->run_at_load = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_parse_start_on_mount(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->start_on_mount = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_parse_init_groups(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->init_groups = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_parse_abandon_process_group(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->abandon_process_group = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_parse_environment_variables(job_manifest_t manifest, const ucl_object_t *obj)
{
	const ucl_object_t *tmp = NULL;
	ucl_object_iter_t it = NULL;
	cvec_t vector = NULL;

	vector = cvec_new();
	if (!vector)
	{
		log_error("failed to allocate vector");
		return -1;
	}

	while ((tmp = ucl_iterate_object (obj, &it, true)))
	{
		if (!job_manifest_object_is_type(tmp, UCL_STRING))
		{
			log_error("unexpected object type while parsing environment variables");
			cvec_free(vector);
			return -1;
		}

		log_debug("adding environment variable: %s=%s", ucl_object_key(tmp), ucl_object_tostring(tmp));
		cvec_push(vector, (char*)ucl_object_key(tmp));
		cvec_push(vector, (char*)ucl_object_tostring(tmp));
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

static int job_manifest_parse_program_arguments(job_manifest_t manifest, const ucl_object_t *obj)
{
	return job_manifest_parse_cvec(&manifest->program_arguments, obj);
}

static int job_manifest_parse_watch_paths(job_manifest_t manifest, const ucl_object_t *obj)
{
	return job_manifest_parse_cvec(&manifest->watch_paths, obj);
}

static int job_manifest_parse_queue_directories(job_manifest_t manifest, const ucl_object_t *obj)
{
	return job_manifest_parse_cvec(&manifest->queue_directories, obj);
}

static int job_manifest_parse_start_interval(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->start_interval = ucl_object_toint(obj);
	return 0;
}

static int job_manifest_parse_sockets(job_manifest_t manifest, const ucl_object_t *obj)
{
	/*struct job_manifest_socket *socket = NULL;*/
	
	return 0;
}

static int job_manifest_set_credentials(job_manifest_t job_manifest)
{
	uid_t uid;
	struct passwd *pwent;
	struct group *grent;

	uid = getuid();
	if (uid == 0) {
		if (!job_manifest->user_name)
			job_manifest->user_name = strdup("root");
		
		if (!job_manifest->group_name)
			job_manifest->group_name = strdup("wheel");
	}
	else {
		if (job_manifest->user_name)
			free(job_manifest->user_name);
		
		if ((pwent = getpwuid(uid)) == NULL)
			return -1;
		
		job_manifest->user_name = strdup(pwent->pw_name);

		if ((grent = getgrgid(pwent->pw_gid)) == NULL)
			return -1;

		if (job_manifest->group_name)
			free(job_manifest->group_name);
		
		job_manifest->group_name = strdup(grent->gr_name);
	}
	
	job_manifest->init_groups = true;
	
	return 0;
}

static int job_manifest_validate(job_manifest_t job_manifest)
{
	if (!job_manifest->label) {
		log_error("job does not have a label");
		return -1;
	}

	if (!job_manifest->program) {
		log_error("job %s does not set `program'", job_manifest->label);
		return -1;
	}

	if (!job_manifest->user_name)
	{
		log_error("job %s does not set `user_name'", job_manifest->label);
		return -1;
	}

	if (!job_manifest->group_name)
	{
		log_error("job %s does not set `group_name'", job_manifest->label);
		return -1;
	}
	
	return 0;
}

job_manifest_t job_manifest_new(void)
{
	job_manifest_t job_manifest;

	job_manifest = calloc(1, sizeof(*job_manifest));
	
	if (!job_manifest)
		return (NULL);
	
	job_manifest->exit_timeout = DEFAULT_EXIT_TIMEOUT;
	job_manifest->throttle_interval = DEFAULT_THROTTLE_INTERVAL;
	job_manifest->init_groups = true;
	
	SLIST_INIT(&job_manifest->sockets);

	return job_manifest;
}

void job_manifest_free(job_manifest_t job_manifest)
{
	struct job_manifest_socket *jms, *jms_tmp;

	if (!job_manifest)
		return;

	free(job_manifest->label);
	free(job_manifest->user_name);
	free(job_manifest->group_name);
	free(job_manifest->working_directory);
	free(job_manifest->root_directory);
	free(job_manifest->stdin_path);
	free(job_manifest->stdout_path);
	free(job_manifest->stderr_path);
	free(job_manifest->jail_name);

	cvec_free(job_manifest->program_arguments);
	cvec_free(job_manifest->watch_paths);
	cvec_free(job_manifest->queue_directories);
	cvec_free(job_manifest->environment_variables);
	
	SLIST_FOREACH_SAFE(jms, &job_manifest->sockets, entry, jms_tmp) {
		SLIST_REMOVE(&job_manifest->sockets, jms, job_manifest_socket, entry);
		job_manifest_socket_free(jms);
	}
	
	free(job_manifest);
}

int job_manifest_read(job_manifest_t job_manifest, const char *infile)
{
	unsigned char *buf = NULL;
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

	return (job_manifest_parse(job_manifest, buf, bufsz));

 err_out:
	if (f) (void)fclose(f);
	free(buf);
	return (-1);
}

/* NOTE: buf will be owned by job_manifest_t for free() purposes */
static int job_manifest_parse(job_manifest_t job_manifest, unsigned char *buf, size_t bufsize)
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

	while ((tmp = ucl_iterate_object (obj, &it, true)))
	{
		log_debug("parsing key `%s'", ucl_object_key(tmp));
		for (job_manifest_item_parser_t *item_parser = manifest_parser_map; item_parser->key != NULL; item_parser++)
		{
			if (!strcmp(item_parser->key, ucl_object_key(tmp)))
			{
				if (!job_manifest_object_is_type(tmp, item_parser->object_type))
				{
					log_error("failed while validating key `%s' with value: `%s'", ucl_object_key(tmp), ucl_object_tostring_forced(tmp));
					rc = -1;
					goto cleanup;
				}
				
				log_debug("parsing value `%s'", ucl_object_tostring_forced(tmp));
				if (item_parser->parser(job_manifest, tmp))
				{
					log_error("failed while parsing key `%s' with value: `%s'", ucl_object_key(tmp), ucl_object_tostring_forced(tmp));
					rc = -1;
					goto cleanup;
				}
				break;
			}
		}
	}

	if (job_manifest_set_credentials(job_manifest)) {
		log_error("unable to set credentials on job %s", job_manifest->label ? job_manifest->label : "unknown");
		rc = -1;
		goto cleanup;
	}

	if (job_manifest_validate(job_manifest)) {
		log_error("job %s failed validation", job_manifest->label ? job_manifest->label : "unknown");
		rc = -1;
		goto cleanup;
	}

cleanup:
    
	if (parser)
		ucl_parser_free(parser);

	if (obj)
		ucl_object_unref(obj);

	return rc;
}

void job_manifest_retain(job_manifest_t job_manifest)
{
	job_manifest->refcount++;
}

void job_manifest_release(job_manifest_t job_manifest)
{
	job_manifest->refcount--;
	if (job_manifest->refcount < 0)
		job_manifest_free(job_manifest);
}
