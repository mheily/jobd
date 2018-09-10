#!/bin/sh -ex

cleanup() {
    [ -z "$tail_pid" ] || kill $tail_pid
    [ -z "$jobd_pid" ] || kill $jobd_pid
}

err() {
    echo "ERROR: $*"
    exit 1
}

trap cleanup EXIT

#on linux: echo '/tmp/core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -H -c unlimited >/dev/null
ulimit -S -c unlimited >/dev/null

objdir="./test/obj"
rm -rf $objdir
mkdir -p $objdir
make distclean
PREFIX=$objdir ./configure
make all -j6
make install

install test/job.d/* ./test/obj/share/jmf/manifests
$objdir/bin/jobcfg -v init
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
for x in $(seq 1 10) ; do
    grep -q 'job sleep1 .* exited' test.log && break || true
    sleep 1
done
grep -q 'job sleep1 .* exited' test.log || err "unexpected response"
