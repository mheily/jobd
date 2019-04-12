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
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>

#include "database.h"
#include "logger.h"
#include "memory.h"
#include "job.h"
#include "parser.h"

struct child_context {
    char *working_directory;
    char *root_directory;
    int init_groups;
    char *user_name;
    char *group_name;
    char *stderr_path;
    char *stdin_path;
    char *stdout_path;
    char *umask_str;
};

static int
get_child_context(struct child_context *ctx, int64_t jid)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char sql[] = "SELECT working_directory, root_directory, init_groups, "
                       "user_name, gid, "
                       "standard_error_path, standard_in_path, standard_out_path, "
                       "umask "
                       "FROM jobs WHERE id = ?";

    if (db_query(&stmt, sql, "i", jid) < 0)
        return db_error;

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            break;
        case SQLITE_DONE:
            return printlog(LOG_ERR, "job no longer exists");
        default:
            return db_error;
    }

    // FIXME: need to error check strdup() calls
    ctx->working_directory = strdup((char *) sqlite3_column_text(stmt, 0));
    ctx->root_directory = strdup((char *) sqlite3_column_text(stmt, 1));
    ctx->init_groups = sqlite3_column_int(stmt, 2);
    ctx->user_name = strdup((char *) sqlite3_column_text(stmt, 3));
    ctx->group_name = strdup((char *) sqlite3_column_text(stmt, 4));
    ctx->stderr_path = strdup((char *) sqlite3_column_text(stmt, 5));
    ctx->stdin_path = strdup((char *) sqlite3_column_text(stmt, 6));
    ctx->stdout_path = strdup((char *) sqlite3_column_text(stmt, 7));
    ctx->umask_str = strdup((char *) sqlite3_column_text(stmt, 8));

    return 0;
}

static void free_child_context(struct child_context *ctx)
{
    if (ctx) {
        free(ctx->working_directory);
        free(ctx->root_directory);
        free(ctx->user_name);
        free(ctx->group_name);
        free(ctx->stderr_path);
        free(ctx->stdin_path);
        free(ctx->stdout_path);
        free(ctx->umask_str);
        free(ctx);
    }
}

#define CLEANUP_CHILD_CTX __attribute__((__cleanup__(autofree_child_context)))
void autofree_child_context(struct child_context **ctx)
{
    free_child_context(*ctx);
    *ctx = NULL;
}

