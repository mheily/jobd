.POSIX:

CONFDIR=/etc
SBINDIR=/sbin

SQLITE_SRCDIR := vendor/sqlite-amalgamation-3240000
SQLITE_CFLAGS := -I$(SQLITE_SRCDIR) -DSQLITE_THREADSAFE=0 \
					-DSQLITE_OMIT_LOAD_EXTENSION
SQLITE_OBJ := $(SQLITE_SRCDIR)/sqlite3.o

CFLAGS+=-Wall -Wextra -Werror -I$(SQLITE_SRCDIR)
CFLAGS+=-g -O0

# for asprintf()
CFLAGS+=-D_GNU_SOURCE

jobd_OBJS=jobd.o database.o ipc.o job.o logger.o parser.o toml.o tsort.o $(SQLITE_OBJ)
jobcfg_OBJS=jobcfg.o database.o ipc.o job.o logger.o parser.o toml.o $(SQLITE_OBJ)
jobstat_OBJS=jobstat.o database.o ipc.o logger.o $(SQLITE_OBJ)

all: jobcfg jobd jobstat

install: all
	$(INSTALL) -d -m 755 $$DESTDIR$(CONFDIR)/etc/job.d
	$(INSTALL) -m 755 jobd job $$DESTDIR$(SBINDIR)/jobd

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -DCONFDIR=$(CONFDIR) $< -o $@

$(SQLITE_OBJ):
	$(CC) -c $(SQLITE_CFLAGS) -o $@ $(SQLITE_SRCDIR)/sqlite3.c

jobstat: $(jobstat_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobstat_OBJS)

jobcfg: $(jobcfg_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobcfg_OBJS)

jobd: $(jobd_OBJS)
	$(CC) $(CFLAGS) -o $@ $(jobd_OBJS) -lrt

clean:
	rm -f *.o jobd jobcfg jobstat

distclean: clean
	rm -f $(SQLITE_OBJ)