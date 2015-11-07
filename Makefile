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

launchd_SOURCES=job.c log.c launchd.c manifest.c socket.c jsmn/jsmn.c
DEBUGFLAGS=-g -O0 -DDEBUG

all: launchd

check: launchd
	cd test && make && ./jmtest

launchd: $(launchd_SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(launchd_SOURCES)

launchd-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) launchd

clean:
	rm -f *.o
	rm -f launchd
	cd test && $(MAKE) clean

dist: clean
	mkdir $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	cp -R $(DISTFILES) $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	find $(PACKAGE_NAME)-$(PACKAGE_VERSION) -name '.gitignore' -exec rm {} \;
	tar cvf $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	rm -rf $(PACKAGE_NAME)-$(PACKAGE_VERSION)

install:
	install -s -m 755 launchd $$DESTDIR$(SBINDIR)
	install -m 755 launchctl $$DESTDIR$(BINDIR)
	install -d -m 700 $$DESTDIR/.launchd
	install -d -m 755 $$DESTDIR$(SYSCONFDIR)/launchd \
		$$DESTDIR$(SYSCONFDIR)/launchd/agents \
		$$DESTDIR$(SYSCONFDIR)/launchd/daemons
	install -d -m 755 $$DESTDIR$(DATADIR)/launchd \
		$$DESTDIR$(DATADIR)/launchd/agents \
		$$DESTDIR$(DATADIR)/launchd/daemons
	test `uname` = "FreeBSD" && install -m 755 rc.FreeBSD $$DESTDIR/usr/local/etc/rc.d/launchd || true

.PHONY: all clean launchd