static int
redirect_file_descriptor(int oldfd, const char *path, int flags, mode_t mode)
{
    int newfd;

    newfd = open(path, flags, mode);
    if (newfd < 0) {
        printlog(LOG_ERR, "open(2) of %s: %s", path, strerror(errno));
        return (-1);
    }
    if (dup2(newfd, oldfd) < 0) {
        printlog(LOG_ERR, "dup2(2): %s", strerror(errno));
        (void) close(newfd);
        return (-1);
    }
    if (close(newfd) < 0) {
        printlog(LOG_ERR, "close(2): %s", strerror(errno));
        return (-1);
    }

    return (0);
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

/* Run actions in the child after fork(2) but before execve(2) */
static int
_job_child_pre_exec(struct child_context *ctx)
{
    gid_t gid;
    sigset_t mask;

    if (parse_gid(&gid, ctx->group_name) < 0)
        return printlog(LOG_ERR, "unable to resolve group name `%s'", ctx->group_name);

    (void) setsid();
    sigfillset(&mask);
    (void) sigprocmask(SIG_UNBLOCK, &mask, NULL);

    //TODO: setrlimit

    if (getuid() == 0) {
        if (strcmp(ctx->root_directory, "/") && (chroot(ctx->root_directory) < 0))
            return printlog(LOG_ERR, "chroot(2) to %s: %s", ctx->root_directory, strerror(errno));
    }
    if (chdir(ctx->working_directory) < 0)
        return printlog(LOG_ERR, "chdir(2) to %s: %s", ctx->working_directory, strerror(errno));
    if (getuid() == 0) {
        if (ctx->init_groups && (initgroups(ctx->user_name, gid) < 0))
            return printlog(LOG_ERR, "initgroups(3): %s", strerror(errno));
        if (setgid(gid) < 0)
            return printlog(LOG_ERR, "setgid(2): %s", strerror(errno));
#ifndef __GLIBC__
        /* KLUDGE: above is actually a test for BSD */
        if (setlogin(user_name) < 0) {
            printlog(LOG_ERR, "setlogin(2): %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
#endif
        struct passwd *pwd = getpwnam(ctx->user_name);
        if (!pwd)
            return printlog(LOG_ERR, "user not found: %s", ctx->user_name);
        if (setuid(pwd->pw_uid) < 0)
            return printlog(LOG_ERR, "setuid(2): %s", strerror(errno));
    }

    mode_t job_umask;
    sscanf(ctx->umask_str, "%hi", (unsigned short *) &job_umask);
    (void) umask(job_umask);

    //TODO this->setup_environment();
    //this->createDescriptors();

    if (redirect_file_descriptor(STDIN_FILENO, ctx->stdin_path, O_RDONLY, 0600) < 0)
        return printlog(LOG_ERR, "unable to redirect STDIN");
    if (redirect_file_descriptor(STDOUT_FILENO, ctx->stdout_path, O_CREAT | O_WRONLY, 0600) < 0)
        return printlog(LOG_ERR, "unable to redirect STDOUT");
    if (redirect_file_descriptor(STDERR_FILENO, ctx->stderr_path, O_CREAT | O_WRONLY, 0600) < 0)
        return printlog(LOG_ERR, "unable to redirect STDERR");

    return 0;
}

static int
job_command_exec(pid_t *child, job_id_t id, const char *command)
{
    pid_t pid;
    char *filename;
    char exec_command[JOB_ARG_MAX + sizeof("exec ")];
    char *argv[5];
    static char *envp_workaround[] = {NULL};
    char **envp;
    ssize_t rv;

    *child = 0;

    rv = snprintf((char *) &exec_command, sizeof(exec_command), "exec %s", command);
    if (rv < 0 || rv >= (int) sizeof(exec_command)) {
        printlog(LOG_ERR, "buffer too small");
        return (-1);
    }

    filename = "/bin/sh";
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = exec_command;
    argv[3] = NULL;
    envp = (char **) &envp_workaround; //string_array_data(job->environment_variables);

    struct child_context CLEANUP_CHILD_CTX *ctx = NULL;
    if (NULL == (ctx = malloc(sizeof(*ctx))))
        return printlog(LOG_ERR, "malloc(3): %s", strerror(errno));
    if (get_child_context(ctx, id) < 0)
        return printlog(LOG_ERR, "error getting child context");

    pid = fork();
    if (pid < 0) {
        printlog(LOG_ERR, "fork(2): %s", strerror(errno));
        return (-1);
    } else if (pid == 0) {
        if (_job_child_pre_exec(ctx) < 0) {
            printlog(LOG_ERR, "error setting child context");
            exit(EXIT_FAILURE);
        }
        if (execve(filename, argv, envp) < 0) {
            printlog(LOG_ERR, "execve(2): %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        /* NOTREACHED */
    } else {
        printlog(LOG_DEBUG, "job `%s': executing command as pid %d: %s", job_id_to_str(id), pid, exec_command);
        *child = pid;
    }

    return (0);
}

static int job_script_exec(pid_t *child, job_id_t jid, char *script);

int
job_method_exec(pid_t *child, job_id_t jid, const char *method_name)
{
    char *script;
    if (job_get_method(&script, jid, method_name) < 0)
        return -1;
    if (!script) {
        printlog(LOG_DEBUG, "job `%s': method not found: `%s'", job_id_to_str(jid), method_name);
        return 0;
    }
    printlog(LOG_DEBUG, "job `%s': invoking method `%s'", job_id_to_str(jid), method_name);
    int result = job_script_exec(child, jid, script);
    free(script);
    return result;
}

static int
job_script_exec(pid_t *child, job_id_t jid, char *script)
{
    const char **empty_envp = {NULL};
    pid_t pid;
    char *filename = NULL;
    char *argv[5];
    char **envp;

    *child = 0;

    struct child_context CLEANUP_CHILD_CTX *ctx = NULL;
    if (NULL == (ctx = malloc(sizeof(*ctx))))
        return printlog(LOG_ERR, "malloc(3): %s", strerror(errno));
    if (get_child_context(ctx, jid) < 0)
        return printlog(LOG_ERR, "error getting child context");

    filename = "/bin/sh";
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = script;
    argv[3] = NULL;
    envp = (char **) empty_envp; //XXX-FIXME string_array_data(job->environment_variables);

    pid = fork();
    if (pid < 0)
        return printlog(LOG_ERR, "fork(2): %s", strerror(errno));

    if (pid == 0) {
        if (_job_child_pre_exec(ctx) < 0) {
            printlog(LOG_ERR, "error setting child context");
            exit(EXIT_FAILURE);
        }
        if (execve(filename, argv, envp) < 0) {
            printlog(LOG_ERR, "execve(2): %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        /* NOTREACHED */
    } else {
        printlog(LOG_DEBUG, "job `%s': child pid %d is running", job_id_to_str(jid), pid);
        *child = pid;
    }

    return 0;
}

const char *job_state_to_str(enum job_state state)
{
    switch (state) {
        case JOB_STATE_UNKNOWN:
            return ("unknown");
        case JOB_STATE_DISABLED:
            return ("disabled");
        case JOB_STATE_PENDING:
            return ("pending");
        case JOB_STATE_STARTING:
            return ("starting");
        case JOB_STATE_RUNNING:
            return ("running");
        case JOB_STATE_STOPPING:
            return ("stopping");
        case JOB_STATE_STOPPED:
            return ("stopped");
        case JOB_STATE_COMPLETE:
            return ("complete");
        case JOB_STATE_ERROR:
            return ("error");
        default:
            return ("invalid_state");
    }
}

int
job_start(pid_t *pid, job_id_t id)
{
    enum job_state state;
    char command[JOB_ARG_MAX];

    if (job_get_state(&state, id) < 0) {
        printlog(LOG_ERR, "unable to get job state");
        return (-1);
    }
    if (job_get_command(command, id) < 0) {
        printlog(LOG_ERR, "unable to get command");
        return (-1);
    }

    printlog(LOG_DEBUG, "job `%s' current_state=%s next_state=starting", job_id_to_str(id), job_state_to_str(state));

    if (state != JOB_STATE_PENDING && state != JOB_STATE_STOPPED) {
        printlog(LOG_ERR, "job is in the wrong state to be started: %s", job_state_to_str(state));
        return (-1);
    }

    if (command[0] != '\0') {
        if (job_command_exec(pid, id, command) < 0) {
            printlog(LOG_ERR, "start command failed");
            return (-1);
        }
    } else {
        if (job_method_exec(pid, id, "start") < 0) {
            printlog(LOG_ERR, "start method failed");
            return (-1);
        }
    }

    /*
        job->state = JOB_STATE_STARTING;

        TODO: allow the use of a check script that waits for the service to finish initializing.
    */
    if (*pid > 0) {
        if (job_set_state(id, JOB_STATE_RUNNING) < 0)
            return printlog(LOG_ERR, "unable to set state");

        printlog(LOG_DEBUG, "job %s started with pid %d", job_id_to_str(id), *pid);
        if (job_register_pid(id, *pid) < 0)
            return printlog(LOG_ERR, "unable to register pid");
    }

    return (0);
}

int
job_stop(job_id_t id)
{
    pid_t pid, job_pid;
    enum job_state state;

    if (job_get_state(&state, id) < 0)
        return printlog(LOG_ERR, "state lookup failed");
    if (job_get_pid(&job_pid, id) < 0)
        return printlog(LOG_ERR, "pid lookup failed");

    printlog(LOG_DEBUG, "job `%s' current_state=%s next_state=stopping pid=%d",
            job_id_to_str(id), job_state_to_str(state), job_pid);

    if (state == JOB_STATE_DISABLED) {
        printlog(LOG_DEBUG, "job %s is disabled; stopping has no effect", job_id_to_str(id));
        return (0);
    }

    if (state == JOB_STATE_STOPPED) {
        printlog(LOG_DEBUG, "job %s is already stopped", job_id_to_str(id));
        return (0);
    }

    if (state != JOB_STATE_RUNNING && state != JOB_STATE_STARTING) {
        printlog(LOG_ERR, "job is in the wrong state to be stopped: %s", job_state_to_str(state));
        return (-1);
    }

    char *script;
    if (job_get_method(&script, id, "stop") < 0)
        return printlog(LOG_ERR, "job_get_method() failed");
    if (script) {
        if (job_script_exec(&pid, id, script) < 0)
            return printlog(LOG_ERR, "stop method failed");
    } else {
        pid = 0;
    }

    if (pid > 0 && (job_pid == 0)) {
        job_pid = pid;
    } else if (!pid && (job_pid > 0)) {
        printlog(LOG_DEBUG, "sending SIGTERM to job %s (pid %d)", job_id_to_str(id), job_pid);
        if (kill(job_pid, SIGTERM) < 0) {
            if (errno == ESRCH) {
                /* Probably a harmless race condition, but note it anyway */
                printlog(LOG_WARNING, "job %s (pid %d): no such process", job_id_to_str(id), job_pid);
                if (job_set_state(id, JOB_STATE_STOPPED) < 0)
                    return (-1);
            } else {
                printlog(LOG_ERR, "kill(2): %s", strerror(errno));
                return (-1);
            }
        }
        if (job_set_state(id, JOB_STATE_STOPPING) < 0)
            return (-1);
    } else {
        /* FIXME: open design question: what about jobs with a PID *and* a stop method? */
    }

    //TODO: start a timeout

    return (0);
}

struct job *
job_new(void)
{
    struct job *j;

    j = calloc(1, sizeof(*j));
    if (!j)
        return (NULL);

    if (!(j->after = string_array_new()))
        goto err_out;
    if (!(j->before = string_array_new()))
        goto err_out;
    if (!(j->environment_variables = string_array_new()))
        goto err_out;

    return (j);

err_out:
    job_free(j);
    return (NULL);
}

void
job_free(struct job *job)
{
    if (job) {
        string_array_free(job->after);
        string_array_free(job->before);
        free(job->command);
        free(job->description);
        string_array_free(job->environment_variables);
        free(job->id);
        free(job->title);
        free(job->root_directory);
        free(job->standard_error_path);
        free(job->standard_in_path);
        free(job->standard_out_path);
        free(job->working_directory);
        free(job->user_name);
        free(job->group_name);
        free(job->umask_str);
        free(job);
    }
}

int job_get_command(char dest[JOB_ARG_MAX], job_id_t jid)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char sql[] = "SELECT command FROM jobs WHERE id = ?";

    dest[0] = '\0';

    if (db_query(&stmt, sql, "i", jid) < 0)
        return printlog(LOG_ERR, "db_query() failed");

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            strncpy(dest, (char *) sqlite3_column_text(stmt, 0), JOB_ARG_MAX - 2);
            dest[JOB_ARG_MAX - 1] = '\0';
            return 0;
        case SQLITE_DONE:
            return -1; //lame
        default:
            return db_error;
    }
}

// caller must free value
int job_get_property(char **value, const char *key, int64_t jid)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "SELECT current_value FROM properties "
                      "WHERE job_id = ? "
                      "AND name = ?";

    if (jid == INVALID_ROW_ID || !key)
        return -1;

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, jid) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            *value = strdup((char *) sqlite3_column_text(stmt, 0));
            return 0;
        case SQLITE_DONE:
            *value = NULL;
            return 0;
        default:
            *value = NULL;
            return db_error;
    }
}

int job_set_property(int64_t jid, const char *key, const char *value)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "UPDATE properties "
                      "SET current_value = ? "
                      "WHERE job_id = ? AND name = ?";

    if (jid == INVALID_ROW_ID || !key || !value)
        return printlog(LOG_ERR, "invalid parameters");

    // XXX-FIXME implement type checking here

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, jid) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, 3, key, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;

    switch (sqlite3_step(stmt)) {
        case SQLITE_DONE:
            if (sqlite3_changes(dbh) == 1)
                return 0;
            else
                return printlog(LOG_ERR, "update had no effect");
        default:
            return db_error;
    }
}

