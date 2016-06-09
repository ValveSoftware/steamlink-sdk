/***
  This file is part of PulseAudio.

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include <check.h>

#include <pulsecore/thread.h>
#include <pulsecore/once.h>
#include <pulsecore/log.h>
#include <pulsecore/core-util.h>
#include <pulsecore/atomic.h>
#include <pulse/xmalloc.h>

static pa_once once = PA_ONCE_INIT;
static volatile unsigned n_run = 0;
static const char * volatile ran_by = NULL;
#ifdef HAVE_PTHREAD
static pthread_barrier_t barrier;
#endif
static unsigned n_cpu;

#define N_ITERATIONS 500
#define N_THREADS 100

static void once_func(void) {
    n_run++;
    ran_by = (const char*) pa_thread_get_data(pa_thread_self());
}

static void thread_func(void *data) {
#ifdef HAVE_PTHREAD
    int r;

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    static pa_atomic_t i_cpu = PA_ATOMIC_INIT(0);
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    cpuset_t mask;
#else
    cpu_set_t mask;
#endif

    CPU_ZERO(&mask);
    CPU_SET((size_t) (pa_atomic_inc(&i_cpu) % n_cpu), &mask);
    fail_unless(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == 0);
#endif

    pa_log_debug("started up: %s", (char *) data);

    r = pthread_barrier_wait(&barrier);
    fail_unless(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);
#endif /* HAVE_PTHREAD */

    pa_run_once(&once, once_func);
}

START_TEST (once_test) {
    unsigned n, i;

    n_cpu = pa_ncpus();

    for (n = 0; n < N_ITERATIONS; n++) {
        pa_thread* threads[N_THREADS];

#ifdef HAVE_PTHREAD
        fail_unless(pthread_barrier_init(&barrier, NULL, N_THREADS) == 0);
#endif

        /* Yes, kinda ugly */
        pa_zero(once);

        for (i = 0; i < N_THREADS; i++)
            threads[i] = pa_thread_new("once", thread_func, pa_sprintf_malloc("Thread #%i", i+1));

        for (i = 0; i < N_THREADS; i++)
            pa_thread_join(threads[i]);

        fail_unless(n_run == 1);
        pa_log_info("ran by %s", ran_by);

        for (i = 0; i < N_THREADS; i++) {
            pa_xfree(pa_thread_get_data(threads[i]));
            pa_thread_free(threads[i]);
        }

        n_run = 0;
        ran_by = NULL;

#ifdef HAVE_PTHREAD
        fail_unless(pthread_barrier_destroy(&barrier) == 0);
#endif
    }
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("Once");
    tc = tcase_create("once");
    tcase_add_test(tc, once_test);
    /* the default timeout is too small,
     * set it to a reasonable large one.
     */
    tcase_set_timeout(tc, 60 * 60);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
