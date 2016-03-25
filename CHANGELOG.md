# Change Log
All notable changes to this project will be documented in this file
using the [Keep A ChangeLog](http://keepachangelog.com/) style.
This project adheres to [Semantic Versioning](http://semver.org/).

## [0.4.3] - Unreleased
### Fixed
- Fix a build failure on i386.
- Fix incorrect detection of --mandir and --sysconfdir in the ./configure script
- Fix handling of stale pidfiles
- Fix a crash when a job with a duplicate label is loaded. 

### Added
- Some experimental PC-BSD plist files have been added to the manifests/ directory.

## [0.4.2] - 2016-03-20
### Fixed
- Fix a bug in the init scripts that caused /usr/local/share/launchd/daemons
  to be ignored.
- Fix a bug in the 'make install' target that caused manpages to be installed
  in the wrong place.
- Create directories automatically during 'make install'.
- Stop requiring autoconf/automake to build libucl.

## [0.4.1] - 2016-03-18
### Security
- This release is the first to be checked by static analysis tools;
  specifically Coverity Scan and clang-analyzer. These tools found a
  small number of bugs related to memory management. Users are encouraged 
  to upgrade to this release for enhanced security and reliability.

### Added
- Continuous Integration testing is now performed via TravisCI.
- Support for building on OpenBSD and NetBSD was added. These platforms
  have not been extensively tested, so YMMV.
- Add support for the Umask job key described in launchd.plist(5)
- Add support for the RunAtLoad job key described in launchd.plist(5)
- A partial implementation of calendar scheduling has been written. It
  does not currently work at all, but many of the pieces are there.

### Changed
- The JSMN parser has been replaced by [libucl](https://github.com/vstakhov/libucl).
  This will allow jobs to be specified using JSON, YAML, or Nginx-style configuration.
- Jobs will need to specify the RunAtLoad key to run automatically. This
  behavior more closely matches the behavior of Apple's launchd.
- Logging output now goes to syslog(3) rather than to a custom file.
- The build system has been overhauled to use a "configure" script similar
  to GNU autoconf.
- launchd(8) will no longer automatically run all jobs after they are loaded.
- launchd(8) will no longer automatically load jobs when it starts.
  Instead, it relies on launchctl(1) to load the appropriate jobs. This
  more closely matches the behavior of Apple's launchd.
- The source code has been heavily reorganized, with files grouped into 
  subdirectories.
- libucl and libkqueue are bundled with relaunchd, and will be compiled
  if the host system does not have these libraries.

### Fixed
- Fixed several resource leaks detected by Coverity.
- Fix a lack of default redirection of standard I/O to /dev/null.

### Removed
- The undocumented and incomplete "install/uninstall" modes of launchctl
  have been removed.

## [0.3.1] - 2015-12-06
### Added
- New jail management code, not yet enabled into the default build
- Support for systems without SOCK_CLOEXEC

### Changed
- Stop building the sa-wrapper.so library by default
- Explicitly add C99 mode to the CFLAGS

### Fixed
- Compiles cleanly on FreeBSD 9.3 now.

## [0.3.0] - 2015-12-03
### Changed
- Replace the custom pidfile handling code with code borrowed from libutil.
- Stop using /.launchd and write logs to /var/log, pidfiles to /var/run, and
  everything else to /var/{db,lib}/launchd

## [0.2.1] - 2015-12-01
### Fixed
- Manpage references to paths in the FILES section
- Do not install sa-wrapper.so by default

## [0.2.0] - 2015-12-01
### Added
- Experimental bind(2) wrapper library that allows you to use
socket activation with binaries that don't support it.
- A config.h file with portability macros
- Manual pages imported from the ruby-relaunchd project
- regularly scheduled jobs via the StartInterval key
- Experimental support for compiling under Linux
- Support for setting the nice value
- Experimental support for launching programs in a FreeBSD jail
- Socket activation (incomplete, but handles the typical use case of a TCP socket)
- The ability to unload jobs with launchctl

### Fixed
- Parsing of environment variables

## [0.1.2] - 2015-11-02
- Initial public release
