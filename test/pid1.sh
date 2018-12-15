#!/bin/sh
#
# Assumes "vagrant up" has run, and the box is accessible.
#

if [ "$1" = "inside-the-box" ] ; then
    sudo pkill -9 jobd
    set -ex
    cd /vagrant

    make distclean
    ./configure
    make
    sudo make install
    sudo rm -f /lib/jobd/repository.db
    sudo jobcfg init
    sudo jobcfg -v import <<EOF
name = "rc"
exclusive = true
standard_out_path = "/dev/console"
standard_error_path = "/dev/console"

[methods]
start = "/bin/sh -x /etc/rc autoboot"
stop = "/bin/sh -x /etc/rc.shutdown"
EOF
   
    sudo jobcfg -v import <<EOF
name = "jobd/db_reopen"
exclusive = true
command = "/bin/jobadm jobd reopen_database"
after = "rc"
EOF

    echo 'init_path="/lib/jobd/init"' | sudo tee /boot/loader.conf.local
    exit
fi

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/pid1.sh inside-the-box && sudo reboot'
