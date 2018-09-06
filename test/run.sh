#!/bin/sh -ex

err() {
    echo "ERROR: $*"
    exit 1
}

#on linux: echo '/tmp/core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -H -c unlimited >/dev/null
ulimit -S -c unlimited >/dev/null

test -e jobcfg
rm -f ~/.local/share/jmf/repository.db
./jobcfg init -f schema.sql
./jobcfg import -f test/job.d
#sqlite3 ~/.local/share/jmf/repository.db .schema
#sqlite3 ~/.local/share/jmf/repository.db .dump
sqlite3 -header ~/.local/share/jmf/repository.db 'select * from jobs'
sqlite3 -header ~/.local/share/jmf/repository.db 'select * from job_methods'

./jobd -fv >test.log 2>&1 &
pid=$!
for x in $(seq 1 10) ; do
    grep -q 'job sleep1 .* exited' test.log && break || true
    sleep 1
done
grep -q 'job sleep1 .* exited' test.log || err "unexpected response"
kill $pid