int
job_get_method(char **dest, job_id_t jid, const char *method_name)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "SELECT "
                      "(SELECT group_concat(shellcode, char(10)) "
                      "   FROM properties_view"
                      "   WHERE job_id = ?) || char(10) || script "
                      "FROM job_methods "
                      "WHERE job_id = ? "
                      "AND name = ?";

    if (jid == INVALID_ROW_ID || !method_name)
        return -1;

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, jid) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, jid) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, 3, method_name, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            *dest = strdup((char *) sqlite3_column_text(stmt, 0));
            return 0;
        case SQLITE_DONE:
            *dest = NULL;
            return 0;
        default:
            *dest = NULL;
            return db_error;
    }
}

int
job_enable(job_id_t id)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "UPDATE properties SET current_value = 1 WHERE job_id = ? AND name = 'enabled'";
    enum job_state state;
    pid_t pid;

    if (job_get_state(&state, id) < 0)
        return printlog(LOG_ERR, "unable to get job state");

    if (state == JOB_STATE_PENDING) {
        printlog(LOG_DEBUG, "job is already enabled");
        return 0;
    }

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;
    if (sqlite3_changes(dbh) == 0)
        return printlog(LOG_ERR, "job %s does not exist", job_id_to_str(id));

    if (job_set_state(id, JOB_STATE_PENDING) < 0)
        return -1;

    printlog(LOG_DEBUG, "job %s has been enabled", job_id_to_str(id));
    job_start(&pid, id);
    return 0;
}

