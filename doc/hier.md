# Directory hierarchy

## XDG variables

Non-root users respect XDG variables and provide defaults:

  XDG_RUNTIME_DIR ||= /tmp/jobd-$USER
  XDG_CONFIG_HOME ||= $HOME/.config
  XDG_DATA_HOME ||= $HOME/.config

Root user ignores XDG variables and hardcodes them to:

  XDG_RUNTIME_DIR = $runstatedir, typically /var/run
  XDG_CONFIG_HOME = $sysconfdir, typically /usr/local/etc
  XDG_DATA_HOME = /var/db/jobd

For all users, jobd data is stored under $XDG_*/jobd

see https://wiki.archlinux.org/index.php/XDG_Base_Directory_support

## jobd directories

$XDG_DATA_HOME/jobd = dataDir - persistent data storage
  ||
  property/ = jobPropertyDataDir - enabled/disabled, faults, and user-defined properties
  ??? manifest/ = UCL manifests auto-loaded when jobd starts up
   
$XDG_RUNTIME_DIR/jobd = runtimeDir - transient data storage
  ||
  job/ = jobManifestDir - normalized, emitted JSON of loaded jobs
  status/ = jobStatusDir - PID, status info for each job
  ipc.sock = socketPath - IPC socket
  jobd.pid = pidfilePath - PID of running jobd

## Autoloading

Manifests in jobdConfig::getAutoloadDir() are loaded when
the jobd process starts.

DILEMMA: have a /usr/share/$manifest and /usr/local/etc/$manifest ??
are the properties stored in the manifest editable after the job loads?

PROBLEM: conflicts with idea of load job once, persists across a reboot

## Other

Directories are created on demand, with appropriate permissions.

