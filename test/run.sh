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
		grep -q "$msg" test.log && break || true
		sleep 1
	done
	grep -q "$msg" test.log || err "unexpected response"
	echo "assert_contains: success: ${msg}"
}

trap cleanup EXIT

#on linux: echo '/tmp/core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -H -c unlimited >/dev/null
ulimit -S -c unlimited >/dev/null

objdir="./test/obj"
./test/build.sh

install test/job.d/* ./test/obj/share/jmf/manifests
$objdir/bin/jobcfg -f test/job.d -v import
#sqlite3 ~/.local/share/jmf/repository.db .schema
#sqlite3 ~/.local/share/jmf/repository.db .dump
#sqlite3 -header ~/.local/share/jmf/repository.db 'select * from jobs'
#sqlite3 -header ~/.local/share/jmf/repository.db 'select * from job_methods'


set +x
echo "# Test log started on $(date)" > test.log
tail -f test.log &
tail_pid=$!
$objdir/sbin/jobd -fv >>test.log 2>&1 &
jobd_pid=$!


# Test if a job finishes
assert_contains 'job sleep1 .* exited'

# Test IPC
$objdir/bin/jobadm enable_me enable
assert_contains 'job enable_me has been enabled'
assert_contains 'job enable_me .* exited'
$objdir/bin/jobadm enable_me disable
assert_contains 'job enable_me has been disabled'

# Disable a running job
$objdir/bin/jobadm disable_me disable
assert_contains 'job disable_me has been disabled'
assert_contains 'sending SIGTERM to job disable_me'

printf "\n\nSUCCESS: All tests passed.\n"
exit 0
