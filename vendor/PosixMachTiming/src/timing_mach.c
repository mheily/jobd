#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include <time.h>
#include "timing_mach.h"

#ifdef __MACH__
/* ******** */
/* __MACH__ */

#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/clock.h>

/* timing struct for osx */
static struct TimingMach {
    mach_timebase_info_data_t timebase;
    clock_serv_t cclock;
} timing_mach_g;

/* mach clock port */
extern mach_port_t clock_port;

int timing_mach_init (void) {
    int retval = mach_timebase_info(&timing_mach_g.timebase);
    if (retval == 0) {
        retval = host_get_clock_service(mach_host_self(),
                                        CALENDAR_CLOCK, &timing_mach_g.cclock);
    }
    return retval;
}

int clock_gettime(clockid_t id, struct timespec *tspec) {
    mach_timespec_t mts;
    if (id == CLOCK_REALTIME) {
        if (clock_get_time(timing_mach_g.cclock, &mts) != 0)
            return -1;
        tspec->tv_sec = mts.tv_sec;
        tspec->tv_nsec = mts.tv_nsec;
    } else if (id == CLOCK_MONOTONIC) {
        if (clock_get_time(clock_port, &mts) != 0)
            return -1;
        tspec->tv_sec = mts.tv_sec;
        tspec->tv_nsec = mts.tv_nsec;
    } else {
        /* only CLOCK_MONOTOIC and CLOCK_REALTIME clocks supported */
        return -1;
    }
    return 0;
}

int clock_nanosleep_abstime(const struct timespec *req, struct timespec *rem) {
    struct timespec ts_delta;
    int retval = 0;
    retval = clock_gettime(CLOCK_MONOTONIC, &ts_delta);
    if (retval == 0) {
        timespec_monodiff (&ts_delta, req);
        retval = nanosleep(&ts_delta, rem);
    }
    return retval;
}

/* __MACH__ */
/* ******** */
#endif

/* no inline functions if not at least C99 */
#ifndef TIMING_C99
# define inline
#endif

/* timespec to double */
inline double timespec2secd(const struct timespec *ts_in) {
    return ((double) ts_in->tv_sec) + ((double) ts_in->tv_nsec ) * TIMING_NANO;
}

/* timespec difference (monotonic) */
inline void timespec_monodiff(struct timespec *ts_out,
                              const struct timespec *ts_in) {
    /* out = in - out,
       where in > out
     */
    ts_out->tv_sec = ts_in->tv_sec - ts_out->tv_sec;
    ts_out->tv_nsec = ts_in->tv_nsec - ts_out->tv_nsec;
    if (ts_out->tv_nsec < 0) {
        ts_out->tv_sec = ts_out->tv_sec - 1;
        ts_out->tv_nsec = ts_out->tv_nsec + TIMING_GIGA;
    }
}

/* timespec addition (monotonic) */
inline void timespec_monoadd(struct timespec *ts_out,
                             const struct timespec *ts_in) {
    /* out = in + out,
       where in > out
     */
    ts_out->tv_sec = ts_out->tv_sec + ts_in->tv_sec;
    ts_out->tv_nsec = ts_out->tv_nsec + ts_in->tv_nsec;
    if (ts_out->tv_nsec >= TIMING_GIGA) {
        ts_out->tv_sec = ts_out->tv_sec + 1;
        ts_out->tv_nsec = ts_out->tv_nsec - TIMING_GIGA;
    }
}

/* clean up define 'inline' */
#ifndef TIMING_C99
# undef inline
#endif
