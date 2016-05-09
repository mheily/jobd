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
#include "chroot.h"

#include "util.h"

class ChrootJail {
public:
	ChrootJail() {};
	//int parse_manifest(const ucl_object_t *obj);
	int acquire_resources();
	int release_resources();
	int set_execution_context();
	bool valid();

	/** FIXME: duplicate of main manifest key */
	std::string rootDirectory;

	/** Tarball to be extracted into the chroot directory */
	std::string dataSource;

	bool destroyAtUnload = false;
	bool acquired = false;
private:
	bool createChrootJail();
};

#if 0

/* Parse the ChrootJail key */
int ChrootJail::parse_manifest(const ucl_object_t *obj)
{
	ucl_object_iter_t it;
	const ucl_object_t *cur;

	it = ucl_object_iterate_new(obj);

	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		const char *key = ucl_object_key(cur);
		const char *val = ucl_object_tostring_forced(cur);

		/* TODO: allow this to be an array of archive names */
		if (!strcmp(key, "Archive")) {
			// TODO: validate pathname
			this->dataSource = val;
		} else if (!strcmp(key, "RootDirectory")) {
			// TODO: validate pathname
			this->rootDirectory = val;
		} else if (!strcmp(key, "DestroyAtUnload")) {
			if (!ucl_object_toboolean_safe(cur, &this->destroyAtUnload)) {
				log_error("Syntax error: boolean expected");
				ucl_object_iterate_free(it);
				return -1;
			}
		} else {
			log_error("Syntax error: unknown key: %s", key);
			ucl_object_iterate_free(it);
			return -1;
		}
	}

	ucl_object_iterate_free(it);

	return 0;
}
#endif

int ChrootJail::acquire_resources() {
	if (access(this->rootDirectory.c_str(), R_OK | W_OK | X_OK) < 0) {
		if (errno != ENOENT) {
			log_errno("access(2) of %s", this->rootDirectory.c_str());
			return -1;
		}

		// TODO: verify it's a directory, not a file

		if (!this->createChrootJail())
			return -1;
	}

	this->acquired = true;

	return 0;
}

bool ChrootJail::createChrootJail() {
	char buf[COMMAND_MAX];
	std::string command;

	if (mkdir(this->rootDirectory.c_str(), 0700) < 0) {
		log_errno("mkdir(2) of %s", this->rootDirectory.c_str());
		return false;
	}

	command = "tar -C " + this->rootDirectory + " -xf " + this->dataSource;
	if (run_system(&buf, "%s", command.c_str()) < 0) {
		log_error("unable to unpack %s", this->dataSource.c_str());
		return false;
	}

	// FIXME: assumes FreeBSD
	// FIXME: no security controls around device visibility
	command = "mount -t devfs devfs " + this->rootDirectory + "/dev";
	if (run_system(&buf, "%s", command.c_str()) < 0) {
		log_error("unable to mount devfs");
		return false;
	}

	return true;
}

int ChrootJail::release_resources() {
	if (!this->acquired)
		return 0;

	if (this->destroyAtUnload) {
		char buf[COMMAND_MAX];
		std::string command;

		// FIXME: assumes FreeBSD
		command = "umount -f " + this->rootDirectory + "/dev";
		if (run_system(&buf, "%s", command.c_str()) < 0) {
			log_error("unable to unmount /dev");
			return false;
		}

		// FIXME: FreeBSD'ism
		command = "chflags -R noschg " + this->rootDirectory;
		if (run_system(&buf, "%s", command.c_str()) < 0) {
			log_error("unable to run chflags");
			return false;
		}

		command = "rm -rf " + this->rootDirectory;
		if (run_system(&buf, "%s", command.c_str()) < 0) {
			log_error("unable to remove %s", this->rootDirectory.c_str());
			return false;
		}
	}

	this->acquired = false;

	return 0;
}

bool ChrootJail::valid() {
	// FIXME: validate root directory is not /

	// TODO: verify everything is a legal pathname
	return this->rootDirectory.length();
}

int ChrootJail::set_execution_context() {
	log_debug("chroot(2) to %s", this->rootDirectory.c_str());
	if (chroot(this->rootDirectory.c_str()) < 0) {
		log_errno("chroot(2) to %s", this->rootDirectory.c_str());
		return -1;
	}
	return 0;
}

int chroot_jail_load_handler(struct chroot_jail *jail) {
	if (!jail) return 0;
	return reinterpret_cast<ChrootJail*>(jail)->acquire_resources();
}

int chroot_jail_unload_handler(struct chroot_jail *jail) {
	if (!jail) return 0;
	return reinterpret_cast<ChrootJail*>(jail)->release_resources();
}

int chroot_jail_context_handler(struct chroot_jail *jail) {
	if (!jail) return 0;
	return reinterpret_cast<ChrootJail*>(jail)->set_execution_context();
}

void chroot_jail_free(struct chroot_jail *jail) {
	delete reinterpret_cast<ChrootJail*>(jail);
}
