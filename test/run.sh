#!/bin/sh -ex

#on linux: echo '/tmp/core_%e.%p' | sudo tee /proc/sys/kernel/core_pattern
ulimit -H -c unlimited >/dev/null
ulimit -S -c unlimited >/dev/null

test -e jobcfg
rm -f ~/.local/share/jmf/repository.db
./jobcfg init
./jobcfg import -f test/job.d
#sqlite3 ~/.local/share/jmf/repository.db .schema
#sqlite3 ~/.local/share/jmf/repository.db .dump
sqlite3 -header ~/.local/share/jmf/repository.db 'select * from jobs'
sqlite3 -header ~/.local/share/jmf/repository.db 'select * from job_methods'
