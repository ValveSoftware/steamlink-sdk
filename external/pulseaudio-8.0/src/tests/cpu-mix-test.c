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

#include <pulsecore/cpu.h>
#include <pulsecore/cpu-arm.h>
#include <pulsecore/random.h>
#include <pulsecore/macro.h>
#include <pulsecore/mix.h>

#include "runtime-test-util.h"

#define SAMPLES 1028
#define TIMES 1000
#define TIMES2 100

static void acquire_mix_streams(pa_mix_info streams[], unsigned nstreams) {
    unsigned i;

    for (i = 0; i < nstreams; i++)
        streams[i].ptr = pa_memblock_acquire_chunk(&streams[i].chunk);
}

static void release_mix_streams(pa_mix_info streams[], unsigned nstreams) {
    unsigned i;

    for (i = 0; i < nstreams; i++)
        pa_memblock_release(streams[i].chunk.memblock);
}

static void run_mix_test(
        pa_do_mix_func_t func,
        pa_do_mix_func_t orig_func,
        int align,
        int channels,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, int16_t, in0[SAMPLES * 4]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, in1[SAMPLES * 4]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, out[SAMPLES * 4]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, out_ref[SAMPLES * 4]) = { 0 };
    int16_t *samples0, *samples1;
    int16_t *samples, *samples_ref;
    int nsamples;
    pa_mempool *pool;
    pa_memchunk c0, c1;
    pa_mix_info m[2];
    int i;

    pa_assert(channels == 1 || channels == 2 || channels == 4);

    /* Force sample alignment as requested */
    samples0 = in0 + (8 - align);
    samples1 = in1 + (8 - align);
    samples = out + (8 - align);
    samples_ref = out_ref + (8 - align);
    nsamples = channels * (SAMPLES - (8 - align));

    fail_unless((pool = pa_mempool_new(false, 0)) != NULL, NULL);

    pa_random(samples0, nsamples * sizeof(int16_t));
    c0.memblock = pa_memblock_new_fixed(pool, samples0, nsamples * sizeof(int16_t), false);
    c0.length = pa_memblock_get_length(c0.memblock);
    c0.index = 0;

    pa_random(samples1, nsamples * sizeof(int16_t));
    c1.memblock = pa_memblock_new_fixed(pool, samples1, nsamples * sizeof(int16_t), false);
    c1.length = pa_memblock_get_length(c1.memblock);
    c1.index = 0;

    m[0].chunk = c0;
    m[0].volume.channels = channels;
    for (i = 0; i < channels; i++) {
        m[0].volume.values[i] = PA_VOLUME_NORM;
        m[0].linear[i].i = 0x5555;
    }

    m[1].chunk = c1;
    m[1].volume.channels = channels;
    for (i = 0; i < channels; i++) {
        m[1].volume.values[i] = PA_VOLUME_NORM;
        m[1].linear[i].i = 0x6789;
    }

    if (correct) {
        acquire_mix_streams(m, 2);
        orig_func(m, 2, channels, samples_ref, nsamples * sizeof(int16_t));
        release_mix_streams(m, 2);

        acquire_mix_streams(m, 2);
        func(m, 2, channels, samples, nsamples * sizeof(int16_t));
        release_mix_streams(m, 2);

        for (i = 0; i < nsamples; i++) {
            if (samples[i] != samples_ref[i]) {
                pa_log_debug("Correctness test failed: align=%d, channels=%d", align, channels);
                pa_log_debug("%d: %hd != %04hd (%hd + %hd)\n",
                    i,
                    samples[i], samples_ref[i],
                    samples0[i], samples1[i]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing %d-channel mixing performance with %d sample alignment", channels, align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            acquire_mix_streams(m, 2);
            func(m, 2, channels, samples, nsamples * sizeof(int16_t));
            release_mix_streams(m, 2);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            acquire_mix_streams(m, 2);
            orig_func(m, 2, channels, samples_ref, nsamples * sizeof(int16_t));
            release_mix_streams(m, 2);
        } PA_RUNTIME_TEST_RUN_STOP
    }

    pa_memblock_unref(c0.memblock);
    pa_memblock_unref(c1.memblock);

    pa_mempool_free(pool);
}

START_TEST (mix_special_test) {
    pa_cpu_info cpu_info = { PA_CPU_UNDEFINED, {}, false };
    pa_do_mix_func_t orig_func, special_func;

    cpu_info.force_generic_code = true;
    pa_mix_func_init(&cpu_info);
    orig_func = pa_get_mix_func(PA_SAMPLE_S16NE);

    cpu_info.force_generic_code = false;
    pa_mix_func_init(&cpu_info);
    special_func = pa_get_mix_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking special mix (s16, stereo)");
    run_mix_test(special_func, orig_func, 7, 2, true, true);

    pa_log_debug("Checking special mix (s16, 4-channel)");
    run_mix_test(special_func, orig_func, 7, 4, true, true);

    pa_log_debug("Checking special mix (s16, mono)");
    run_mix_test(special_func, orig_func, 7, 1, true, true);
}
END_TEST

#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
START_TEST (mix_neon_test) {
    pa_do_mix_func_t orig_func, neon_func;
    pa_cpu_arm_flag_t flags = 0;

    pa_cpu_get_arm_flags(&flags);

    if (!(flags & PA_CPU_ARM_NEON)) {
        pa_log_info("NEON not supported. Skipping");
        return;
    }

    orig_func = pa_get_mix_func(PA_SAMPLE_S16NE);
    pa_mix_func_init_neon(flags);
    neon_func = pa_get_mix_func(PA_SAMPLE_S16NE);

    pa_log_debug("Checking NEON mix (s16, stereo)");
    run_mix_test(neon_func, orig_func, 7, 2, true, true);

    pa_log_debug("Checking NEON mix (s16, 4-channel)");
    run_mix_test(neon_func, orig_func, 7, 4, true, true);

    pa_log_debug("Checking NEON mix (s16, mono)");
    run_mix_test(neon_func, orig_func, 7, 1, true, true);
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

    tc = tcase_create("mix");
    tcase_add_test(tc, mix_special_test);
#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
    tcase_add_test(tc, mix_neon_test);
#endif
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
