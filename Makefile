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

include Makefile.inc

launchd_SOURCES=job.c log.c launchd.c manager.c manifest.c socket.c jsmn/jsmn.c timer.c
DEBUGFLAGS=-g -O0 -DDEBUG

# Flags needed by GCC/glibc
#CFLAGS+=-std=c99 -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_GNU_SOURCE -I/usr/include/kqueue/
#LDFLAGS+=-lkqueue -lpthread

all: launchd sa-wrapper/sa-wrapper.so

check: launchd
	cd test && make && ./jmtest

launchd: $(launchd_SOURCES) config.h
	$(CC) -include config.h $(CFLAGS) $(LDFLAGS) -o $@ $(launchd_SOURCES)

sa-wrapper/sa-wrapper.so:
	cd sa-wrapper ; $(MAKE)

launchd-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) launchd

config.h: Makefile Makefile.inc
	echo "/* Automatically generated -- do not edit */" > config.h
	printf '#define HAVE_SYS_LIMITS_H ' >> config.h
	echo '#include <sys/limits.h>' | $(CC) $(CFLAGS) -x c -c - 2>/dev/null; \
		echo "$$? == 0" | bc >> config.h
 
clean:
	rm -f *.o config.h
	rm -f launchd
	cd test && $(MAKE) clean

dist: clean
	mkdir $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	cp -R $(DISTFILES) $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	find $(PACKAGE_NAME)-$(PACKAGE_VERSION) -name '.gitignore' -exec rm {} \;
	tar cvf $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	rm -rf $(PACKAGE_NAME)-$(PACKAGE_VERSION)

# Not installed by default; this is still under development
install-extra:
	install -m 755 sa-wrapper/sa-wrapper.so $$DESTDIR$(LIBDIR)

install:
	install -s -m 755 launchd $$DESTDIR$(SBINDIR)
	install -m 755 launchctl $$DESTDIR$(BINDIR)
	install -d -m 700 $$DESTDIR/var/db/launchd
	install -d -m 755 $$DESTDIR$(SYSCONFDIR)/launchd \
		$$DESTDIR$(SYSCONFDIR)/launchd/agents \
		$$DESTDIR$(SYSCONFDIR)/launchd/daemons
	install -d -m 755 $$DESTDIR$(DATADIR)/launchd \
		$$DESTDIR$(DATADIR)/launchd/agents \
		$$DESTDIR$(DATADIR)/launchd/daemons

	cat vendor/NextBSD/man/launchd.plist.5 | gzip > $$DESTDIR$(MANDIR)/man5/launchd.plist.5.gz
	for manpage in vendor/NextBSD/man/*.[0-9] ; do \
		section=`echo $$manpage | sed 's/.*\.//'` ; \
		cat $$manpage | gzip > $$DESTDIR$(MANDIR)/man$$section/`basename $$manpage`.gz ; \
	done
 	
	test `uname` = "FreeBSD" && install -m 755 rc.FreeBSD $$DESTDIR/usr/local/etc/rc.d/launchd || true

release:
	test -n "$$VERSION"
	git tag -a -m'Version $$VERSION' v$$VERSION
	git push origin v$$VERSION

.PHONY: all clean launchd
