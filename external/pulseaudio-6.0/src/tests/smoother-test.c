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

#include <stdio.h>
#include <stdlib.h>

#include <check.h>

#include <pulse/timeval.h>

#include <pulsecore/log.h>
#include <pulsecore/time-smoother.h>

START_TEST (smoother_test) {
    pa_usec_t x;
    unsigned u = 0;
    pa_smoother *s;
    int m;

/*     unsigned msec[] = { */
/*         200, 200, */
/*         300, 320, */
/*         400, 400, */
/*         500, 480, */
/*         0, 0 */
/*     }; */

    int msec[200];

    srand(0);

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    for (m = 0, u = 0; u < PA_ELEMENTSOF(msec); u+= 2) {

        msec[u] = m+1 + (rand() % 100) - 50;
        msec[u+1] = m + (rand() % 2000) - 1000   + 5000;

        m += rand() % 100;

        if (msec[u] < 0)
            msec[u] = 0;

        if (msec[u+1] < 0)
            msec[u+1] = 0;
    }

    s = pa_smoother_new(700*PA_USEC_PER_MSEC, 2000*PA_USEC_PER_MSEC, false, true, 6, 0, true);

    for (x = 0, u = 0; x < PA_USEC_PER_SEC * 10; x += PA_USEC_PER_MSEC) {

        while (u < PA_ELEMENTSOF(msec) && (pa_usec_t) msec[u]*PA_USEC_PER_MSEC < x) {
            pa_smoother_put(s, (pa_usec_t) msec[u] * PA_USEC_PER_MSEC, (pa_usec_t) msec[u+1] * PA_USEC_PER_MSEC);
            pa_log_debug("%i\t\t%i", msec[u],  msec[u+1]);
            u += 2;

            if (u < PA_ELEMENTSOF(msec))
                pa_smoother_resume(s, (pa_usec_t) msec[u] * PA_USEC_PER_MSEC, true);
        }

        pa_log_debug("%llu\t%llu", (unsigned long long) (x/PA_USEC_PER_MSEC), (unsigned long long) (pa_smoother_get(s, x)/PA_USEC_PER_MSEC));
    }

    pa_smoother_free(s);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Smoother");
    tc = tcase_create("smoother");
    tcase_add_test(tc, smoother_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
