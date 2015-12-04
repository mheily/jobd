Could relaunchd offer a mechanism to create capsicum(4) sandboxes?
-----------------------------------------------------------------

  It would be possible to extend launchd(8) to give it the ability to
initialize the sandbox for programs that are Capsicum aware. This would improve
the overall security of programs which use Capsicum.

  The main benefit of adding Capsicum support to launchd is that the
capsicum(4) security policy can be viewed and audited by the end user or system
administrator, without requiring them to look at the source code or trust the
person who compiled the program. The security policy is in a separate document,
that is separate from the source code and the binary excutable.

  If someone puts a backdoor in the source code of a daemon, having a separate
security policy managed by launchd will thwart the effectiveness of the
backdoor. You no longer have to trust that the code you are running actually
puts itself into a sandbox; instead, you can trust that launchd initializes the
sandbox according to the policy that you can audit fairly easily.

  This would require some kind of user interaction when the security policy of
a program changes, similar to how smartphones ask you to confirm changes to app
permissions when you upgrade to a new version of an app.

 You could execute an untrusted binary without having the source code,
  and be confident that the binary will run in a sandbox as described in the
launchd.plist(5) file. 

## Implementation

The basic idea is that programs could add Capsicum directives to their
launchd.plist(5) job specifications. Here's an example:

```C
{
  "Label": "mydaemon",
  "Program": "/usr/local/sbin/mydaemon",
  "User": "nobody",
  "Group": "nogroup",
  "Capsicum": {
    "CapabilitySet": {
      "kq_fd": {
        "SystemCall":["kqueue"], 
        "Rights": ["CAP_KQUEUE"]},
      },
      "resolv_fd":{
        "SystemCall": ["open", "/etc/resolv.conf", "O_RDONLY"], 
        "Rights": ["CAP_READ", "CAP_SYNC", "CAP_EVENT"],
      },
      "data_fd": {
        "SystemCall": ["open", "/var/mydata", "O_RDONLY"], 
        "Rights": ["CAP_CREATE"],
      },
    }
  }
}
```

When launchd starts the job, it would translate the above
job manifest into the following code and execute it:

```C
   int fd[3];

   fd[0] = kqueue();
   fd[1] = open("/etc/resolv.conf", O_RDONLY);
   fd[2] = open("/var/mydata", O_RDONLY);
   if (fork() == 0) {
        cap_rights_t setrights;

        cap_rights_init(&setrights, CAP_KQUEUE);
        cap_rights_limit(fd[0], &setrights);

        cap_rights_init(&setrights, CAP_READ | CAP_SYNC | CAP_EVENT);
        cap_rights_limit(fd[1], &setrights);

        cap_rights_init(&setrights, CAP_CREATE);
        cap_rights_limit(fd[2], &setrights);

        cap_enter();
        
        execve("/usr/local/sbin/mydaemon", NULL, NULL);
    }
```
   
The 'mydaemon' executable would be linked with librelaunch,
which would provide a function for retrieving the file descriptors
by looking up their key. There could also be a fallback mechanism
in case the program was built without support for librelaunch.

Here's what it would look like, roughly:

```C
#if USE_LIBRELAUNCH
# include <relaunch.h>
#else
# define launch_capsicum_fd(_id) (-1)
#endif

int kq_fd, resolv_fd, data_fd;

kq_fd = launch_get_cap_fd("kq_fd");
if (kq_fd < 0) {
        kq_fd = kqueue();
}

resolv_fd = launch_get_cap_fd("resolv_fd");
if (resolv_fd < 0) {
        resolv_fd = open("/etc/resolv.conf", O_RDONLY);
}

data_fd = launch_get_cap_fd("data_fd");
if (data_fd < 0) {
        data_fd = open("/var/mydata", O_RDONLY);
}
```
