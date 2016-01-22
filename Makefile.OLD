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

launchd_CFLAGS=-include config.h -std=c99 -Wall -Werror -Ivendor/libucl/include
launchd_SOURCES=job.c log.c launchd.c manager.c manifest.c socket.c \
                   timer.c pidfile.c flopen.c
LIBUCL_A=vendor/libucl/src/.libs/libucl.a
DEBUGFLAGS=-g -O0 -DDEBUG

all: launchd

check: launchd
	cd test && make check

launchd: $(launchd_SOURCES) config.h $(LIBUCL_A)
	$(CC) $(launchd_CFLAGS) $(CFLAGS) -o $@ $(launchd_SOURCES) $(LIBUCL_A) $(LDADD)

$(LIBUCL_A):
	git submodule init
	git submodule update
	cd vendor/libucl && ./autogen.sh && CFLAGS="-gdwarf-2 -gstrict-dwarf -g -O0" ./configure && make

sa-wrapper/sa-wrapper.so:
	cd sa-wrapper ; $(MAKE)

launchd-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) launchd

config.h: Makefile Makefile.inc
	echo "/* Automatically generated -- do not edit */" > config.h
	printf '#define HAVE_SYS_LIMITS_H ' >> config.h
	echo '#include <sys/limits.h>' | $(CC) $(CFLAGS) -x c -o /dev/null -c - 2>/dev/null; \
		echo "$$? == 0" | bc >> config.h
	echo "#define CACHEDIR \"/var/cache/launchd\"" >> config.h
	test -d /var/db && statedir=/var/db/launchd || statedir=/var/lib/launchd ; \
		echo "#define PKGSTATEDIR \"$$statedir\"" >> config.h

# Convert C preprocessor definitions into shell variables
vars.sh: config.h
	echo '# Automatically generated -- do not edit' > vars.sh
	egrep '^#define' config.h | sed 's/^#define //; s/ /=/' >> vars.sh

clean:
	rm -f *.o config.h vars.sh
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

install: vars.sh
	install -s -m 755 launchd $$DESTDIR$(SBINDIR)
	install -m 755 launchctl $$DESTDIR$(BINDIR)
	. vars.sh ; install -d -m 700 $$DESTDIR$$PKGSTATEDIR $$DESTDIR$$CACHEDIR
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
