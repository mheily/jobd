# for asprintf()
CFLAGS+=-D_GNU_SOURCE

# for libbsd
#CFLAGS+=-isystem /usr/include/bsd -DLIBBSD_OVERLAY

all: jobd job

# for debug
CFLAGS+=-g -O0

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

job: jobd
	ln -s jobd job

# rc: rc.c toml.c job.h job.c logger.c parser.c logger.h array.h
# 	$(CC) $(CFLAGS) -Wall -Werror -o rc rc.c job.c logger.c parser.c toml.c

jobd: jobd.c toml.c job.c logger.c logger.h parser.c job.h array.h Makefile
	$(CC) $(CFLAGS) -Wall -Werror -o jobd jobd.c job.c logger.c parser.c toml.c -lrt

copy-to-freebsd-base:
	mkdir -p /usr/src/sbin/jobd
	cp Makefile.FreeBSD /usr/src/sbin/jobd/Makefile
	cp *.c *.h *.8 *.5 /usr/src/sbin/jobd
	cd /usr/src/sbin/jobd && make

clean:
	rm -f jobd job
