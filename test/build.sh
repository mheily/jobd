#!/bin/sh -ex

objdir="$(pwd)/test/obj"
rm -rf $objdir
mkdir -p $objdir $objdir/libexec $objdir/run/jobd
grep -q './test/obj' config.mk || {
	make distclean
	export PREFIX=$objdir
	export EXEC_PREFIX=$objdir
	export BINDIR=$objdir/bin
	export SBINDIR=$objdir/sbin
	export MANDIR=$objdir/man
	export PKGCONFIGDIR=$objdir/etc/jobd
	export RUNSTATEDIR=$objdir/var/run/jobd
	export RUNDIR=$objdir/run
	./configure
}
make all -j6
make install
. ./config.inc
$BINDIR/jobcfg init
