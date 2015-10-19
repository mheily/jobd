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

#include "jsmn/jsmn.h"
#include "manifest.h"
#include "cvec.h"
#include "log.h"

#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

/* The internal state of the parser */
struct parser_state {
	job_manifest_t	jm;	/* The job manifest we are building */
	char *buf;		/* The entire buffer */
	jsmntok_t *tok;	/* An array of tokens */
	size_t	pos;	/* The current position within the array */
	char	*key;	/* The current key in the key/value pair we are parsing */
	//TODO: pointer to the current grammar in use, for when we do subparsing
};
typedef struct parser_state *parser_state_t;

static void token_dump(jsmntok_t tok) {
	log_debug("token info: type=%d size=%d", tok.type, tok.size);
}

static int parse_string(char **dst, parser_state_t p) {
	char *buf = NULL;

	if (p->tok[p->pos].type != JSMN_STRING) return -1;
	buf = strndup(p->buf + p->tok[p->pos].start, p->tok[p->pos].end - p->tok[p->pos].start);
	if (buf == NULL) return -1;
	log_debug("parsed %s => %s", p->key, buf);
	*dst = buf;
	return 1;
}

static int parse_bool(bool *dst, parser_state_t p) {
	if (p->tok[p->pos].type != JSMN_PRIMITIVE) return -1;

	switch (p->buf[p->tok[p->pos].start]) {
		case 't': *dst = true; break;
		case 'f': *dst = false; break;
		default: log_error("bad boolean value for %s", p->key); return -1;
	};
	log_debug("parsed %s => %s", p->key, *dst ? "true" : "false");
	return (1);
}

static int parse_cvec(cvec_t *dst, parser_state_t p) {
	int i;
	jsmntok_t child;
	char *item = NULL;
	cvec_t cv;

	cv = cvec_new();
	if (!cv) return -1;

	if (p->tok[p->pos].type != JSMN_ARRAY) goto err_out;
	log_debug("got %d child tokens", p->tok[p->pos].size);
	for (i = 0; i < p->tok[p->pos].size; i++) {
		child = p->tok[p->pos + i + 1];
		if (child.type != JSMN_STRING) goto err_out;
		item = strndup(p->buf + child.start, child.end - child.start);
		if (item == NULL) goto err_out;
		log_debug("parsed item: %s", item);
		if (cvec_push(cv, item) < 0) goto err_out;
		item = NULL;
	}
	*dst = cv;
	return (p->tok[p->pos].size + 1);

err_out:
	free(item);
  	cvec_free(cv);
  	return (-1);
}

static int parse_not_implemented(parser_state_t p) {
	log_error("feature %s not implemented", p->key);
	return -1;
}