int
job_disable(job_id_t id)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    enum job_state state;

    if (job_get_state(&state, id) < 0)
        return printlog(LOG_ERR, "unable to get job state");

    if (state == JOB_STATE_DISABLED) {
        printlog(LOG_DEBUG, "job is already disabled");
        return 0;
    }

    const char *sql = "UPDATE properties SET current_value = 0 WHERE job_id = ? AND name = 'enabled'";
    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, id) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;
    if (sqlite3_changes(dbh) == 0)
        return printlog(LOG_ERR, "job %s does not exist", job_id_to_str(id));

    printlog(LOG_DEBUG, "job %s has been disabled", job_id_to_str(id));
    if (state == JOB_STATE_STARTING ||
        state == JOB_STATE_RUNNING ||
        state == JOB_STATE_STOPPING) {
        job_stop(id);
        //FIXME: this will reset the state to STOPPING, but will we remember to disable it when it terminates?
    }
    return 0;
}

int
job_register_pid(int64_t row_id, pid_t pid)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO processes "
                      " (pid, job_id, start_time) "
                      "VALUES "
                      " (?, ?, ?)";

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, pid) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, row_id) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 3, time(NULL)) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;

    return 0;
}

int
job_get_pid(pid_t *pid, int64_t row_id)
{
    int64_t result;
    const char *sql = "SELECT processes.pid FROM processes WHERE job_id = ?";

    if (db_get_id(&result, sql, "i", row_id) < 0) {
        printlog(LOG_ERR, "database error");
        *pid = 0;
        return (-1);
    }
    if (result == INVALID_ROW_ID) {
        *pid = 0;
        return (1);
    }
    *pid = (pid_t) result;
    return (0);
}

