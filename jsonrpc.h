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

#ifndef _JSONRPC_H
#define _JSONRPC_H

#include <sqlite3.h>
#include <stdint.h>

#define IPC_REQUEST_PARAM_MAX 8
struct jsonrpc_request {
    char * method;
    char *param_name[IPC_REQUEST_PARAM_MAX];
    char *param_value[IPC_REQUEST_PARAM_MAX];
    uint32_t nparams;
    char * id;
};

struct jsonrpc_response {
    char *result;
    struct {
        int code;
        char *message;
        char *data;
    } error;
    char *id;
};

int jsonrpc_init(void);
void jsonrpc_shutdown(void);
struct jsonrpc_request * jsonrpc_request_new(const char *id, const char *method, uint32_t nparams, ...);
int jsonrpc_request_parse(struct jsonrpc_request **dest, const char *buf, int bytes);
int jsonrpc_request_serialize(char **result, const struct jsonrpc_request *req);
const char *jsonrpc_request_param(const struct jsonrpc_request *req, const char *name);
void jsonrpc_request_free(struct jsonrpc_request *req);
void jsonrpc_request_destroy(struct jsonrpc_request **req);
#define CLEANUP_JSONRPC_REQUEST __attribute__((__cleanup__(jsonrpc_request_destroy)))


struct jsonrpc_response * jsonrpc_response_new(const char *id);
int jsonrpc_response_parse(struct jsonrpc_response **dest, const char *buf, int bytes);
void jsonrpc_response_free(struct jsonrpc_response *res);
void jsonrpc_response_destroy(struct jsonrpc_response **res);
int jsonrpc_response_set_result(struct jsonrpc_response *res, const char *result);
int jsonrpc_response_set_error(struct jsonrpc_response *res, int retcode, const char *message);
int jsonrpc_response_serialize(char **result, const struct jsonrpc_response *res);

#define CLEANUP_JSONRPC_RESPONSE __attribute__((__cleanup__(jsonrpc_response_destroy)))


#endif /* _JSONRPC_H */
