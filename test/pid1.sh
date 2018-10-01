#!/bin/sh
#
# Assumes "vagrant up" has run, and the box is accessible.
#

if [ "$1" = "inside-the-box" ] ; then
    set -ex
    cd /vagrant

    make distclean
    ./configure --as-system-manager
    make
    make init
    sudo make install
    sudo rm -f /lib/jmf/var/jmf/repository.db
    sudo jobcfg init
    rm -f /tmp/*.toml
    cat >/tmp/rc.toml <<EOF
name = "rc"
exclusive = true
standard_out_path = "/dev/console"
standard_error_path = "/dev/console"

[methods]
start = "/bin/sh -x /etc/rc autoboot"
stop = "/bin/sh -x /etc/rc.shutdown"
EOF
   
    cat >/tmp/reopen_db.toml <<EOF
name = "jobd/db_reopen"
exclusive = true
command = "/bin/jobadm jobd reopen_database"
after = "rc"
EOF
    sudo rm -f /lib/jmf/share/jmf/manifests/*
    sudo mv /tmp/*.toml /lib/jmf/share/jmf/manifests
    sudo jobcfg -f /lib/jmf/share/jmf/manifests -v import
    echo 'init_path="/lib/jmf/sbin/init"' | sudo tee /boot/loader.conf
    sudo reboot
    exit
fi

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/pid1.sh inside-the-box'
