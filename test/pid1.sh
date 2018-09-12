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
    sudo /lib/jmf/bin/jobcfg init
    cat >/tmp/rc.toml <<EOF
name = "rc"
exclusive = true

[methods]
start = "/bin/sh -x /etc/rc autoboot"
stop = "/bin/sh -x /etc/rc.shutdown"
EOF
    sudo mv /tmp/rc.toml /lib/jmf/share/jmf/manifests
    sudo /lib/jmf/bin/jobcfg -f /lib/jmf/share/jmf/manifests -v import
    echo 'init_path="/lib/jmf/sbin/init"' | sudo tee /boot/loader.conf
    sudo reboot
    exit
fi

set -ex
test -e Vagrantfile
vagrant rsync
vagrant ssh -c '/vagrant/test/pid1.sh inside-the-box'