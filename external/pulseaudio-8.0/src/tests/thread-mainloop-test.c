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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <check.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/thread-mainloop.h>

#include <pulsecore/macro.h>
#include <pulsecore/core-rtclock.h>

static void tcb(pa_mainloop_api *a, pa_time_event *e, const struct timeval *tv, void *userdata) {
    pa_assert_se(pa_threaded_mainloop_in_thread(userdata));
    fprintf(stderr, "TIME EVENT START\n");
    pa_threaded_mainloop_signal(userdata, 1);
    fprintf(stderr, "TIME EVENT END\n");
}

START_TEST (thread_mainloop_test) {
    pa_mainloop_api *a;
    pa_threaded_mainloop *m;
    struct timeval tv;

    m = pa_threaded_mainloop_new();
    fail_unless(m != NULL);
    a = pa_threaded_mainloop_get_api(m);
    fail_unless(m != NULL);

    fail_unless(pa_threaded_mainloop_start(m) >= 0);

    pa_threaded_mainloop_lock(m);

    fail_unless(!pa_threaded_mainloop_in_thread(m));

    a->time_new(a, pa_timeval_rtstore(&tv, pa_rtclock_now() + 5 * PA_USEC_PER_SEC, true), tcb, m);

    fprintf(stderr, "waiting 5s (signal)\n");
    pa_threaded_mainloop_wait(m);
    fprintf(stderr, "wait completed\n");
    pa_threaded_mainloop_accept(m);
    fprintf(stderr, "signal accepted\n");

    pa_threaded_mainloop_unlock(m);

    fprintf(stderr, "waiting 5s (sleep)\n");
    pa_msleep(5000);

    pa_threaded_mainloop_stop(m);

    pa_threaded_mainloop_free(m);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Thread MainLoop");
    tc = tcase_create("threadmainloop");
    tcase_add_test(tc, thread_mainloop_test);
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
