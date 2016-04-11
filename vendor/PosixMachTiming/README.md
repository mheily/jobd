# PosixMachTiming #
A tiny program to partially emulate POSIX `clock_nanosleep()` and 
`clock_gettime()` for mac OS X.

## Build ##
There is an extremely simple test program in the 
[src](https://github.com/ChisholmKyle/PosixMachTiming/tree/master/src) folder
you can compile and run from the command line. For example:

    clang timing*.c -o timing_test && ./timing_test

should output something like

    Epoch Time is 1439596916 seconds

    Performing some math operations (sum) ...
    Done. (result is: sum = 1.64493)
    Elapsed Time = 1.000011e+00 s

    Wait 2 seconds ...
    Done waiting.
    Elapsed Time = 2.000011e+00 s

    Epoch Time is 1439596919 seconds
