/***
  This file is part of PulseAudio.

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

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <check.h>

#include <pulse/mainloop.h>

#ifdef TEST2
#include <pulse/mainloop-signal.h>
#endif

#include <daemon/cpulimit.h>

/* A simple example for testing the cpulimit subsystem */

static time_t start;

#ifdef TEST2

static void func(pa_mainloop_api *m, pa_signal_event *e, int sig, void *userdata) {
    time_t now;
    time(&now);

    if ((now - start) >= 30) {
        m->quit(m, 1);
        fprintf(stderr, "Test failed\n");
        fail();
    } else
        raise(SIGUSR1);
}

#endif

START_TEST (cpulimit_test) {
    pa_mainloop *m;

    m = pa_mainloop_new();
    fail_unless(m != NULL);

    pa_cpu_limit_init(pa_mainloop_get_api(m));

    time(&start);

#ifdef TEST2
    pa_signal_init(pa_mainloop_get_api(m));
    pa_signal_new(SIGUSR1, func, NULL);
    raise(SIGUSR1);
    pa_mainloop_run(m, NULL);
    pa_signal_done();
#else
    for (;;) {
        time_t now;
        time(&now);

        if ((now - start) >= 30) {
            fprintf(stderr, "Test failed\n");
            fail();
            break;
        }
    }
#endif

    pa_cpu_limit_done();

    pa_mainloop_free(m);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("CPU Limit");
    tc = tcase_create("cpulimit");
    tcase_add_test(tc, cpulimit_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