static int parse_label(parser_state_t p) {
	if (parse_string(&p->jm->label, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_user_name(parser_state_t p) {
	if (parse_string(&p->jm->user_name, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_program(parser_state_t p) {
	if (parse_string(&p->jm->program, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_ProgramArguments(parser_state_t p) {
	return parse_cvec(&p->jm->program_arguments, p);
}

static int parse_WatchPaths(parser_state_t p) {
	return parse_cvec(&p->jm->watch_paths, p);
}

static int parse_QueueDirectories(parser_state_t p) {
	return parse_cvec(&p->jm->queue_directories, p);
}

static int parse_group_name(parser_state_t p) {
	if (parse_string(&p->jm->group_name, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_EnableGlobbing(parser_state_t p) {
	return parse_bool(&p->jm->enable_globbing, p);
}

static int parse_RunAtLoad(parser_state_t p) {
	return parse_bool(&p->jm->run_at_load, p);
}

// XXX seems very illogical, perhaps the test.json file is malformed?
static int parse_EnvironmentVariables(parser_state_t p) {
	int i;
	size_t tokens_left;
	jsmntok_t child;
	char *item = NULL;
	cvec_t cv;

	cv = cvec_new();
	if (!cv) return -1;

	token_dump(p->tok[p->pos]);
	if (p->tok[p->pos].type != JSMN_OBJECT) goto err_out;
	tokens_left = p->tok[p->pos].size;
	log_debug("got %zu child tokens", tokens_left);
	for (i = 0; i < tokens_left; i += 2) {
		log_debug(" -- hash token");
		child = p->tok[p->pos + i + 1];
		token_dump(child);
		if (child.type != JSMN_STRING || child.size != 1) goto err_out;
		tokens_left += 1; //XXX-HORRIBLE
		item = strndup(p->buf + child.start, child.end - child.start);
		if (item == NULL) goto err_out;
		log_debug("parsed item: %s", item);
		if (cvec_push(cv, item) < 0) goto err_out;
		item = NULL;

		// XXX-COPY PASTA FROM ABOVE STANZA
		child = p->tok[p->pos + i + 2];
		token_dump(child);
		if (child.type != JSMN_STRING || child.size != 0) goto err_out;
		item = strndup(p->buf + child.start, child.end - child.start);
		if (item == NULL) goto err_out;
		log_debug("parsed item: %s", item);
		if (cvec_push(cv, item) < 0) goto err_out;
		item = NULL;
	}
	p->jm->environment_variables = cv;

	return (p->tok[p->pos].size + tokens_left + 1);

err_out:
	free(item);
  	cvec_free(cv);
  	return (-1);
}

static int parse_WorkingDirectory(parser_state_t p) {
	if (parse_string(&p->jm->working_directory, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_RootDirectory(parser_state_t p) {
	if (parse_string(&p->jm->root_directory, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_InitGroups(parser_state_t p) {
	return parse_bool(&p->jm->init_groups, p);
}

static int parse_StandardInPath(parser_state_t p) {
	if (parse_string(&p->jm->stdin_path, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_StandardOutPath(parser_state_t p) {
	if (parse_string(&p->jm->stdout_path, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static int parse_StandardErrorPath(parser_state_t p) {
	if (parse_string(&p->jm->stderr_path, p) < 1) return -1;
	//TODO: validation
	return (1);
}

static const struct {
        const char *ident;
        int (*func)(parser_state_t);
} manifest_parser[] = {
        { "Label", parse_label },
		{ "Disabled", parse_not_implemented },
		{ "UserName", parse_user_name },
		{ "GroupName", parse_group_name },
		{ "inetdCompatibility", parse_not_implemented },
		{ "Program", parse_program },
		{ "ProgramArguments", parse_ProgramArguments },
		{ "EnableGlobbing", parse_EnableGlobbing },
		{ "KeepAlive", parse_not_implemented },
		{ "RunAtLoad", parse_RunAtLoad },
		{ "WorkingDirectory", parse_WorkingDirectory },
		{ "RootDirectory", parse_RootDirectory },
		{ "EnvironmentVariables", parse_EnvironmentVariables },
		{ "Umask", parse_not_implemented },
		{ "TimeOut", parse_not_implemented },
		{ "ExitTimeOut", parse_not_implemented },
		{ "ThrottleInterval", parse_not_implemented },
		{ "InitGroups", parse_InitGroups },
		{ "WatchPaths", parse_WatchPaths },
		{ "QueueDirectories", parse_QueueDirectories },
		{ "StartOnMount", parse_not_implemented },
		{ "StartInterval", parse_not_implemented },
		{ "StartCalendarInterval", parse_not_implemented },
		{ "StandardInPath", parse_StandardInPath },
		{ "StandardOutPath", parse_StandardOutPath },
		{ "StandardErrorPath", parse_StandardErrorPath },
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
		{ "Sockets", parse_not_implemented },

		{ NULL, NULL },
};

/* Validate the semantic correctness of the manifest */
static int job_manifest_validate(job_manifest_t jm)
{
	size_t i;
	cvec_t new_argv = NULL;

	/* Require Program or ProgramArguments */
	if (!jm->program && cvec_length(jm->program_arguments) == 0) {
		log_error("job %s does not set Program or ProgramArguments", jm->label);
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
	return (0);

err_out:
	free(new_argv);
	return (-1);
}

job_manifest_t job_manifest_new()
{
	job_manifest_t jm;
	char *argv0 = NULL;

	jm = calloc(1, sizeof(*jm));
	if (jm == NULL) return (NULL);
	jm->exit_timeout = 20;
	jm->throttle_interval = 10;
	jm->init_groups = true;
	if ((jm->program_arguments = cvec_new()) == NULL) goto err_out;
	if ((jm->watch_paths = cvec_new()) == NULL) goto err_out;
	if ((jm->queue_directories = cvec_new()) == NULL) goto err_out;
	if ((jm->environment_variables = cvec_new()) == NULL) goto err_out;

	return (jm);

err_out:
	job_manifest_free(jm);
	return (NULL);
}

void job_manifest_free(job_manifest_t jm)
{
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
int job_manifest_parse(job_manifest_t jm, char *buf, size_t bufsz)
{
    jsmn_parser p;
    //jsmntok_t *tok;
    const size_t tokcount = 500;
    size_t keylen;
    int i, j, rv, parse_status, eaten;
    bool match;
    struct parser_state state;

	if (jm == NULL || buf == NULL || bufsz == 0)
		return -1;

	state.buf = buf;
	state.jm = jm;
	state.key = NULL;
	state.pos = 0;
	jm->json_buf = buf;
	jm->json_size = bufsz;

    jsmn_init(&p);
    state.tok = malloc(sizeof(*(state.tok)) * tokcount);
    if (state.tok == NULL) goto err_out;
    rv = jsmn_parse(&p, buf, bufsz - 1, state.tok, tokcount);
    if (rv < 0) goto err_out;

    /* Verify there is at least one object */
    if (rv < 1 || state.tok[0].type != JSMN_OBJECT) goto err_out;

    /* Verify there is a Key->Value token combo */
    for (state.pos = 1; state.pos < rv;) {
    	if (state.tok[state.pos].type != JSMN_STRING) goto err_out;
    	keylen = state.tok->end - state.tok->start;

    	if (state.pos + 1 == rv) goto err_out;
    	state.key = strndup(buf + state.tok[state.pos].start, state.tok[state.pos].end - state.tok[state.pos].start);
    	if (state.key == NULL) goto err_out;
    	state.pos++;

		/* Parse the key/value pair */
    	match = false;
		for (j = 0; manifest_parser[j].ident != NULL; j++) {
			 if (strncmp(state.key, manifest_parser[j].ident, keylen) == 0) {
				  eaten = manifest_parser[j].func(&state);
				  if (eaten < 1) {
					 log_error("failed to parse %s", state.key);
					 goto err_out;
				 } else {
					 state.pos += eaten;
				 }
				 free(state.key);
				 match = true;
				 break;
			 }
		}
		if (!match) { log_error("unsupported key: %s", state.key); goto err_out; }
    }
    if (job_manifest_validate(jm) < 0) goto err_out;

	return 0;

err_out:
	free(state.key);
	free(jm->json_buf);
	jm->json_buf = NULL;
	jm->json_size = bufsz;
	return -1;
}
