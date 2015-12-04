# Change Log
All notable changes to this project will be documented in this file
using the [Keep A ChangeLog](http://keepachangelog.com/) style.
This project adheres to [Semantic Versioning](http://semver.org/).

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
