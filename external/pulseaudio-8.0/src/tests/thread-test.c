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

#include <check.h>

#include <pulse/xmalloc.h>
#include <pulsecore/thread.h>
#include <pulsecore/macro.h>
#include <pulsecore/mutex.h>
#include <pulsecore/once.h>
#include <pulsecore/log.h>
#include <pulsecore/core-util.h>

static pa_mutex *mutex = NULL;
static pa_cond *cond1 = NULL, *cond2 = NULL;
static pa_tls *tls = NULL;

static int magic_number = 0;

#define THREADS_MAX 20

static void once_func(void) {
    pa_log("once!");
}

static pa_once once = PA_ONCE_INIT;

static void thread_func(void *data) {
    pa_tls_set(tls, data);

    pa_log_info("thread_func() for %s starting...", (char*) pa_tls_get(tls));

    pa_mutex_lock(mutex);

    for (;;) {
        int k, n;

        pa_log_info("%s waiting ...", (char*) pa_tls_get(tls));

        for (;;) {

            if (magic_number < 0)
                goto quit;

            if (magic_number != 0)
                break;

            pa_cond_wait(cond1, mutex);
        }

        k = magic_number;
        magic_number = 0;

        pa_mutex_unlock(mutex);

        pa_run_once(&once, once_func);

        pa_cond_signal(cond2, 0);

        pa_log_info("%s got number %i", (char*) pa_tls_get(tls), k);

        /* Spin! */
        for (n = 0; n < k; n++)
            pa_thread_yield();

        pa_mutex_lock(mutex);
    }

quit:

    pa_mutex_unlock(mutex);

    pa_log_info("thread_func() for %s done...", (char*) pa_tls_get(tls));
}

START_TEST (thread_test) {
    int i, k;
    pa_thread* t[THREADS_MAX];

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    mutex = pa_mutex_new(false, false);
    cond1 = pa_cond_new();
    cond2 = pa_cond_new();
    tls = pa_tls_new(pa_xfree);

    for (i = 0; i < THREADS_MAX; i++) {
        t[i] = pa_thread_new("test", thread_func, pa_sprintf_malloc("Thread #%i", i+1));
        fail_unless(t[i] != 0);
    }

    pa_mutex_lock(mutex);

    pa_log("loop-init");

    for (k = 0; k < 100; k++) {
        pa_assert(magic_number == 0);

        magic_number = (int) rand() % 0x10000;

        pa_log_info("iteration %i (%i)", k, magic_number);

        pa_cond_signal(cond1, 0);

        pa_cond_wait(cond2, mutex);
    }

    pa_log("loop-exit");

    magic_number = -1;
    pa_cond_signal(cond1, 1);

    pa_mutex_unlock(mutex);

    for (i = 0; i < THREADS_MAX; i++)
        pa_thread_free(t[i]);

    pa_mutex_free(mutex);
    pa_cond_free(cond1);
    pa_cond_free(cond2);
    pa_tls_free(tls);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Thread");
    tc = tcase_create("thread");
    tcase_add_test(tc, thread_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