int
job_get_label_by_pid(char label[JOB_ID_MAX], pid_t pid)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "SELECT jobs.job_id "
                      "  FROM jobs "
                      "INNER JOIN processes ON processes.job_id = jobs.id "
                      "  WHERE pid = ?";

    label[0] = '\0';
    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, pid) != SQLITE_OK)
        return db_error;

    int rv = sqlite3_step(stmt);
    if (rv == SQLITE_DONE) {
        label[0] = '\0';
        //FIXME: INDICATE NO RESULT
    } else if (rv == SQLITE_ROW) {
        strncpy(label, (char *) sqlite3_column_text(stmt, 0), JOB_ID_MAX - 1);
        label[JOB_ID_MAX] = '\0';
    } else {
        return db_error;
    }

    return 0;
}

int
job_set_exit_status(pid_t pid, int status)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "UPDATE processes "
                      "SET exited = 1, exit_status = ?, end_time = ?"
                      "WHERE pid = ?";

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, status) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, time(NULL)) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 3, pid) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;

    return 0;
}

int
job_set_signal_status(pid_t pid, int signum)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "UPDATE processes "
                      "SET signaled = 1, signal_number = ?, end_time = ?"
                      "WHERE pid = ?";

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, signum) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, time(NULL)) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 3, pid) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;

    return 0;
}

