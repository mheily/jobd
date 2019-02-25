#!/bin/sh -ex

objdir="$(pwd)/test/obj"
rm -rf $objdir
mkdir -p $objdir $objdir/libexec $objdir/run/jobd $objdir/var/log
grep -q $objdir config.h || {
    cmake -DCMAKE_INSTALL_PREFIX=$objdir .
}
make all -j6
make install
. ./config.inc
$BINDIR/jobcfg init
