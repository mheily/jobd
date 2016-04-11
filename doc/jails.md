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
The full name of the FreeBSD release.

## Example

{
	"Label": "org.example.jailtest",
	"Jail": {
		"Name": "org_example_jailtest",
		"Hostname": "jailtest",
		"Machine": "amd64",
		"Release": "10.3-RELEASE",
	},
}
