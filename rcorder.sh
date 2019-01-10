#!/bin/sh
exec /sbin/rcorder $* | egrep -v '^/etc/rc.d/(sysctl)$'
