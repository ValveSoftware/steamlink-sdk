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
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <pulse/rtclock.h>
#include <pulsecore/random.h>
#include <pulsecore/macro.h>

#include "runtime-test-util.h"

static inline int32_t pa_mult_s16_volume_32(int16_t v, int32_t cv) {
    /* Multiplying the 32 bit volume factor with the
     * 16 bit sample might result in an 48 bit value. We
     * want to do without 64 bit integers and hence do
     * the multiplication independently for the HI and
     * LO part of the volume. */
    int32_t hi = cv >> 16;
    int32_t lo = cv & 0xFFFF;
    return ((v * lo) >> 16) + (v * hi);
}

static inline int32_t pa_mult_s16_volume_64(int16_t v, int32_t cv) {
    /* Multiply with 64 bit integers on 64 bit platforms */
    return (v * (int64_t) cv) >> 16;
}

#define SAMPLES 1028
#define TIMES 10000
#define TIMES2 100

START_TEST (mult_s16_test) {
    int16_t samples[SAMPLES];
    int32_t volumes[SAMPLES];
    int32_t sum1 = 0, sum2 = 0;
    int i;

    pa_random(samples, sizeof(samples));
    pa_random(volumes, sizeof(volumes));

    for (i = 0; i < SAMPLES; i++) {
        int32_t a = pa_mult_s16_volume_32(samples[i], volumes[i]);
        int32_t b = pa_mult_s16_volume_64(samples[i], volumes[i]);

        if (a != b) {
            pa_log_debug("%d: %d != %d", i, a, b);
            fail();
        }
    }

    PA_RUNTIME_TEST_RUN_START("32 bit mult", TIMES, TIMES2) {
        for (i = 0; i < SAMPLES; i++) {
            sum1 += pa_mult_s16_volume_32(samples[i], volumes[i]);
        }
    } PA_RUNTIME_TEST_RUN_STOP

    PA_RUNTIME_TEST_RUN_START("64 bit mult", TIMES, TIMES2) {
        for (i = 0; i < SAMPLES; i++)
            sum2 += pa_mult_s16_volume_64(samples[i], volumes[i]);
    } PA_RUNTIME_TEST_RUN_STOP

    fail_unless(sum1 == sum2);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

#if __WORDSIZE == 64 || ((ULONG_MAX) > (UINT_MAX))
    pa_log_debug("This seems to be 64-bit code.");
#elif  __WORDSIZE == 32
    pa_log_debug("This seems to be 32-bit code.");
#else
    pa_log_debug("Don't know if this is 32- or 64-bit code.");
#endif

    s = suite_create("Mult-s16");
    tc = tcase_create("mult-s16");
    tcase_add_test(tc, mult_s16_test);
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
