#!/bin/sh -x
#
# Assumes "vagrant up" has run, and the box is accessible.
#

if [ "$1" = "inside-the-box" ] ; then
    sudo pkill -9 jobd
    set -ex
    cd /vagrant

    make distclean
    ./configure
    . ./config.inc
    make
    sudo make install
    sudo rm -f $LOCALSTATEDIR/repository.db
    sudo jobcfg init
    sudo jobcfg -v -f $DATAROOTDIR/manifests import
    echo "init_path=\"$LIBEXECDIR/init\"" | sudo tee /boot/loader.conf.local


    exit
fi

# If there is a 'fresh' snapshot, revert to it.
vagrant snapshot list 2>&1 | grep -q fresh && \
    vagrant snapshot restore --no-provision fresh

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/pid1.sh inside-the-box && sudo reboot'
