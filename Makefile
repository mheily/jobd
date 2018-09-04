CONFDIR=/etc
SBINDIR=/sbin
#TODO: DBPATH=/etc/jobs.db

CFLAGS+=-Wall -Wextra -Werror
CFLAGS+=-g -O0

# for asprintf()
CFLAGS+=-D_GNU_SOURCE

jobd_OBJS=database.o ipc.o job.o logger.o parser.o toml.o tsort.o
jobcfg_OBJS=database.o ipc.o job.o logger.o parser.o toml.o
jobstat_OBJS=database.o ipc.o logger.o

all: jobcfg jobd jobstat

install: all
	$(INSTALL) -d -m 755 $$DESTDIR$(CONFDIR)/etc/job.d
	$(INSTALL) -m 755 jobd job $$DESTDIR$(SBINDIR)/jobd

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -DCONFDIR=$(CONFDIR) $< -o $@

jobstat: jobstat.o $(jobcfg_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(jobstat_OBJS) -lsqlite3

jobcfg: jobcfg.o $(jobcfg_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(jobcfg_OBJS) -lsqlite3

jobd: jobd.o $(jobd_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(jobd_OBJS) -lrt -lsqlite3

clean:
	rm -f *.o jobd jobcfg jobstat
