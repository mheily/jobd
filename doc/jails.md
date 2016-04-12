# Jail support

Experimental support for jails.

## Manifest language

Jail
This key is a dictionary that contains keys related to
launching programs within a jail.

Name
The jail name. This cannot contain dots.

Hostname
The hostname within the jail, set via sethostname(3).

Machine
The machine type. Currently only 'i386' and 'amd64' are supported.

Release
The full name of the FreeBSD release as reported by `uname -r`.

## Example

The following job manifest will create a jail, install the thttpd package,
and launch thttpd inside of the jail. When an incoming connection arrives
at port 80 on the host, it will be transparently forwarded to the jail.
There is no need to setup NAT, bridging, or assign an IP address to the jail.

{
	"Label": "com.example.jail-test",
	"ProgramArguments": ["/usr/local/sbin/thttpd", "-D"],
	"Packages": ["thttpd"],
	"Jail": {
		"Name": "com_example_jail-test",
		"Hostname": "jail-test",
		"Machine": "amd64",
		"Release": "10.3-RELEASE",
	},
	"Sockets": {
		"thttpd": {
			"SockServiceName": "80",
		},
	},
}
