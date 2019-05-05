/*
 * Copyright (c) 2019 Mark Heily <mark@heily.com>
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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "memory.h"
#include "jsonrpc.h"

static int initialized;
static sqlite3 *memdbh;    /* In-memory database */

#define db_error printlog(LOG_ERR, "database error %d: %s", sqlite3_errcode(memdbh), sqlite3_errmsg(memdbh))

int jsonrpc_init(void)
{
    assert(!initialized);
    if (sqlite3_open(":memory:", &memdbh) != SQLITE_OK) {
        return db_error;
    }
    initialized = 1;
    return 0;
}

void jsonrpc_shutdown(void)
{
    if (initialized) {
        sqlite3_close(memdbh);
        initialized = 0;
    }
}

struct jsonrpc_request *jsonrpc_request_new(const char *id, const char *method, uint32_t nparams, ...)
{
    assert(id);
    assert(method);
    assert(nparams <= IPC_REQUEST_PARAM_MAX);

    struct jsonrpc_request *req = calloc(1, sizeof(*req));
    if (!req)
        return NULL;

    req->id = strdup(id); //XXX ERRORCHECK
    req->method = strdup(method);//XXX ERRORCHECK
    req->nparams = nparams;

    va_list args;
    va_start(args, nparams);
    //TODO: any way to have the compiler help validate these exist?
    for (uint32_t i = 0; i < nparams; i++) {
        char *key = va_arg(args, char *);
        char *val = va_arg(args, char *);
        req->param_name[i] = strdup(key);//XXX ERRORCHECK
        req->param_value[i] = strdup(val);//XXX ERRORCHECK
    }
    va_end(args);

    return (req);
}

void jsonrpc_request_free(struct jsonrpc_request *req)
{
    if (req) {
        free(req->id);
        free(req->method);
        for (uint32_t i = 0; i < req->nparams; i++) {
            free(req->param_name[i]);
            free(req->param_value[i]);
        }
        free(req);
    }
}

void jsonrpc_request_destroy(struct jsonrpc_request **req)
{
    if (req) {
        jsonrpc_request_free(*req);
        *req = NULL;
    }
}


// FIXME: only supports keyword parameters, not a list.
// FIXME: does not support nested data structures in params
// FIXME: does not support NULL as a parameter value
int
jsonrpc_request_parse(struct jsonrpc_request **dest, const char *buf, int bytes)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    struct jsonrpc_request CLEANUP_JSONRPC_REQUEST *req = NULL;

    *dest = NULL;

    //json rpc example
    //{"jsonrpc": "2.0", "method": "subtract", "params": [42, 23], "id": 1}

    const char *sql = "SELECT key, value, path FROM json_tree(?) WHERE atom IS NOT NULL";
    if (!(req = calloc(1, sizeof(*req))))
        return printlog(LOG_ERR, "calloc failed");

    if (sqlite3_prepare_v2(memdbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return printlog(LOG_ERR, "prepare failed");
    if (sqlite3_bind_text(stmt, 1, buf, bytes, SQLITE_STATIC) != SQLITE_OK)
        return printlog(LOG_ERR, "bind_text failed");

    const char *key, *value, *path;
    bool version_number_match = false;
next_row:
    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            key = (const char *) sqlite3_column_text(stmt, 0);
            value = (const char *) sqlite3_column_text(stmt, 1);
            path = (const char *) sqlite3_column_text(stmt, 2);
            if (!strcmp(path, "$")) {
                if (!strcmp(key, "jsonrpc")) {
                    if (!strcmp(value, "2.0")) {
                        version_number_match = true;
                    } else {
                        return printlog(LOG_ERR, "got unexpected JSON-RPC version number: %s", value);
                    }
                } else if (!strcmp(key, "id")) {
                    req->id = strdup(value);
                    if (!req->id)
                        return printlog(LOG_ERR, "strdup(2): %s", strerror(errno));
                } else if (!strcmp(key, "method")) {
                    req->method = strdup(value);
                    if (!req->method)
                        return printlog(LOG_ERR, "strdup(2): %s", strerror(errno));
                } else {
                    return printlog(LOG_ERR, "got unexpected key in RPC: %s", key);
                }
            } else if (!strcmp(path, "$.params")) {
                if (req->nparams < IPC_REQUEST_PARAM_MAX) {
                    char *k = strdup(key);
                    char *v = strdup(value);
                    if (!k || !v) {
                        free(k);
                        free(v);
                        return printlog(LOG_ERR, "strdup failed");
                    }
                    req->param_name[req->nparams] = k;
                    req->param_value[req->nparams] = v;
                    req->nparams++;
                } else {
                    return printlog(LOG_ERR, "too many parameters provided");
                }
            } else {
                return printlog(LOG_ERR, "unhandled path: %s", path);
            }
            goto next_row;

        case SQLITE_DONE:
            break;

        default:
            return db_error;
    }

    uint32_t errors = 0;
    if (!version_number_match) {
        printlog(LOG_ERR, "JSON-RPC version was not provided");
        errors++;
    }
    if (!req->method) {
        printlog(LOG_ERR, "JSON-RPC method was not provided");
        errors++;
    }
    if (!req->id) {
        printlog(LOG_ERR, "JSON-RPC id was not provided");
        errors++;
    }
    if (errors)
        return -1;

    *dest = req;
    req = NULL;

    return 0;
}

