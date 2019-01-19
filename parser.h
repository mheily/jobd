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

#ifndef _PARSER_H
#define _PARSER_H

struct job;
struct job_parser;

int job_db_insert(struct job_parser *jpr);
int parse_job_file(struct job_parser *jpr, const char *path);
struct job_parser * job_parser_new(void);
void job_parser_free(struct job_parser *jpr);
int parser_import(const char *path);

#define PROPERTY_TYPE_INVALID 0
#define PROPERTY_TYPE_INT 1
#define PROPERTY_TYPE_STRING 2
#define PROPERTY_TYPE_BOOL 3

#endif /* _PARSER_H */
