#!/bin/sh
#
# Assumes "vagrant up" has run, and the box is accessible.
#

if [ "$1" = "inside-the-box" ] ; then
    set -ex
    cd /vagrant

    # WORKAROUND: rsync blows away a bunch of stuff in the tree
    # maybe out of tree builds are good :)
    make distclean
    ./configure --as-system-manager
    ./test/build.sh
    ./test/run.sh

    exit
fi

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/vagrant.sh inside-the-box'
