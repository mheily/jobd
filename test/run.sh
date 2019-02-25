#!/bin/sh -ex

cleanup() {
    [ -z "$tail_pid" ] || kill $tail_pid
    [ -z "$jobd_pid" ] || kill $jobd_pid
}

err() {
    echo "ERROR: $*"
    exit 1
}

assert_contains() {
	msg="$*"
	echo "assert_contains: waiting for: ${msg}"
	for x in $(seq 1 10) ; do
		grep -q "$msg" $logfile && break || true
		sleep 1
	done
	grep -q "$msg" $logfile || err "unexpected response"
	echo "assert_contains: success: ${msg}"
}

trap cleanup EXIT

#on linux: echo '/tmp/core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -H -c unlimited >/dev/null
ulimit -S -c unlimited >/dev/null

objdir="./test/obj"
./test/build.sh

. ./config.inc

logfile="$LOCALSTATEDIR/log/$PROJECT_NAME/boot.log"

install test/job.d/* $DATAROOTDIR/$PROJECT_NAME/manifests
$BINDIR/jobcfg -f test/job.d -v import

set +x
touch $logfile
tail -f $logfile &
tail_pid=$!
$objdir/sbin/jobd -fvv > $logfile 2>&1 &
jobd_pid=$!


# Test if a job finishes
assert_contains 'job sleep1 .* exited'

# Test IPC
$objdir/bin/jobadm jobd reopen_database
$objdir/bin/jobadm enable_me enable
assert_contains 'job enable_me has been enabled'
assert_contains 'job enable_me .* exited'
$objdir/bin/jobadm enable_me disable
assert_contains 'job enable_me has been disabled'

# Disable a running job
$objdir/bin/jobadm disable_me disable
assert_contains 'job disable_me has been disabled'
assert_contains 'sending SIGTERM to job disable_me'

kill $jobd_pid
assert_contains 'sending SIGTERM to job shutdown_handler'

printf 'waiting for jobd to terminate.. '
for i in $(seq 1 10) ; do
    kill -0 $jobd_pid 2>/dev/null || break
    sleep 1
done
kill -0 $jobd_pid 2>/dev/null && err 'jobd still running' || true
jobd_pid=""
echo 'done'

# Run the jobstat command
$objdir/bin/jobstat >> $logfile 2>&1
assert_contains 'Label'

printf "\n\nSUCCESS: All tests passed.\n"
exit 0