const char *jsonrpc_request_param(const struct jsonrpc_request *req, const char *name)
{
    assert(req);
    assert(name);
    for (uint32_t i = 0; i < req->nparams; i++) {
        if (!strcmp(req->param_name[i], name))
            return req->param_value[i];
    }
    return NULL;
}

// Caller must free result
int jsonrpc_request_serialize(char **result, const struct jsonrpc_request *req)
{
    assert(initialized);
    assert(result);
    assert(req);

    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    char sql[1024];
    *result = NULL;

    //TODO: error check
    sql[0] = '\0';
    strncat(sql, "SELECT json_object('jsonrpc', '2.0', 'method', ?, 'id', ?, 'params', json_object(", sizeof(sql) - 1);
    for (uint32_t i = 0; i < req->nparams; i++) {
        if (i < (req->nparams - 1))
            strncat(sql, "?, ?, ", sizeof(sql) - 1);
        else
            strncat(sql, "?, ?))", sizeof(sql) - 1);
    }
    sql[sizeof(sql) - 1] = '\0';

    if (sqlite3_prepare_v2(memdbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    uint32_t bindidx = 1;
    if (sqlite3_bind_text(stmt, bindidx++, req->method, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;
    if (sqlite3_bind_text(stmt, bindidx++, req->id, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;
    for (uint32_t i = 0; i < req->nparams; i++) {
        if (sqlite3_bind_text(stmt, bindidx++, req->param_name[i], -1, SQLITE_STATIC) != SQLITE_OK)
            return printlog(LOG_ERR, "bind_text failed");
        if (sqlite3_bind_text(stmt, bindidx++, req->param_value[i], -1, SQLITE_STATIC) != SQLITE_OK)
            return printlog(LOG_ERR, "bind_text failed");
    }

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            *result = strdup((char *) sqlite3_column_text(stmt, 0));
            if (!*result)
                return printlog(LOG_ERR, "strdup");
            break;
        case SQLITE_DONE:
        default:
            // TODO: log error
            return -1;
    }

    return 0;
}

struct jsonrpc_response *jsonrpc_response_new(const char *id)
{
    struct jsonrpc_response *res = calloc(1, sizeof(*res));
    if (!res)
        return NULL;

    if (id) {
        res->id = strdup(id);
        if (!res->id) {
            free(res);
            return NULL;
        }
    }

    return res;
}

void jsonrpc_response_free(struct jsonrpc_response *res)
{
    if (res) {
        if (res->result) {
            free(res->result);
        } else {
            free(res->error.message);
            free(res->error.data);
        }
        free(res->id);
        free(res);
    }
}

void jsonrpc_response_destroy(struct jsonrpc_response **res)
{
    if (res && *res) {
        jsonrpc_response_free(*res);
        *res = NULL;
    }
}

int jsonrpc_response_set_result(struct jsonrpc_response *res, const char *result)
{
    if (res->error.code != 0)
        return -1;
    char *p = strdup(result);
    if (!p)
        return -1;
    res->result = p;
    return 0;
}

int jsonrpc_response_set_error(struct jsonrpc_response *res, int retcode, const char *message)
{
    if (res->result)
        return -1;
    res->error.message = strdup(message);
    if (!res->error.message)
        return -1;
    res->error.code = retcode;
    return 0;
}

// Caller must free result
int jsonrpc_response_serialize(char **result, const struct jsonrpc_response *res)
{
    assert(initialized);
    assert(result);
    assert(res);

    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    char sql[1024];
    *result = NULL;

    //TODO: error check
    sql[0] = '\0';
    strncat(sql, "SELECT json_object('jsonrpc', '2.0', 'id', ?, ", sizeof(sql) - 1);
    if (res->error.code == 0) {
        strncat(sql, "'result', ?)", sizeof(sql) - 1);
    } else {
        //NOTE: data field not implemented yet
        strncat(sql, "'error', json_object('code', ?, 'message', ?))", sizeof(sql) - 1);
    }
    sql[sizeof(sql) - 1] = '\0';

    if (sqlite3_prepare_v2(memdbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return db_error;
    uint32_t bindidx = 1;
    if (sqlite3_bind_text(stmt, bindidx++, res->id, -1, SQLITE_STATIC) != SQLITE_OK)
        return db_error;
    if (res->error.code == 0) {
        if (sqlite3_bind_text(stmt, bindidx++, res->result, -1, SQLITE_STATIC) != SQLITE_OK)
            return db_error;
    } else {
        if (sqlite3_bind_int(stmt, bindidx++, res->error.code) != SQLITE_OK)
            return db_error;
        if (sqlite3_bind_text(stmt, bindidx++, res->error.message, -1, SQLITE_STATIC) != SQLITE_OK)
            return db_error;
    }

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            *result = strdup((char *) sqlite3_column_text(stmt, 0));
            if (!*result)
                return printlog(LOG_ERR, "strdup");
            break;
        case SQLITE_DONE:
        default:
            // TODO: log error
            return -1;
    }

    return 0;
}

int
jsonrpc_response_parse(struct jsonrpc_response **dest, const char *buf, int bytes)
{
    sqlite3_stmt CLEANUP_STMT *stmt = NULL;
    struct jsonrpc_response CLEANUP_JSONRPC_RESPONSE *res = NULL;

    *dest = NULL;

    const char *sql = "SELECT key, value, path FROM json_tree(?) WHERE atom IS NOT NULL";
    if (!(res = calloc(1, sizeof(*res))))
        return printlog(LOG_ERR, "calloc failed");

    if (sqlite3_prepare_v2(memdbh, sql, -1, &stmt, 0) != SQLITE_OK)
        return printlog(LOG_ERR, "prepare failed");
    if (sqlite3_bind_text(stmt, 1, buf, bytes, SQLITE_STATIC) != SQLITE_OK)
        return printlog(LOG_ERR, "bind_text failed");

    const char *key, *value, *path;
    bool version_number_match = false;
next_row:
    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            key = (const char *) sqlite3_column_text(stmt, 0);
            value = (const char *) sqlite3_column_text(stmt, 1);
            path = (const char *) sqlite3_column_text(stmt, 2);
            if (!strcmp(path, "$")) {
                if (!strcmp(key, "jsonrpc")) {
                    if (!strcmp(value, "2.0")) {
                        version_number_match = true;
                    } else {
                        return printlog(LOG_ERR, "got unexpected JSON-RPC version number: %s", value);
                    }
                } else if (!strcmp(key, "id")) {
                    res->id = strdup(value);
                    if (!res->id)
                        return printlog(LOG_ERR, "strdup: %s", strerror(errno));
                } else if (!strcmp(key, "result")) {
                    res->result = strdup(value);
                    if (!res->result)
                        return printlog(LOG_ERR, "strdup: %s", strerror(errno));
                } else {
                    return printlog(LOG_ERR, "got unexpected key in RPC: %s", key);
                }
            } else if (!strcmp(path, "$.error")) {
                if (!strcmp(key, "code")) {
                    res->error.code = sqlite3_column_int(stmt, 1);
                } else if (!strcmp(key, "message")) {
                    res->error.message = strdup(value);
                    if (!res->error.message)
                        return printlog(LOG_ERR, "strdup: %s", strerror(errno));
                } else {
                    return printlog(LOG_ERR, "unexpected error key");
                }
            } else {
                return printlog(LOG_ERR, "unhandled path: %s", path);
            }
            goto next_row;

        case SQLITE_DONE:
            break;

        default:
            return db_error;
    }

    uint32_t errors = 0;
    if (!version_number_match) {
        printlog(LOG_ERR, "JSON-RPC version was not provided");
        errors++;
    }
    if (res->result && res->error.code != 0) {
        printlog(LOG_ERR, "result and error are mutually exclusive");
        errors++;
    }
    if (!res->id) {
        printlog(LOG_ERR, "JSON-RPC id was not provided");
        errors++;
    }
    if (errors)
        return -1;

    *dest = res;
    res = NULL;

    return 0;
}