.POSIX:

INSTALL ?= /usr/bin/install

INIT_SRCDIR := vendor/freebsd-init-r338454
INIT_CFLAGS := -DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
INIT_LDADD := -lutil -lcrypt

SQLITE_SRCDIR := vendor/sqlite-amalgamation-3240000
SQLITE_CFLAGS := -I$(SQLITE_SRCDIR) -DSQLITE_THREADSAFE=0 \
					-DSQLITE_OMIT_LOAD_EXTENSION
SQLITE_OBJ := $(SQLITE_SRCDIR)/sqlite3.o

CFLAGS+=-Wall -Wextra -Werror -I$(SQLITE_SRCDIR) -Ivendor
CFLAGS+=-g -O0

# for asprintf()
CFLAGS+=-D_GNU_SOURCE

bin_BINS := jobcfg jobstat jobadm jobprop

jobd_OBJS=jobd.o database.o ipc.o job.o logger.o parser.o vendor/pidfile.o vendor/flopen.o toml.o $(SQLITE_OBJ)
jobcfg_OBJS=jobcfg.o database.o ipc.o job.o logger.o parser.o toml.o $(SQLITE_OBJ)
jobadm_OBJS=jobadm.o database.o ipc.o logger.o $(SQLITE_OBJ)
jobstat_OBJS=jobstat.o database.o ipc.o logger.o $(SQLITE_OBJ)
jobprop_OBJS=jobprop.o database.o ipc.o logger.o $(SQLITE_OBJ)

all: jobd $(bin_BINS) init

init: init.c config.h
	$(CC) -O2 -Wall -Werror init.c -o init

install: all config.mk
	$(MAKE) -f Makefile -f config.mk install-stage2

install-stage2:
	$(INSTALL) -d -m 755 $(DESTDIR)$(PKGCONFIGDIR) \
		$(DESTDIR)$(BINDIR) $(DESTDIR)$(SBINDIR) \
		$(DESTDIR)$(DATAROOTDIR) \
		$(DESTDIR)$(DATAROOTDIR)/manifests \
		$(DESTDIR)$(LIBEXECDIR) \
		$(DESTDIR)$(LOCALSTATEDIR) \
		$(DESTDIR)$(RUNSTATEDIR) \
		$(DESTDIR)$(RUNDIR)
	$(INSTALL) -m 555 jobd $(DESTDIR)$(SBINDIR)/jobd
	$(INSTALL) -m 555 init $(DESTDIR)$(LIBEXECDIR)/init
	$(INSTALL) -m 555 shutdown.sh $(DESTDIR)$(LIBEXECDIR)/shutdown
	$(INSTALL) -m 555 jobadm jobcfg jobstat jobprop $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 444 schema.sql volatile.sql views.sql $(DESTDIR)$(DATAROOTDIR)
	test ! -d share/manifests/`uname` || $(INSTALL) -m 444 share/manifests/`uname`/* $(DESTDIR)$(DATAROOTDIR)/manifests

%.o: %.c *.h config.h
	$(CC) -c $(CFLAGS) $< -o $@

config.h config.mk:
	./configure

$(SQLITE_OBJ):
	$(CC) -c $(SQLITE_CFLAGS) -o $@ $(SQLITE_SRCDIR)/sqlite3.c

jobadm: $(jobadm_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobadm_OBJS)

jobstat: $(jobstat_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobstat_OBJS)

jobcfg: $(jobcfg_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobcfg_OBJS)

jobprop: $(jobprop_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobprop_OBJS)

jobd: $(jobd_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobd_OBJS) -lrt

clean:
	rm -f *.o vendor/*.o jobd $(bin_BINS) init

distclean: clean
	rm -f $(SQLITE_OBJ) config.mk config.h config.inc

sloccount:
	cloc --by-file `ls *.[c] | egrep -v '(toml)\.'`
	cloc --by-file `ls *.[ch] | egrep -v '(toml|queue)\.'`

PHONY : sloccount
