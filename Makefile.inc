#
# Copyright (c) 2015 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

PACKAGE_NAME ?= relaunchd
PACKAGE_VERSION ?= 0.1.1
MAJOR_VERSION != echo $(PACKAGE_VERSION) | cut -f1 -d.
MINOR_VERSION != echo $(PACKAGE_VERSION) | cut -f2 -d.
PATCH_VERSION != echo $(PACKAGE_VERSION) | cut -f3 -d.

PREFIX ?= /usr/local
LIBDIR = $(PREFIX)/lib
BINDIR = $(PREFIX)/bin
SBINDIR = $(PREFIX)/sbin
LIBEXECDIR = $(PREFIX)/libexec
MANDIR = $(PREFIX)/man
SYSCONFDIR = $(PREFIX)/etc
DATADIR = $(PREFIX)/share
LOCALSTATEDIR = /var
RUNSTATEDIR = $(LOCALSTATEDIR)/run

# Files to include in the tarball
DISTFILES = *.[ch] rc.* launchctl LICENSE README.md Makefile Makefile.inc include jsmn test

