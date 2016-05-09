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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <libjob/logger.h>
#include "dataset.h"
#include "util.h"

struct dataset_list;

struct Dataset {
	Dataset() {};
	int acquire();
	int release();
	bool valid();

	std::string pool;
	std::string name;
	std::string origin;
	std::string quota;
	std::string mountPoint;
	bool destroyAtUnload = false;
	bool acquired = false;
};

class DatasetList {
public:
	DatasetList() {}
	int acquire_resources();
	int release_resources();
	std::vector<Dataset*> datasets;
};

#if 0
int DatasetList::parse_manifest(const ucl_object_t *obj)
{
	ucl_object_iter_t it;
	const ucl_object_t *cur;
	Dataset *ds = new Dataset();

	it = ucl_object_iterate_new(obj);

	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		const char *key = ucl_object_key(cur);
		const char *val = ucl_object_tostring_forced(cur);

		if (!strcmp(key, "Name")) {
			ds->name = val; //FIXME:SECURITY - validation needed
		} else if (!strcmp(key, "Pool")) {
			ds->pool = val; //FIXME:SECURITY - validation needed
		} else if (!strcmp(key, "Origin")) {
			ds->origin = val; //FIXME:SECURITY - validation needed
		} else if (!strcmp(key, "Quota")) {
			ds->quota = val; //FIXME:SECURITY - validation needed
		} else if (!strcmp(key, "MountPoint")) {
			ds->mountPoint = val; //FIXME:SECURITY - validation needed
		} else if (!strcmp(key, "DestroyAtUnload")) {
			if (!ucl_object_toboolean_safe(cur, &ds->destroyAtUnload)) {
				log_error("Syntax error: boolean expected");
				ucl_object_iterate_free(it);
				delete ds;
				return -1;
			}
		} else {
			log_error("Syntax error: unknown key: %s", key);
			ucl_object_iterate_free(it);
			delete ds;
			return -1;
		}
	}

	ucl_object_iterate_free(it);

	if (!ds->valid()) {
		log_error("invalid dataset specification");
		delete ds;
		return -1;
	}

	datasets.push_back(ds);
	return 0;
}
#endif

int DatasetList::acquire_resources() {
	for (auto &dataset : this->datasets) {
		if (dataset->acquire() < 0) {
			return -1;
		}
	}
	return 0;
}

int DatasetList::release_resources() {
	for (auto &dataset : this->datasets) {
		if (dataset->release() < 0) {
			return -1;
		}
	}
	return 0;
}

bool Dataset::valid() {
	// note: origin is optional, and only used by clones
	// note: quota is optional
	return this->name.length() && this->pool.length() && this->mountPoint.length();
}

int Dataset::acquire() {
	char buf[COMMAND_MAX];
	std::string command;
	std::fstream file(this->mountPoint);

	command = "zfs get mountpoint " + this->pool + "/" + this->name;
	if (run_system(&buf, "%s >/dev/null 2>&1", command.c_str()) == 0) {
		log_debug("using existing ZFS dataset mounted at %s", this->mountPoint.c_str());
		// TODO: verify the dataset matches our expectation wrt mountpoint, quota
	} else {
		command = "zfs create -o mountpoint=" + this->mountPoint;
		if (this->quota.length())
			command += " -o quota=" + this->quota;
		command += " " + this->pool + "/" + this->name;


		if (run_system(&buf, "%s", command.c_str()) < 0) {
			log_error("unable to create dataset");
			return -1;
		}
	}

	this->acquired = true;

	return 0;
}

int Dataset::release() {
	char buf[COMMAND_MAX];
	std::string command;
	std::fstream file(this->mountPoint);

	if (!this->acquired)
		return 0;

	if (this->destroyAtUnload) {
		command = "zfs destroy -f " + this->pool + "/" + this->name;
		if (run_system(&buf, "%s >/dev/null 2>&1", command.c_str()) < 0) {
			log_error("failed to destroy dataset");
			return -1;
		}
	}

	this->acquired = false;

	return 0;
}


int dataset_list_load_handler(struct dataset_list *dsl) {
	if (!dsl) return 0;
	return reinterpret_cast<DatasetList*>(dsl)->acquire_resources();
}

int dataset_list_unload_handler(struct dataset_list *dsl) {
	if (!dsl) return 0;
	return reinterpret_cast<DatasetList*>(dsl)->release_resources();
}

void dataset_list_free(struct dataset_list *dsl) {
	delete reinterpret_cast<DatasetList*>(dsl);
}
