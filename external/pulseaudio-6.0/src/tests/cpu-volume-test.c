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
#include <pulsecore/cpu-orc.h>
#include <pulsecore/random.h>
#include <pulsecore/macro.h>
#include <pulsecore/sample-util.h>

#include "runtime-test-util.h"

/* Common defines for svolume tests */
#define SAMPLES 1028
#define TIMES 1000
#define TIMES2 100
#define PADDING 16

static void run_volume_test(
        pa_do_volume_func_t func,
        pa_do_volume_func_t orig_func,
        int align,
        int channels,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, int16_t, s[SAMPLES]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, s_ref[SAMPLES]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, s_orig[SAMPLES]) = { 0 };
    int32_t volumes[channels + PADDING];
    int16_t *samples, *samples_ref, *samples_orig;
    int i, padding, nsamples, size;

    /* Force sample alignment as requested */
    samples = s + (8 - align);
    samples_ref = s_ref + (8 - align);
    samples_orig = s_orig + (8 - align);
    nsamples = SAMPLES - (8 - align);
    if (nsamples % channels)
        nsamples -= nsamples % channels;
    size = nsamples * sizeof(int16_t);

    pa_random(samples, size);
    memcpy(samples_ref, samples, size);
    memcpy(samples_orig, samples, size);

    for (i = 0; i < channels; i++)
        volumes[i] = PA_CLAMP_VOLUME((pa_volume_t)(rand() >> 15));
    for (padding = 0; padding < PADDING; padding++, i++)
        volumes[i] = volumes[padding];

    if (correct) {
        orig_func(samples_ref, volumes, channels, size);
        func(samples, volumes, channels, size);

        for (i = 0; i < nsamples; i++) {
            if (samples[i] != samples_ref[i]) {
                pa_log_debug("Correctness test failed: align=%d, channels=%d", align, channels);
                pa_log_debug("%d: %04hx != %04hx (%04hx * %08x)\n", i, samples[i], samples_ref[i],
                        samples_orig[i], volumes[i % channels]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing svolume %dch performance with %d sample alignment", channels, align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            memcpy(samples, samples_orig, size);
            func(samples, volumes, channels, size);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            memcpy(samples_ref, samples_orig, size);
            orig_func(samples_ref, volumes, channels, size);
        } PA_RUNTIME_TEST_RUN_STOP

        fail_unless(memcmp(samples_ref, samples, size) == 0);
    }
}

#if defined (__i386__) || defined (__amd64__)
START_TEST (svolume_mmx_test) {
    pa_do_volume_func_t orig_func, mmx_func;
    pa_cpu_x86_flag_t flags = 0;
    int i, j;

    pa_cpu_get_x86_flags(&flags);

    if (!((flags & PA_CPU_X86_MMX) && (flags & PA_CPU_X86_CMOV))) {
        pa_log_info("MMX/CMOV not supported. Skipping");
        return;
    }

    orig_func = pa_get_volume_func(PA_SAMPLE_S16NE);
    pa_volume_func_init_mmx(flags);
    mmx_func = pa_get_volume_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking MMX svolume");
    for (i = 1; i <= 3; i++) {
        for (j = 0; j < 7; j++)
            run_volume_test(mmx_func, orig_func, j, i, true, false);
    }
    run_volume_test(mmx_func, orig_func, 7, 1, true, true);
    run_volume_test(mmx_func, orig_func, 7, 2, true, true);
    run_volume_test(mmx_func, orig_func, 7, 3, true, true);
}
END_TEST

START_TEST (svolume_sse_test) {
    pa_do_volume_func_t orig_func, sse_func;
    pa_cpu_x86_flag_t flags = 0;
    int i, j;

    pa_cpu_get_x86_flags(&flags);

    if (!(flags & PA_CPU_X86_SSE2)) {
        pa_log_info("SSE2 not supported. Skipping");
        return;
    }

    orig_func = pa_get_volume_func(PA_SAMPLE_S16NE);
    pa_volume_func_init_sse(flags);
    sse_func = pa_get_volume_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking SSE2 svolume");
    for (i = 1; i <= 3; i++) {
        for (j = 0; j < 7; j++)
            run_volume_test(sse_func, orig_func, j, i, true, false);
    }
    run_volume_test(sse_func, orig_func, 7, 1, true, true);
    run_volume_test(sse_func, orig_func, 7, 2, true, true);
    run_volume_test(sse_func, orig_func, 7, 3, true, true);
}
END_TEST
#endif /* defined (__i386__) || defined (__amd64__) */

#if defined (__arm__) && defined (__linux__)
START_TEST (svolume_arm_test) {
    pa_do_volume_func_t orig_func, arm_func;
    pa_cpu_arm_flag_t flags = 0;
    int i, j;

    pa_cpu_get_arm_flags(&flags);

    if (!(flags & PA_CPU_ARM_V6)) {
        pa_log_info("ARMv6 instructions not supported. Skipping");
        return;
    }

    orig_func = pa_get_volume_func(PA_SAMPLE_S16NE);
    pa_volume_func_init_arm(flags);
    arm_func = pa_get_volume_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking ARM svolume");
    for (i = 1; i <= 3; i++) {
        for (j = 0; j < 7; j++)
            run_volume_test(arm_func, orig_func, j, i, true, false);
    }
    run_volume_test(arm_func, orig_func, 7, 1, true, true);
    run_volume_test(arm_func, orig_func, 7, 2, true, true);
    run_volume_test(arm_func, orig_func, 7, 3, true, true);
}
END_TEST
#endif /* defined (__arm__) && defined (__linux__) */

START_TEST (svolume_orc_test) {
    pa_do_volume_func_t orig_func, orc_func;
    pa_cpu_info cpu_info;
    int i, j;

#if defined (__i386__) || defined (__amd64__)
    pa_zero(cpu_info);
    cpu_info.cpu_type = PA_CPU_X86;
    pa_cpu_get_x86_flags(&cpu_info.flags.x86);
#endif

    orig_func = pa_get_volume_func(PA_SAMPLE_S16NE);

    if (!pa_cpu_init_orc(cpu_info)) {
        pa_log_info("Orc not supported. Skipping");
        return;
    }

    orc_func = pa_get_volume_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking Orc svolume");
    for (i = 1; i <= 2; i++) {
        for (j = 0; j < 7; j++)
            run_volume_test(orc_func, orig_func, j, i, true, false);
    }
    run_volume_test(orc_func, orig_func, 7, 1, true, true);
    run_volume_test(orc_func, orig_func, 7, 2, true, true);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("CPU");

    tc = tcase_create("svolume");
#if defined (__i386__) || defined (__amd64__)
    tcase_add_test(tc, svolume_mmx_test);
    tcase_add_test(tc, svolume_sse_test);
#endif
#if defined (__arm__) && defined (__linux__)
    tcase_add_test(tc, svolume_arm_test);
#endif
    tcase_add_test(tc, svolume_orc_test);
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
