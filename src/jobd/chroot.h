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

#include "manifest.h"

struct chroot_jail;

//int chroot_jail_parse_manifest(job_manifest_t manifest, const ucl_object_t *obj);
int chroot_jail_load_handler(struct chroot_jail *jail);
int chroot_jail_unload_handler(struct chroot_jail *jail);
int chroot_jail_context_handler(struct chroot_jail *jail);
void chroot_jail_free(struct chroot_jail *jail);

#endif /* RELAUNCHD_CHROOT_H_ */
