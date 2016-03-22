#!/bin/sh
#
# Copyright (c) 2015 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# KLUDGE: hardcoded for Linux. This should be hooked into ./configure output somehow.
#
if [ `uname` = 'Linux' ] ; then
	prefix="/usr"
	sysconfdir="/etc"
else
	prefix="/usr/local"
	sysconfdir="$prefix/etc"
fi

if [ `id -u` -eq 0 ] ; then
	pkgdatadir="/var/db/launchd"
	pidfile="/var/run/launchd.pid"
	agentdir="${sysconfdir}/launchd/agents"
	daemondir="${sysconfdir}/launchd/daemons"
else
	pkgdatadir="$HOME/.launchd/run"
	pidfile="$pkgdatadir/launchd.pid"
	agentdir="$HOME/.launchd/agents"
	daemondir="" # N/A
fi
watchdir="$pkgdatadir/new"

command=$1
shift

#
# Start launchd if it is not already running
#
if [ -f $pidfile ] ; then
	kill -0 `cat $pidfile` >/dev/null 2>&1 || rm -f $pidfile
fi
if [ ! -f $pidfile ] ; then
	$prefix/sbin/launchd
	if [ $? -ne 0 ] ; then
		echo "ERROR: $launchd failed to start" > /dev/stderr
		exit 1
	fi
fi

for path in $*
do
	if [ ! -e "$path" ] ; then
		echo "ERROR: $path does not exist" > /dev/stderr
		exit 1
	fi
done

case $command in
load)
	filespec=`find $* -name '*.json'`
	for path in $filespec ; do
		cp $path $watchdir
	done
	kill -HUP `cat $pidfile`
	;;

unload)
	filespec=`find $* -name '*.json'`
	for path in $filespec ; do
		label=`basename "$path" | sed 's/\.json$//'`
		touch $watchdir/$label.unload
	done
	kill -HUP `cat $pidfile`
	;;

list)
	listfile="$pkgdatadir/launchctl.list"
	rm -f $listfile
	kill -USR1 `cat $pidfile`
	count=0
	while [ ! -f $listfile ] ; do
		sleep 0.2
		count=$((count + 1))
		if [ $count -eq 50 ] ; then
			echo "ERROR: no response from launchd" > /dev/stderr
			exit 1
		fi
	done
	cat $listfile
	;;

*)
	echo "invalid command"
	exit 1
esac

exit 0
