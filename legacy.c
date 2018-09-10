// interfacing w/ legacy init systems
//

freebsd:
rcorder -s nostart -s firstboot /etc/rc.d/* /usr/local/etc/rc.d/*


add to /etc/rc to divert to jobd:

--- /tmp/rc.OLD	2018-09-07 18:37:15.391305000 +0000
+++ /tmp/rc.NEW	2018-09-07 18:44:19.723292000 +0000
@@ -71,6 +71,11 @@
 . /etc/rc.subr
 load_rc_config

+# Allow jobd(8) to manage the rest of the boot process, if desired.
+if [ "$1" != "nojobd" -a "$use_jobd" = "YES" ] ; then
+	exec /lib/jmf/bin/jobd
+fi
+
 # If we receive a SIGALRM, re-source /etc/rc.conf; this allows rc.d
 # scripts to perform "boot-time configuration" including enabling and
 # disabling rc.d scripts which appear later in the boot order.

---cut------

					    		
first script is sysctl. replace /etc/sysctl.conf and /etc/sysctl.conf.local
with jobcfg. need arbitrary key/val pairs for mib settings
