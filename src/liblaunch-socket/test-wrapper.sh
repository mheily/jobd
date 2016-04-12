#!/bin/sh
cd ..
make clean launchd-debug
cd sa-wrapper
../launchd -fv &
sleep 2
tail -f ~/.launchd/launchd.log &
./setup-plist.rb
sleep 2
message=`curl http://localhost:8088`
if [ $? -ne 0 ] ; then
        echo fail
        retval=1
else
        if [ "x$message" = "xhello world" ] ; then
                echo success
                retval=0
        else
                echo "fail (mismatching message)"
                retval=2
        fi
fi
kill %1
kill %2
echo "response from the server"
echo $message
echo "standard output of the job:"
cat test-wrapper.out
echo "standard error of the job:"
cat test-wrapper.err
rm -f *.out *.err
exit $retval
