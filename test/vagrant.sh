#!/bin/sh
#
# Assumes "vagrant up" has run, and the box is accessible.
#

if [ "$1" = "inside-the-box" ] ; then
    set -ex
    cd /vagrant

    ./test/run.sh

    exit
fi

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/vagrant.sh inside-the-box'