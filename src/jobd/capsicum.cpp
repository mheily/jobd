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

extern "C" {
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
}

#include <libjob/logger.h>
#include "capsicum.h"

using json_t = nlohmann::json;

#if HAVE_CAPSICUM

/*
This was generated from the output of:
for flag in `grep CAP_ /usr/include/sys/capsicum.h | awk '{print $2}' | grep ^CAP | sort` ;
do
    shortname=$(echo $flag | tr 'A-Z' 'a-z' | sed 's/^cap_//')
    echo "{ \"$shortname\", $flag },"
done
*/
static const std::map<string, unsigned long long> cap_rights_strings =
{
		{ "accept", CAP_ACCEPT },
		{ "acl_check", CAP_ACL_CHECK },
		{ "acl_delete", CAP_ACL_DELETE },
		{ "acl_get", CAP_ACL_GET },
		{ "acl_set", CAP_ACL_SET },
		{ "all0", CAP_ALL0 },
		{ "all1", CAP_ALL1 },
		{ "bind", CAP_BIND },
		{ "bindat", CAP_BINDAT },
		{ "chflagsat", CAP_CHFLAGSAT },
		{ "connect", CAP_CONNECT },
		{ "connectat", CAP_CONNECTAT },
		{ "create", CAP_CREATE },
		{ "event", CAP_EVENT },
		{ "extattr_delete", CAP_EXTATTR_DELETE },
		{ "extattr_get", CAP_EXTATTR_GET },
		{ "extattr_list", CAP_EXTATTR_LIST },
		{ "extattr_set", CAP_EXTATTR_SET },
		{ "fchdir", CAP_FCHDIR },
		{ "fchflags", CAP_FCHFLAGS },
		{ "fchmod", CAP_FCHMOD },
		{ "fchmodat", CAP_FCHMODAT },
		{ "fchown", CAP_FCHOWN },
		{ "fchownat", CAP_FCHOWNAT },
		{ "fcntl", CAP_FCNTL },
		{ "fcntl_all", CAP_FCNTL_ALL },
		{ "fcntl_getfl", CAP_FCNTL_GETFL },
		{ "fcntl_getown", CAP_FCNTL_GETOWN },
		{ "fcntl_setfl", CAP_FCNTL_SETFL },
		{ "fcntl_setown", CAP_FCNTL_SETOWN },
		{ "fexecve", CAP_FEXECVE },
		{ "flock", CAP_FLOCK },
		{ "fpathconf", CAP_FPATHCONF },
		{ "fsck", CAP_FSCK },
		{ "fstat", CAP_FSTAT },
		{ "fstatat", CAP_FSTATAT },
		{ "fstatfs", CAP_FSTATFS },
		{ "fsync", CAP_FSYNC },
		{ "ftruncate", CAP_FTRUNCATE },
		{ "futimes", CAP_FUTIMES },
		{ "futimesat", CAP_FUTIMESAT },
		{ "getpeername", CAP_GETPEERNAME },
		{ "getsockname", CAP_GETSOCKNAME },
		{ "getsockopt", CAP_GETSOCKOPT },
		{ "ioctl", CAP_IOCTL },
		{ "ioctls_all", CAP_IOCTLS_ALL },
		{ "kqueue", CAP_KQUEUE },
		{ "kqueue_change", CAP_KQUEUE_CHANGE },
		{ "kqueue_event", CAP_KQUEUE_EVENT },
		{ "linkat_source", CAP_LINKAT_SOURCE },
		{ "linkat_target", CAP_LINKAT_TARGET },
		{ "listen", CAP_LISTEN },
		{ "lookup", CAP_LOOKUP },
		{ "mac_get", CAP_MAC_GET },
		{ "mac_set", CAP_MAC_SET },
		{ "mkdirat", CAP_MKDIRAT },
		{ "mkfifoat", CAP_MKFIFOAT },
		{ "mknodat", CAP_MKNODAT },
		{ "mmap", CAP_MMAP },
		{ "mmap_r", CAP_MMAP_R },
		{ "mmap_rw", CAP_MMAP_RW },
		{ "mmap_rwx", CAP_MMAP_RWX },
		{ "mmap_rx", CAP_MMAP_RX },
		{ "mmap_w", CAP_MMAP_W },
		{ "mmap_wx", CAP_MMAP_WX },
		{ "mmap_x", CAP_MMAP_X },
		{ "pdgetpid", CAP_PDGETPID },
		{ "pdkill", CAP_PDKILL },
		{ "pdwait", CAP_PDWAIT },
		{ "peeloff", CAP_PEELOFF },
		{ "poll_event", CAP_POLL_EVENT },
		{ "pread", CAP_PREAD },
		{ "pwrite", CAP_PWRITE },
		{ "read", CAP_READ },
		{ "recv", CAP_RECV },
		{ "renameat_source", CAP_RENAMEAT_SOURCE },
		{ "renameat_target", CAP_RENAMEAT_TARGET },
		{ "seek", CAP_SEEK },
		{ "seek_tell", CAP_SEEK_TELL },
		{ "sem_getvalue", CAP_SEM_GETVALUE },
		{ "sem_post", CAP_SEM_POST },
		{ "sem_wait", CAP_SEM_WAIT },
		{ "send", CAP_SEND },
		{ "setsockopt", CAP_SETSOCKOPT },
		{ "shutdown", CAP_SHUTDOWN },
		{ "sock_client", CAP_SOCK_CLIENT },
		{ "sock_server", CAP_SOCK_SERVER },
		{ "symlinkat", CAP_SYMLINKAT },
		{ "ttyhook", CAP_TTYHOOK },
		{ "unlinkat", CAP_UNLINKAT },
		{ "write", CAP_WRITE },
};

static void set_rights(int fd, const json_t& json)
{
	cap_rights_t setrights;

	cap_rights_init(&setrights);
	for (auto const & it : json) {
		auto kv = cap_rights_strings.find(it);
		if (kv == cap_rights_strings.end()) {
			log_error("invalid rights string");
			throw "bad rights string";
		}
		unsigned long long val = kv->second;
		cap_rights_set(&setrights, val);
	}
	cap_rights_limit(fd, &setrights);
}
#endif // HAVE_CAPSICUM

void capsicum_resources_acquire(json_t& manifest, std::map<std::string, int> descriptors)
{
#if HAVE_CAPSICUM
	if (manifest.find("CapsicumRights") != manifest.end()) {
		json_t o = manifest["CapsicumRights"];
		for (nlohmann::json::iterator it = o.begin(); it != o.end(); ++it) {
			string key = it.key();
			auto kv = descriptors.find(key);
			if (kv == descriptors.end()) {
				log_error("no descriptor matching `%s'", key.c_str());
				throw "bad manifest";
			}
			set_rights(kv->second, it.value());
		}
	}
#endif
}
