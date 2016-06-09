/***
  This file is part of PulseAudio.

  Copyright 2008 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include <sys/param.h>
#include <sys/cpuset.h>
#endif
#endif
#endif

#include <pulse/util.h>
#include <pulse/timeval.h>
#include <pulse/gccmacro.h>

#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/thread.h>
#include <pulsecore/core-util.h>
#include <pulsecore/core-rtclock.h>

static int msec_lower, msec_upper;

static void work(void *p) PA_GCC_NORETURN;

static void work(void *p) {

    pa_log_notice("CPU%i: Created thread.", PA_PTR_TO_UINT(p));

    pa_make_realtime(12);

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
{
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    cpuset_t mask;
#else
    cpu_set_t mask;
#endif

    CPU_ZERO(&mask);
    CPU_SET((size_t) PA_PTR_TO_UINT(p), &mask);
    pa_assert_se(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == 0);
}
#endif

    for (;;) {
        struct timeval now, end;
        uint64_t usec;

        pa_log_notice("CPU%i: Sleeping for 1s", PA_PTR_TO_UINT(p));
        pa_msleep(1000);

        usec =
            (uint64_t) ((((double) rand())*(double)(msec_upper-msec_lower)*PA_USEC_PER_MSEC)/RAND_MAX) +
            (uint64_t) ((uint64_t) msec_lower*PA_USEC_PER_MSEC);

        pa_log_notice("CPU%i: Freezing for %ims", PA_PTR_TO_UINT(p), (int) (usec/PA_USEC_PER_MSEC));

        pa_rtclock_get(&end);
        pa_timeval_add(&end, usec);

        do {
            pa_rtclock_get(&now);
        } while (pa_timeval_cmp(&now, &end) < 0);
    }
}

int main(int argc, char*argv[]) {
    unsigned n;

    pa_log_set_level(PA_LOG_INFO);

    srand((unsigned) time(NULL));

    if (argc >= 3) {
        msec_lower = atoi(argv[1]);
        msec_upper = atoi(argv[2]);
    } else if (argc >= 2) {
        msec_lower = 0;
        msec_upper = atoi(argv[1]);
    } else {
        msec_lower = 0;
        msec_upper = 1000;
    }

    pa_assert(msec_upper > 0);
    pa_assert(msec_upper >= msec_lower);

    pa_log_notice("Creating random latencies in the range of %ims to %ims.", msec_lower, msec_upper);

    for (n = 1; n < pa_ncpus(); n++) {
        pa_assert_se(pa_thread_new("rtstutter", work, PA_UINT_TO_PTR(n)));
    }

    work(PA_INT_TO_PTR(0));

    return 0;
}
