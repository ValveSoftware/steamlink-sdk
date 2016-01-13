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

#include <pulsecore/cpu-arm.h>
#include <pulsecore/cpu-x86.h>
#include <pulsecore/random.h>
#include <pulsecore/macro.h>
#include <pulsecore/sconv.h>

#include "runtime-test-util.h"

#define SAMPLES 1028
#define TIMES 1000
#define TIMES2 100

static void run_conv_test_float_to_s16(
        pa_convert_func_t func,
        pa_convert_func_t orig_func,
        int align,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, int16_t, s[SAMPLES]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, s_ref[SAMPLES]) = { 0 };
    PA_DECLARE_ALIGNED(8, float, f[SAMPLES]);
    int16_t *samples, *samples_ref;
    float *floats;
    int i, nsamples;

    /* Force sample alignment as requested */
    samples = s + (8 - align);
    samples_ref = s_ref + (8 - align);
    floats = f + (8 - align);
    nsamples = SAMPLES - (8 - align);

    for (i = 0; i < nsamples; i++) {
        floats[i] = 2.1f * (rand()/(float) RAND_MAX - 0.5f);
    }

    if (correct) {
        orig_func(nsamples, floats, samples_ref);
        func(nsamples, floats, samples);

        for (i = 0; i < nsamples; i++) {
            if (abs(samples[i] - samples_ref[i]) > 1) {
                pa_log_debug("Correctness test failed: align=%d", align);
                pa_log_debug("%d: %04hx != %04hx (%.24f)\n", i, samples[i], samples_ref[i], floats[i]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing sconv performance with %d sample alignment", align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            func(nsamples, floats, samples);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            orig_func(nsamples, floats, samples_ref);
        } PA_RUNTIME_TEST_RUN_STOP
    }
}

/* This test is currently only run under NEON */
#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
static void run_conv_test_s16_to_float(
        pa_convert_func_t func,
        pa_convert_func_t orig_func,
        int align,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, float, f[SAMPLES]) = { 0.0f };
    PA_DECLARE_ALIGNED(8, float, f_ref[SAMPLES]) = { 0.0f };
    PA_DECLARE_ALIGNED(8, int16_t, s[SAMPLES]);
    float *floats, *floats_ref;
    int16_t *samples;
    int i, nsamples;

    /* Force sample alignment as requested */
    floats = f + (8 - align);
    floats_ref = f_ref + (8 - align);
    samples = s + (8 - align);
    nsamples = SAMPLES - (8 - align);

    pa_random(samples, nsamples * sizeof(int16_t));

    if (correct) {
        orig_func(nsamples, samples, floats_ref);
        func(nsamples, samples, floats);

        for (i = 0; i < nsamples; i++) {
            if (fabsf(floats[i] - floats_ref[i]) > 0.0001f) {
                pa_log_debug("Correctness test failed: align=%d", align);
                pa_log_debug("%d: %.24f != %.24f (%d)\n", i, floats[i], floats_ref[i], samples[i]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing sconv performance with %d sample alignment", align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            func(nsamples, samples, floats);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            orig_func(nsamples, samples, floats_ref);
        } PA_RUNTIME_TEST_RUN_STOP
    }
}
#endif /* defined (__arm__) && defined (__linux__) && defined (HAVE_NEON) */

#if defined (__i386__) || defined (__amd64__)
START_TEST (sconv_sse2_test) {
    pa_cpu_x86_flag_t flags = 0;
    pa_convert_func_t orig_func, sse2_func;

    pa_cpu_get_x86_flags(&flags);

    if (!(flags & PA_CPU_X86_SSE2)) {
        pa_log_info("SSE2 not supported. Skipping");
        return;
    }

    orig_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);
    pa_convert_func_init_sse(PA_CPU_X86_SSE2);
    sse2_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);

    pa_log_debug("Checking SSE2 sconv (float -> s16)");
    run_conv_test_float_to_s16(sse2_func, orig_func, 0, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 1, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 2, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 3, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 4, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 5, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 6, true, false);
    run_conv_test_float_to_s16(sse2_func, orig_func, 7, true, true);
}
END_TEST

START_TEST (sconv_sse_test) {
    pa_cpu_x86_flag_t flags = 0;
    pa_convert_func_t orig_func, sse_func;

    pa_cpu_get_x86_flags(&flags);

    if (!(flags & PA_CPU_X86_SSE)) {
        pa_log_info("SSE not supported. Skipping");
        return;
    }

    orig_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);
    pa_convert_func_init_sse(PA_CPU_X86_SSE);
    sse_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);

    pa_log_debug("Checking SSE sconv (float -> s16)");
    run_conv_test_float_to_s16(sse_func, orig_func, 0, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 1, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 2, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 3, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 4, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 5, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 6, true, false);
    run_conv_test_float_to_s16(sse_func, orig_func, 7, true, true);
}
END_TEST
#endif /* defined (__i386__) || defined (__amd64__) */

#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
START_TEST (sconv_neon_test) {
    pa_cpu_arm_flag_t flags = 0;
    pa_convert_func_t orig_from_func, neon_from_func;
    pa_convert_func_t orig_to_func, neon_to_func;

    pa_cpu_get_arm_flags(&flags);

    if (!(flags & PA_CPU_ARM_NEON)) {
        pa_log_info("NEON not supported. Skipping");
        return;
    }

    orig_from_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);
    orig_to_func = pa_get_convert_to_float32ne_function(PA_SAMPLE_S16LE);
    pa_convert_func_init_neon(flags);
    neon_from_func = pa_get_convert_from_float32ne_function(PA_SAMPLE_S16LE);
    neon_to_func = pa_get_convert_to_float32ne_function(PA_SAMPLE_S16LE);

    pa_log_debug("Checking NEON sconv (float -> s16)");
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 0, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 1, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 2, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 3, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 4, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 5, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 6, true, false);
    run_conv_test_float_to_s16(neon_from_func, orig_from_func, 7, true, true);

    pa_log_debug("Checking NEON sconv (s16 -> float)");
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 0, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 1, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 2, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 3, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 4, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 5, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 6, true, false);
    run_conv_test_s16_to_float(neon_to_func, orig_to_func, 7, true, true);
}
END_TEST
#endif /* defined (__arm__) && defined (__linux__) && defined (HAVE_NEON) */

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("CPU");

    tc = tcase_create("sconv");
#if defined (__i386__) || defined (__amd64__)
    tcase_add_test(tc, sconv_sse2_test);
    tcase_add_test(tc, sconv_sse_test);
#endif
#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
    tcase_add_test(tc, sconv_neon_test);
#endif
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
