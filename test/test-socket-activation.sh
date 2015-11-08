#!/bin/sh -x
cd ..
make launchd-debug
cd test
../launchd -fv &
sleep 2
tail -f ~/.launchd/launchd.log &
./socket-activation.rb setup
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
echo "standard output of the job:"
cat socket-activate.out
echo "standard error of the job:"
cat socket-activate.err
rm -f socket-activate.out socket-activate.err
exit $retval