int job_set_state(int64_t job_id, enum job_state state)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    const char *sql = "UPDATE jobs_current_states "
                      "SET job_state_id = ? "
                      "WHERE job_id = ?";

    if (sqlite3_prepare_v2(dbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 1, state) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_int64(stmt, 2, job_id) != SQLITE_OK)
        return db_error;
    if (sqlite3_step(stmt) != SQLITE_DONE)
        return db_error;
    if (sqlite3_changes(dbh) == 0)
        return printlog(LOG_ERR, "job %s does not exist", job_id_to_str(job_id));

    return 0;
}

int job_get_state(enum job_state *state, job_id_t id)
{
    int64_t result;
    const char *sql = "SELECT job_state_id "
                      " FROM jobs_current_states "
                      "WHERE job_id = ?";

    if (db_get_id(&result, sql, "i", id) < 0) {
        printlog(LOG_ERR, "database error");
        *state = JOB_STATE_UNKNOWN;
        return (-1);
    }
    if (result == INVALID_ROW_ID) {
        printlog(LOG_ERR, "job not found");
        *state = JOB_STATE_UNKNOWN;
        return (1);
    }

    *state = (enum job_state) result;
    return (0);
}

int job_get_type(enum job_type *type, job_id_t id)
{
    int64_t result;
    const char *sql = "SELECT job_type_id FROM jobs WHERE id = ?";

    if (db_get_id(&result, sql, "i", id) < 0) {
        printlog(LOG_ERR, "database error");
        *type = JOB_TYPE_UNKNOWN;
        return (-1);
    }
    if (result == INVALID_ROW_ID) {
        printlog(LOG_ERR, "job not found");
        *type = JOB_TYPE_UNKNOWN;
        return (1);
    }

    *type = (enum job_type) result;
    return (0);
}

const char *
job_id_to_str(job_id_t jid)
{
    const char sql[] = "SELECT job_id FROM jobs WHERE id = ?";
    static char label[JOB_ID_MAX + 1];
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    int rv;

    if (db_query(&stmt, sql, "i", jid) < 0) {
        printlog(LOG_ERR, "db_query() failed");
        strcpy(label, "__error__");
        goto out;
    }

    rv = sqlite3_step(stmt);
    if (rv == SQLITE_DONE) {
        printlog(LOG_ERR, "job no longer exists");
        strcpy(label, "__nonexistent__");
        goto out;
    }
    if (rv != SQLITE_ROW) {
        printlog(LOG_ERR, "sqlite3_step() failed");
        strcpy(label, "__error__");
        goto out;
    }

    /* TODO: prepend with job://, or surround with quotes? */
    rv = snprintf((char *) &label, sizeof(label), "%s", sqlite3_column_text(stmt, 0));
    if (rv >= (int) sizeof(label)) {
        printlog(LOG_ERR, "buffer too small");
        strcpy(label, "__error__");
        goto out;
    }

out:
    label[sizeof(label) - 1] = '\0';
    return ((const char *) &label);
}

int job_get_id(int64_t *jid, const char *label)
{
    const char *sql = "SELECT id FROM jobs WHERE job_id = ?";
    return db_get_id(jid, sql, "s", label);
}

