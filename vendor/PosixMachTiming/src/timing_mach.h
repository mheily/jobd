#ifndef TIMING_MACH_H
#define TIMING_MACH_H
/* ************* */
/* TIMING_MACH_H */

/* C99 check */
#if defined(__STDC__)
# if defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 199901L)
#   define TIMING_C99
#  endif
# endif
#endif

#include <time.h>

#define TIMING_GIGA (1000000000)
#define TIMING_NANO (1e-9)

/* helper function prototypes */
extern double timespec2secd(const struct timespec *ts_in);
extern void timespec_monodiff(struct timespec *ts_out,
                              const struct timespec *ts_in);
extern void timespec_monoadd(struct timespec *ts_out,
                             const struct timespec *ts_in);

#ifdef __MACH__
/* ******** */
/* __MACH__ */

/* only CLOCK_REALTIME and CLOCK_MONOTONIC are emulated */
#ifndef CLOCK_REALTIME
# define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
# define CLOCK_MONOTONIC 1
#endif

/* typdef POSIX clockid_t */
typedef int clockid_t;

/* initialize mach timing */
int timing_mach_init (void);

/* clock_gettime - emulate POSIX */
int clock_gettime(const clockid_t id, struct timespec *tspec);
/* clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
int clock_nanosleep_abstime(const struct timespec *req,
                            struct timespec *rem);

/* __MACH__ */
/* ******** */
#else
/* ***** */
/* POSIX */

/* clock_nanosleep for CLOCK_MONOTONIC and TIMER_ABSTIME */
#ifdef TIMING_C99
static inline int clock_nanosleep_abstime(const struct timespec *req, struct timespec *rem) {
    return clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, req, rem);
}
#else
# define clock_nanosleep_abstime(req,rem) \
         clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, (req), (rem))
#endif

/* POSIX */
/* ***** */
#endif

/* TIMING_MACH_H */
/* ************* */
#endif
