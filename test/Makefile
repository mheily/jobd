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

# Flags needed by GCC/glibc
#CFLAGS+=-std=c99 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_GNU_SOURCE -I/usr/include/kqueue/
#LDFLAGS+=-lkqueue -lpthread

test_CFLAGS+=-g -O0 -DUNIT_TEST -DDEBUG -I../vendor/libucl/include
LIBUCL_A=../vendor/libucl/src/.libs/libucl.a

TESTS=jmtest manifest-test

all: $(TESTS)

jmtest: 
	$(CC) -include ../config.h -I.. -std=c99 $(test_CFLAGS) $(CFLAGS) -o $@ jmtest.c \
	../log.c ../job.c ../manifest.c ../manager.c ../jsmn/jsmn.c ../socket.c ../timer.c \
	$(LIBUCL_A) $(LDADD)

jail-test: jail-test.c ../jail.c ../jail.h
	$(CC) -I.. $(CFLAGS) o $@ jail-test.c ../jail.c ../log.c -ljail $(LDADD)

manifest-test: manifest-test.c
	$(CC) -I.. $(test_CFLAGS) $(CFLAGS) -o $@ manifest-test.c ../job.c ../manifest.c ../socket.c \
		../timer.c ../log.c $(LIBUCL_A) $(LDADD)

socket-activation-test:
	./test-socket-activation.sh

check: jmtest manifest-test
	DEBUG=yes ./jmtest
	./manifest-test

clean:
	rm -f *.o $(TESTS)
