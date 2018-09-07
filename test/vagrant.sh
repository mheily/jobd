#!/bin/sh -ex
#
# Assumes "vagrant up" has run, and the box is accessible.
#
test -e Vagrantfile
vagrant rsync && vagrant ssh -c "sh -ex -c 'cd /vagrant ; make distclean all ; ./test/run.sh'"
