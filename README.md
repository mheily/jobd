# relaunchd

## Overview 

relaunchd is a service management daemon that is similar to the launchd(8)
facility found in the Darwin operating environment [1].

It was written from scratch based on the published API, and all code is
available under the ISC license. See the LICENSE file for more information.

It is currently under heavy development, and should not be used for anything
important. Be especially mindful that there is NO WARRANTY provided with this
software.

## Status

See the [release notes](./CHANGELOG.md) for details about
the current release.

relaunchd aims to support the following platforms:
* FreeBSD 9, 10, and 11-CURRENT
* OpenBSD 5.8
* NetBSD 7.0
* Linux (various flavors)

The core functionality is working:
* loading and unloading jobs with launchctl
* launching jobs
* socket-activated jobs via the Sockets key
* periodic jobs that use the StartInterval key

There are some new features not found in the original launchd:
* JSON is used instead of XML
* you can launch programs that place themselves inside of a jail(8)
* a wrapper library that allows programs to use socket activation without
  modifying the source code.
   
Some things are not implemented yet:
* cron emulation (see issue #17)
* file and directory watches (see issue #25)
* resource limits (see issue #27)
* LaunchOnlyOnce (see issue #28)
* inetdCompatibility (see issue #29)

Some things will probably never be implemented:
* oddities - LimitLoadToHosts, LimitLoadFromHosts
* kernel and launchd debugging - Debug, WaitForDebugger
* Mach IPC
* the StartOnMount key - may require kernel support for filesystem mount
  notifications
* the original XML plist format; use JSON instead.
* hacks and workarounds - HopefullyExitsFirst, HopefullyExitsLast
* Darwin-specific things - EnableTransactions
* legacy keys - Disabled, OnDemand

## Building (all platforms)

The basic commands to build and install the software are:
```
	make
	sudo make install
```

## Building under Linux

You can check the current build status by visiting the 
[Travis CI dashboard](https://travis-ci.org/mheily/relaunchd/builds)

There are a few extra steps when building on Linux:

1. Download and install libkqueue. For Debian-based distributions, you can
   simply run:

	sudo apt-get install libkqueue-dev

   Other distributions will require you to build from source, which is 
   available at:

	https://github.com/mheily/libkqueue/

## Building under OpenBSD

You will need to build libucl, which means installing GNU Autotools:
```
# pkg_add autoconf-2.69p1 automake-1.15 libtool
```

Run the configure script:
```
./configure
```

Since libucl will try to run autoconf/automake, you will need to provide
the environment variables to make(1):
```
AUTOCONF_VERSION=2.69 AUTOMAKE_VERSION=1.15 make
```


## Building under NetBSD

You will need to build libucl, which means installing GNU Autotools:
```
# pkg_add autoconf automake libtool pkg-config
```

## Socket activation

relaunchd uses a different mechanism for socket activation than the one that
Darwin uses.

TODO -- document this

## Installation 

Currently, relaunchd has only been tested on FreeBSD, but should be portable
to other BSD operating systems. 

To install relaunchd, run the following commands:

	make
	sudo make install

This will install the following executable commands:
* launchd
* launchctl

It will also install the following manpages: 

* launchctl(1)
* launchd(8)
* launchd.plist(5)

## Usage

To start launchd, run the following command as root:

	service launchd start

If you want to run jobs in your graphical user session, add the following lines to
your session startup file:

	launchctl load ~/.launchd/agents /usr/local/etc/launchd/agents /usr/local/share/launchd/agents   

## Differences with Darwin launchd

There are some specific design choices that make relaunchd different from the
or iginal launchd found in Darwin.

The original launchd uses friendly MacOS-style names for the directories where
its configuration files are stored.  For example, one of the directories is
named /System/Library/LaunchDaemons. Relaunchd uses more traditional lowercase
names, and stores it's files in:

	$HOME/.launchd/agents
	/usr/local/etc/launchd/agents
	/usr/local/etc/launchd/daemons
        /usr/local/share/launchd/daemons
        /usr/local/share/launchd/agents

On FreeBSD, there is no compelling reason to have launchd run as pid #1, so
relaunchd is designed to be started by the traditional init(8) command.
Relaunchd currently coexists with the /etc/rc mechanism, until such time as all
of the /etc/rc.d scripts can be converted to launchd jobs.

Instead of using the XML plist format, job manifests are specified using JSON.

In the Darwin implementation, there is a single launchd(8) process that is
accessible to all user accounts.  In relaunchd, there is a "system launchd"
process that runs as root and is not accessible by unprivileged users.  Each
unprivileged user may run their own launchd process under their own UID.

In the Darwin implementation, the StartCalendarInterval key can be a hash
or an array of hashes. In relaunchd, only a hash is acceptable.

In the Darwin implementation, the Umask key is a decimal integer.
In relaunchd, octal notation is used, but converted to a string.

## Static Analysis 

Coverity scan reports for relaunchd are available at:
https://scan.coverity.com/projects/mheily-relaunchd?tab=overview

When new releases are created, they will be submitted to Coverity
to re-run the static analyzer.

## Contact Information

For questions, comments, or other feedback about relaunchd, please join
the email discussion list at:

    https://groups.google.com/forum/#!forum/relaunchd-devel

There is also a #relaunchd IRC channel on FreeNode.

## Links

Here are some links to useful information about launchd:
- http://technologeeks.com/docs/launchd.pdf
- http://www.real-world-systems.com/docs/launchctl.1.html
- http://launchd.info/
- https://developer.apple.com/library/mac/technotes/tn2083/_index.html

## References

[1] https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man8/launchd.8.html
