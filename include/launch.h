#ifndef LAUNCH_H_
#define LAUNCH_H_

/*
   		Stateful notification functions
		===============================

	These functions are similar to the notify(3) API found in Darwin.

	Before using these functions, application developers should add the
	appropriate keys to their launchd job manifest.

	Example:

	{ 
	"Label": "foo",
	"Program": "/bin/true",
	"Notifications": {
		"Send":	[
			"foo.status",
			"foo.backlog",
			],
		"Receive": [ 
			"bar.status",
			],
		"LaunchAfter": [ "bar.status=running" ],
		"ExitIf": [ "bar.status=stopped" ],
		}
	}
	
	In the above example, the job 'foo' will be started with the ability
	to send notifications about two items: 'status' and 'backlog'. It will
	be able to receive notifications about the 'status' of the 'bar' job.

	Additionally, the 'foo' job will not be launched until after the 'bar' job
	has sent a notification that it's 'status' is 'running'. If the 'bar'
	job posts a notification that it's 'status' is 'stopped', launchd will
	terminate the 'foo' job by sending it a SIGTERM signal.

*/

/** A notification about a state change. */
struct notify_state {
  char   *ns_name;	
  char   *ns_state;	/* The current state */
  size_t  ns_len;	/* The amount of data stored in ns_state */
};
typedef struct notify_state * notify_state_t;

/** 
  Post a notification about *name* and update the *state*.

  @param name	The name to generate a notification for
  @param state	The new state to report
  @param len	The length of the *state* variable

  @return 0 if successful, or -1 if an error occurs.
*/
int notify_post(const char *name, void *state, size_t len);

/** 
  Check for pending notifications, and return the current state.

  @param changes A buffer to write the notifications to
  @param nchanges The number of changes that can be stored in the buffer

  @return the number of notifications retrieved,
	  0 if no notifications are available,
	  or -1 if an error occurs.
*/
ssize_t notify_check(notify_state_t *changes, size_t nchanges);

/**
  Get a file descriptor that can be monitored for readability.
  When one more notifications are pending, the file descriptor will
  become ready for reading. This descriptor can be added to your
  application's event loop.

  This descriptor could be used with libdispatch by following
  this example:

	dispatch_source_t source = dispatch_source_create(
		DISPATCH_SOURCE_TYPE_READ, notify_get_fd(),
		0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(source, ^{
	    char *name, *state;
	    ssize_t count;
	    notify_state_t changes;

	    count = notify_check(&changes, 1);
	    if (count == 1 && strcmp(changes.ns_name, "foo") == 0) {
	       printf("the current state of foo is %s\n", changes.ns_state);
	    }
    	});
    	dispatch_resume(source);
	
  @return a file descriptor, or -1 if an error occurred.
*/
int notify_get_fd(void);

/**
  Submit a block to be executed when one or more notifications are pending.

  This is basically a convenience function that implements the example code
  shown in the documentation for notify_get_fd(). 

  You must include <dispatch/dispatch.h> and compile with -fblocks to have
  access to this function.

  @param name the name of the notification to wait for
  @param queue the dispatch queue to run the block on
  @param block the block of code to be executed
*/
#if defined(__block) && defined(dispatch_queue_t)
void notify_dispatch(char *name, dispatch_queue_t queue, void (^block)(void));
#endif

/**
  Execute a callback function when one or more notifications are pending.

  This is equivalent to notify_dispatch(), but without using blocks.
*/
#ifdef dispatch_queue_t
void notify_dispatch_f(char *name, dispatch_queue_t queue, void *context, void (*func)(void *));
#endif

/**
  Disable notifications related to *name*.

  @return 0 if successful, or -1 if an error occurs.
*/
int notify_suspend(const char *name);

/**
  Resume notifications related to *name*

  @return 0 if successful, or -1 if an error occurs.
*/
int notify_resume(const char *name);

#endif /* LIBLAUNCH_H_ */
