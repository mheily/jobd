CFLAGS+=-Wall -Wextra -Werror
CFLAGS+=-g -O0

# for asprintf()
CFLAGS+=-D_GNU_SOURCE

LIBADD=-lrt

jobd_OBJS=toml.o job.c logger.o parser.o

all: jobd job

install:
	test `id -u` = "0" && $(MAKE) install-as-system || $(MAKE) install-as-user

install-as-system: all
	install -d -m 755 -o 0 -g 0 /etc/job.d
	install -m 755 jobd /sbin/jobd
	install -m 755 job /sbin/job

install-as-user: all
	install -d -m 755 $$HOME/.config/job.d $$HOME/bin
	install -m 755 jobd job rc $$HOME/bin
	test -e $$HOME/.config/job.d/jobd || sed -e "s,@@HOME@@,$$HOME," < ./job.d/jobd-user > \
		$$HOME/.config/job.d/jobd

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@

job: jobd
	ln -s jobd job

jobd: jobd.o $(jobd_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(jobd_OBJS) $(LIBADD)

#copy-to-freebsd-base:
#	mkdir -p /usr/src/sbin/jobd
#	cp Makefile.FreeBSD /usr/src/sbin/jobd/Makefile
#	cp *.c *.h *.8 *.5 /usr/src/sbin/jobd
#	cd /usr/src/sbin/jobd && make

clean:
	rm -f jobd job
