#!/bin/sh

objdir="./test/obj"
rm -rf $objdir
mkdir -p $objdir
grep -q './test/obj' config.mk || {
	make distclean
	PREFIX=$objdir ./configure
}
make all -j6
make install
$objdir/bin/jobcfg init
