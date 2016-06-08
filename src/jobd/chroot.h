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

#ifndef RELAUNCHD_CHROOT_H_
#define RELAUNCHD_CHROOT_H_

#include <libjob/namespaceImport.hpp>
#include "manifest.h"

class ChrootJail {
public:
	ChrootJail() {};
	void parseManifest(const nlohmann::json json);
	void acquireResources();
	void releaseResources();
	int set_execution_context();
	bool valid();

	/** FIXME: duplicate of main manifest key */
	std::string rootDirectory;

	/** Tarball to be extracted into the chroot directory */
	std::string dataSource;

	bool destroyAtUnload = false;
	bool acquired = false;

	const bool isDefined() const {
		return defined;
	} 

private:
	bool defined = false;
	void createChrootJail();
};

#endif /* RELAUNCHD_CHROOT_H_ */
