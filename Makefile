# for asprintf()
CFLAGS+=-D_GNU_SOURCE

# for libbsd
#CFLAGS+=-isystem /usr/include/bsd -DLIBBSD_OVERLAY

all: jobd

# for debug
CFLAGS+=-g -O0

#install:
#	install -d -m 755 -o 0 -g 0 /var/spool/job
#	install -d -m 700 -o 0 -g 0 /var/spool/job/system

jobd: jobd.c toml.c Makefile
	$(CC) $(CFLAGS) -Wall -Werror -o jobd jobd.c toml.c -lrt
