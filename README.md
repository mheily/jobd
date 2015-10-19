## relaunchd

** Overview **

relaunchd is a service management daemon that implements a subset of the launchd(8) API[1]. 

It was written from scratch based on the published API, and all code is available
under the ISC license. See the LICENSE file for more information.

It is currently under heavy development, and should not be used for anything important.
Be especially mindful that there is NO WARRANTY provided with this software.

** Status **

The core functionality is working
* loading JSON-formatted jobs with launchctl
* launching jobs that use these keys:
  * Label, Program, ProgramArguments, WorkingDirectory, EnvironmentVariables
  * StandardInPath, StandardOutPath, StandardErrorPath 

Some things are not implemented yet:
* anything that requires root privileges, such as UserName or RootDirectory
* unloading jobs, or listing their status
* cron functionality - StartInterval, StartCalendarInterval
* on-demand activation - Sockets, StartOnMount
* file and directory watches - WatchPaths, QueueDirectories
* restarting jobs if they crash - TimeOut, ExitTimeout, KeepAliveTimeout, ThrottleInterval
* resource limits - SoftResourceLimits, HardResourceLimits
* miscellaneous - LaunchOnlyOnce, inetdCompatibility, EnableGlobbing,
  RunAtLoad, Umask

Some things will probably never be implemented:
* oddities - LimitLoadToHosts, LimitLoadToHosts
* kernel and launchd debugging - Debug, WaitForDebugger
* Mach IPC
* the original XML plist format; use JSON instead.
* hacks and workarounds - HopefullyExitsFirst, HopefullyExitsLast
* Darwin-specific things - EnableTransactions
* legacy keys - Disabled, OnDemand

## References

[1] https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man8/launchd.8.html
