#!/bin/sh 
./test/build.sh && gdb -ex 'r -fv' ./test/obj/sbin/jobd
