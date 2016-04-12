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

#include "log.h"
#include "package.h"
#include "util.h"

#ifdef __FreeBSD__
#include "jail.h"
#endif

int package_install(job_manifest_t manifest)
{
	char buf[COMMAND_MAX];
	int i;
	char *name;

	for (i = 0; i < cvec_length(manifest->packages); i++) {
		name = cvec_get(manifest->packages, i);
		log_debug("installing package `%s'", name);

		//FIXME: assumes jails
		if (run_system(&buf, "chroot %s pkg install -y %s", manifest->jail_options->rootdir, name) < 0) {
				log_error("unable to install package: `%s'", name);
				return -1;
		}
	}

	return 0;
}
