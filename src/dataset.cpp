/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
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

#include <string>
#include <vector>
#include <ucl.h>

#include "log.h"
#include "dataset.h"

struct dataset_list;

class Dataset {
public:
	Dataset() {};
	std::string pool;
	std::string name;
	std::string origin;
	std::string quota;
};

class DatasetList {
public:
	DatasetList();
	std::vector<Dataset> datasets;
	int parse_manifest(const ucl_object_t *obj);
};

int DatasetList::parse_manifest(const ucl_object_t *obj)
{
	ucl_object_iter_t it;
	const ucl_object_t *cur;
	Dataset ds;

	it = ucl_object_iterate_new(obj);

	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		const char *key = ucl_object_key(cur);
		const char *val = ucl_object_tostring_forced(cur);

		if (!strcmp(key, "Name")) {
			ds.name = val;
		} else if (!strcmp(key, "Pool")) {
			ds.pool = val;
		} else if (!strcmp(key, "Origin")) {
			ds.origin = val;
		} else if (!strcmp(key, "Quota")) {
			ds.quota = val;
		} else {
			log_error("Syntax error: unknown key: %s", key);
			ucl_object_iterate_free(it);
			return -1;
		}
	}

	ucl_object_iterate_free(it);

	datasets.push_back(ds);

	return 0;
}

