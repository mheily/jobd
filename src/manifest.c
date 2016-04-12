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

typedef struct job_manifest_socket_parser {
	const char *key;
	ucl_type_t object_type;
	int (*parser)(struct job_manifest_socket *socket, const ucl_object_t *object);
} job_manifest_socket_parser_t;

typedef struct job_manifest_item_parser {
	const char *key;
	ucl_type_t object_type;
	int (*parser)(job_manifest_t job_manifest, const ucl_object_t *object);
} job_manifest_item_parser_t;

static bool job_manifest_object_is_type(const ucl_object_t *obj, ucl_type_t object_type);
static unsigned char* job_manifest_prepare_buf_for_file(const char *filename, size_t *buf_size);
static int job_manifest_read_from_file(unsigned char *buf, size_t buf_size, const char *filename);
static ucl_object_t* job_manifest_get_object(unsigned char *buf, size_t buf_size);

static int job_manifest_parse_child(job_manifest_t job_manifest, const ucl_object_t *tmp);
static int job_manifest_parse_label(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_user_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_group_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_program(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_jail_name(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_umask(job_manifest_t manifest, const ucl_object_t *obj);
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
static int job_manifest_parse_keepalive(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_program_arguments(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_watch_paths(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_queue_directories(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_start_interval(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_throttle_interval(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_cvec(cvec_t *dst, const ucl_object_t *obj);
static int job_manifest_parse_sockets(job_manifest_t manifest, const ucl_object_t *obj);
static int job_manifest_parse_sock_service_name(struct job_manifest_socket *socket, const ucl_object_t *object);
static int job_manifest_parse_start_calendar_interval(job_manifest_t manifest, const ucl_object_t *obj);

static const job_manifest_item_parser_t manifest_parser_map[] = {
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
	{ "StartCalendarInterval", UCL_OBJECT,  job_manifest_parse_start_calendar_interval },
	{ "Umask",                 UCL_STRING,  job_manifest_parse_umask },
	{ "KeepAlive",               UCL_BOOLEAN,  job_manifest_parse_keepalive },
	{ "ThrottleInterval",      UCL_INT,   job_manifest_parse_throttle_interval },
	/*
	{ "inetdCompatibility",    SKIP_ITEM,   NULL },
	{ "TimeOut",               SKIP_ITEM,   NULL },
	{ "ExitTimeOut",           SKIP_ITEM,   NULL },
	{ "Disabled",              SKIP_ITEM,   NULL },
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

static const job_manifest_socket_parser_t socket_parser_map[] = {
	{ "SockServiceName",       UCL_STRING,  job_manifest_parse_sock_service_name },
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

	if (*dst)
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

static int job_manifest_parse_umask(job_manifest_t manifest, const ucl_object_t *obj)
{
	int result;

	result = sscanf(ucl_object_tostring(obj), "%hi", (unsigned short *) &manifest->umask);
	return (result >= 0) ? 0 : -1;
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

static int job_manifest_parse_throttle_interval(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->throttle_interval = ucl_object_toint(obj);
	return 0;
}

/** Parse a field within a crontab(5) specification */
static int
parse_cron_field(uint32_t *dst, const ucl_object_t *obj, const char *key, int64_t start, int64_t end)
{
	const ucl_object_t *cur;
	int64_t val;

	if ((cur = ucl_object_find_key(obj, key))) {
		if (!ucl_object_toint_safe(cur, &val)) {
			log_error("wrong type for %s; expected integer", key);
			return -1;
		}
		if (val < start || val > end) {
			log_error("illegal value for %s; expecting %ld-%ld but got %ld",
					key, (long)start, (long)end, (long)val);
			return -1;
		}
		*dst = val;
		/*log_debug("%s=%ld", key, val);*/
	} else {
		*dst = CRON_SPEC_WILDCARD;
	}

	return 0;
}

static int job_manifest_parse_start_calendar_interval(job_manifest_t manifest, const ucl_object_t *obj)
{
	struct cron_spec cron;

	if (parse_cron_field(&cron.minute, obj, "Minute", 0, 59) < 0)
		return -1;

	if (parse_cron_field(&cron.hour, obj, "Hour", 0, 23) < 0)
		return -1;

	if (parse_cron_field(&cron.day, obj, "Day", 1, 31) < 0)
		return -1;

	if (parse_cron_field(&cron.weekday, obj, "Weekday", 0, 7) < 0)
		return -1;

	if (parse_cron_field(&cron.month, obj, "Month", 1, 12) < 0)
		return -1;

	/* Normalize Sunday to always be 0 */
	if (cron.weekday == 7)
		cron.weekday = 0;

	manifest->start_calendar_interval = true;
	manifest->calendar_interval = cron;

	return 0;
}

static int job_manifest_parse_sockets(job_manifest_t manifest, const ucl_object_t *obj)
{
	struct job_manifest_socket *socket = NULL;
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur;

	/* Iterate over the object */
	while ((obj = ucl_iterate_object (obj, &it, true))) {
		socket = job_manifest_socket_new();
		socket->label = strdup(ucl_object_key(obj));

		/* Iterate over the values of a key */
		while ((cur = ucl_iterate_object (obj, &it_obj, true))) {
			for (const job_manifest_socket_parser_t *socket_parser = socket_parser_map; socket_parser->key != NULL; socket_parser++)
			{
				log_debug("parsing socket value `%s'", ucl_object_tostring_forced(cur));
				if (!job_manifest_object_is_type(cur, socket_parser->object_type))
				{
					log_error("object type mismatch while parsing sockets");
					job_manifest_socket_free(socket);
					return -1;
				}

				if (socket_parser->parser(socket, cur))
				{
					log_error("failed to parse socket child");
					job_manifest_socket_free(socket);
					return -1;
				}
			}
		}

		SLIST_INSERT_HEAD(&manifest->sockets, socket, entry);
	}
	return 0;
}

static int job_manifest_parse_sock_service_name(struct job_manifest_socket *socket, const ucl_object_t *object)
{
	return (socket->sock_service_name = strdup(ucl_object_tostring(object))) ? 0 : -1;
}

/* TODO: check the return value of strdup() so we don't crash elsewhere in the program 
  if memory allocation fails */
static int job_manifest_rectify(job_manifest_t job_manifest)
{
	int i, retval = -1;
	cvec_t new_argv = NULL;
	uid_t uid;
	struct passwd *pwent;
	struct group *grent;

	/* Undocumented heuristic to decide if it is an agent:
	 *  - agents cannot set the User property
	 *  - daemons must set the User and Group property
	 */
	job_manifest->job_is_agent = (!job_manifest->user_name && !job_manifest->group_name);

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

	/* If both Program and ProgramArguments are used, consolidate them into one array */
	if (job_manifest->program && job_manifest->program_arguments) {
		new_argv = cvec_new();
		if (!new_argv) goto out;
		if (cvec_push(new_argv, job_manifest->program) < 0) goto out;
		for (i = 0; i < cvec_length(job_manifest->program_arguments); i++) {
			if (cvec_push(new_argv, cvec_get(job_manifest->program_arguments, i)) < 0) goto out;
		}
		cvec_free(job_manifest->program_arguments);
		job_manifest->program_arguments = new_argv;
		new_argv = NULL;
	}

	/* By convention, argv[0] == Program */
	if (job_manifest->program && !job_manifest->program_arguments) {
		job_manifest->program_arguments = cvec_new();
		if (!job_manifest->program_arguments) goto out;
		if (cvec_push(job_manifest->program_arguments, job_manifest->program) < 0) goto out;
	}

	/* By default, redirect standard I/O to /dev/null */
	if (!job_manifest->stdin_path)
		job_manifest->stdin_path = strdup("/dev/null");
	if (!job_manifest->stdout_path)
		job_manifest->stdin_path = strdup("/dev/null");
	if (!job_manifest->stderr_path)
		job_manifest->stdin_path = strdup("/dev/null");

	job_manifest->init_groups = true;

	retval = 0;

out:
	free(new_argv);
	return retval;
}

static int job_manifest_parse_keepalive(job_manifest_t manifest, const ucl_object_t *obj)
{
	manifest->keep_alive.always = ucl_object_toboolean(obj);
	return 0;
}

static int job_manifest_validate(job_manifest_t job_manifest)
{
	if (!job_manifest->label) {
		log_error("job does not have a label");
		return -1;
	}

	/* Require Program or ProgramArguments */
	if (!job_manifest->program && !job_manifest->program_arguments) {
		log_error("job does not set Program or ProgramArguments");
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

	if (job_manifest->start_calendar_interval && job_manifest->start_interval)
	{
		log_error("job %s has both a calendar and a non-calendar interval",
			job_manifest->label);
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
	job_manifest->umask = S_IWGRP | S_IWOTH;
	/* Implicitly set via calloc():
	 job_manifest->keepalive.always = false;
	*/

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

static unsigned char* job_manifest_prepare_buf_for_file(const char *filename, size_t *buf_size)
{
	unsigned char *buf;
	struct stat sb;

	if (stat(filename, &sb))
		return NULL;

	/* WORKAROUND: allocate (buf_size + 1) to prevent future off-by-one errors
	 * reported by Valgrind. It would be better to fix this someplace else.
	 */
	*buf_size = sb.st_size + 1;
	buf = calloc(*buf_size + 1, sizeof(unsigned char));

	return buf;
}

static int job_manifest_read_from_file(unsigned char *buf, size_t buf_size, const char *filename)
{
	int rc = 0;
	FILE *f = NULL;

	if (!(f = fopen(filename, "r")))
		return -1;

	if (fread(buf, 1, buf_size - 1, f) != buf_size - 1)
		rc = -1;

	buf[buf_size] = 0;

	fclose(f);
	return rc;
}

int job_manifest_read(job_manifest_t job_manifest, const char *filename)
{
	unsigned char *buf = NULL;
	size_t buf_size;
	int rc = 0;

	if (!(buf = job_manifest_prepare_buf_for_file(filename, &buf_size)))
		return -1;

	if (job_manifest_read_from_file(buf, buf_size, filename)) {
		free(buf);
		return -1;
	}

	if (!rc)
		rc = job_manifest_parse(job_manifest, buf, buf_size);

	free(buf);

	return rc;
}

static ucl_object_t* job_manifest_get_object(unsigned char *buf, size_t buf_size)
{
	struct ucl_parser *parser = NULL;
	ucl_object_t *obj = NULL;

	parser = ucl_parser_new(0);

	if (!parser)
		return NULL;

	ucl_parser_add_chunk(parser, buf, buf_size);

	if (ucl_parser_get_error(parser))
		log_error("%s", ucl_parser_get_error(parser));
	else
		obj = ucl_parser_get_object(parser);

	ucl_parser_free(parser);

	return obj;
}

static int job_manifest_parse_child(job_manifest_t job_manifest, const ucl_object_t *tmp)
{
	int rc = 0;
	log_debug("parsing key `%s'", ucl_object_key(tmp));
	for (const job_manifest_item_parser_t *item_parser = manifest_parser_map; item_parser->key != NULL; item_parser++) {
		if (!strcmp(item_parser->key, ucl_object_key(tmp))) {
			if (!job_manifest_object_is_type(tmp, item_parser->object_type)) {
				log_error("failed while validating key `%s' with value: `%s'", ucl_object_key(tmp), ucl_object_tostring_forced(tmp));
				rc = -1;
			}
			else {
				log_debug("parsing value `%s'", ucl_object_tostring_forced(tmp));
				if (item_parser->parser(job_manifest, tmp)) {
					log_error("failed while parsing key `%s' with value: `%s'", ucl_object_key(tmp), ucl_object_tostring_forced(tmp));
					rc = -1;
				}
			}
			break;
		}
	}
	return rc;
}

int job_manifest_parse(job_manifest_t job_manifest, unsigned char *buf, size_t buf_size)
{
	int rc = 0;
	const ucl_object_t *tmp = NULL;
	ucl_object_iter_t it = NULL;
	ucl_object_t *obj;

	if (!(obj = job_manifest_get_object(buf, buf_size)))
		return -1;

	while ((tmp = ucl_iterate_object (obj, &it, true)))
		if ((rc = job_manifest_parse_child(job_manifest, tmp)))
			break;

	ucl_object_unref(obj);

	if (rc)
		return rc;

	if (job_manifest_rectify(job_manifest)) {
		log_error("unable to rectify job %s", job_manifest->label ? job_manifest->label : "unknown");
		return -1;
	}

	if (job_manifest_validate(job_manifest)) {
		log_error("job %s failed validation", job_manifest->label ? job_manifest->label : "unknown");
		return -1;
	}

	return rc;
}
