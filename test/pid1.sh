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

    sudo jobcfg -v import <<EOF
name = "sysctl"
type = "task"
wait = true

[methods]

start = """
    /sbin/sysctl -f /etc/sysctl.conf -f /etc/sysctl.conf.local
    test ! -r /etc/sysctl.conf.local || /sbin/sysctl -f /etc/sysctl.conf.local"""
EOF

    sudo jobcfg -v import <<EOF
name = "rc"
type = "task"
after = ["sysctl"]
wait = true
standard_out_path = "/dev/console"
standard_error_path = "/dev/console"

[methods]
start = "/bin/sh -x /etc/rc autoboot"
stop = "/bin/sh -x /etc/rc.shutdown"
EOF
   
    sudo jobcfg -v import <<EOF
name = "jobd/db_reopen"
type = "task"
wait = true
command = "$BINDIR/jobadm jobd reopen_database"
after = "rc"
EOF

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
