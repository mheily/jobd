#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include "timing_mach.h"
#include <stdio.h>
#include <inttypes.h>

/* in source folder, build and run with command:
   clang timing*.c -o timing_test && ./timing_test
*/

int main() {

    int64_t epoch;
    struct timespec before, after;
    double sum, elapsed, waittime;
    unsigned u;

    /* initialize mach timing */
    #ifdef __MACH__
    timing_mach_init();
    #endif

    /* display epoch time */
    clock_gettime(CLOCK_REALTIME, &before);
    epoch = (int64_t) before.tv_sec;
    printf("\nEpoch Time is %" PRId64 " seconds \n", epoch);

    /* time something */
    printf("\nPerforming some math operations (sum) ... \n");
    clock_gettime(CLOCK_MONOTONIC, &before);
    sum = 0.0;
    for(u = 1; u < 100000000; u++){
        sum += 1./u/u;
    }
    clock_gettime(CLOCK_MONOTONIC, &after);
    /* print result */
    printf("Done. (result is: sum = %g)\n", sum);
    timespec_monodiff (&before, &after);
    elapsed = timespec2secd(&before);
    printf("Elapsed Time = %e s\n", elapsed);

    /* absolute time nanosleep */
    after.tv_nsec = 0;
    after.tv_sec = 2;
    waittime = timespec2secd(&after);
    printf("\nWait %g seconds ... \n", waittime);
    clock_gettime(CLOCK_MONOTONIC, &before);
    timespec_monoadd(&after, &before);
    clock_nanosleep_abstime(&after, NULL);
    clock_gettime(CLOCK_MONOTONIC, &after);
    printf("Done waiting.\n");
    /* print result */
    timespec_monodiff (&before, &after);
    elapsed = timespec2secd(&before);
    printf("Elapsed Time = %e s\n", elapsed);

    /* display epoch time */
    clock_gettime(CLOCK_REALTIME, &before);
    epoch = (int64_t) before.tv_sec;
    printf("\nEpoch Time is %" PRId64 " seconds \n", epoch);

}
