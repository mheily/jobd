# Integrating relaunchd with PC-BSD

These instructions will help you use relaunchd to run various components of
the PC-BSD desktop under the Fluxbox window manager. These components include:

 * pc-mixer
 * pc-mounttray
 * pc-systemupdatertray
 * life-preserver-tray
 * xscreensaver

These instructions are intended for advanced users and PC-BSD developers **only**. This is an experimental setup that most people will not want to use.

## Instructions
1. Download the latest version of relaunchd. To grab this from GitHub, run:
```
	git clone https://github.com/mheily/relaunchd.git
	cd relaunchd
	./configure
	make
	sudo make install
```
1. Copy the JSON files for PC-BSD services into launchd's configuration directories.
```
cd manifests/pcbsd
sudo cp agents/* /usr/local/etc/launchd/agents/
sudo cp daemons/* /usr/local/etc/launchd/daemons/
```
1. Enable and start the launchd service (requires becoming root):
```
	# echo 'launchd_enable=YES' >>  /etc/rc.conf
	# service launchd start
```
1. As your normal user account, edit your ~/.xprofile file and comment out the following section:
```
	# Check for any specific PC-BSD apps we need to start at login
	for i in $(ls /usr/local/share/pcbsd/xstartup/*.sh)
	do
	       # Run some task in the background to run in parallel
	       if [ $i = "/usr/local/share/pcbsd/xstartup/enable-ibus.sh" ] || [ $i = "/usr/local/share/pcbsd/xstartup/gpg-agent.sh" ] || [ $i = "/usr/local/share/pcbsd/xstartup/ssh-agent.sh" ] ; then
	               # Cannot work "VARIABLE=value ; export VARIABLE" in the
	               # fork(2)ed script.
	               if [ -x "${i}" ] ; then . ${i}
	               fi
	       else
	       if [ -x "${i}" ] ; then (. ${i}) & 
	       fi
	fi
	done
```
1. Add the following line to the bottom of your ~/.xprofile
```
	launchctl load ~/.launchd/agents /usr/local/etc/launchd/agents /usr/local/share/launchd/agents
```
1. Logout of your normal desktop, and log back in using the Fluxbox session type in PCDM.
1. You should see all of the typical PC-BSD tray applets launch.
1. Start a terminal window and look at the output of "launchctl list". It should look something like:
```
	$ launchctl list
	PID      Status   Label
	-        0        org.pcbsd.disable-beep
	-        0        org.pcbsd.enable-xprofile
	1432     0        org.pcbsd.pc-mixer
	1429     0        org.pcbsd.pc-mounttray
	1428     0        org.pcbsd.pc-systemupdatertray
	1424     0        org.pcbsd.life-preserver-tray
	-        0        org.pcbsd.enable-ibus
	-        0        org.pcbsd.ssh-agent
	1421     0        org.pcbsd.xscreensaver
	-        0        org.pcbsd.gpg-agent
```

## Warnings

Be aware of the following:

 * Using launchd with PC-BSD is totally unsupported and your system might not work correctly if you follow these instructions.
 * If you don't use Fluxbox for your normal PC-BSD desktop, following these instructions will break certain things when
you are not running Fluxbox. Basically, you will need to revert your changes to ~/.xprofile before switching back to your
normal desktop.
 * Some of the org.pcbsd jobs are broken. Specifically:
  * ssh-agent and gpg-agent don't start up in the same shell context as ~/.xprofile
and are not able to inject the SSH_AUTH_SOCK environment variable into the user's
environment. 
  * pc-systemupdatertray might show an error of "Unable to determine system status".

