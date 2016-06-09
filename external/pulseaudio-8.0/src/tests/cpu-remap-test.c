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

#include <pulsecore/cpu-x86.h>
#include <pulsecore/cpu.h>
#include <pulsecore/random.h>
#include <pulsecore/macro.h>
#include <pulsecore/remap.h>

#include "runtime-test-util.h"

#define SAMPLES 1027
#define TIMES 1000
#define TIMES2 100

static void run_remap_test_float(
        pa_remap_t *remap_func,
        pa_remap_t *remap_orig,
        int align,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, float, out_buf_ref[SAMPLES*8]) = { 0.0f, };
    PA_DECLARE_ALIGNED(8, float, out_buf[SAMPLES*8]) = { 0.0f, };
    PA_DECLARE_ALIGNED(8, float, in_buf[SAMPLES*8]);
    float *out, *out_ref;
    float *in;
    unsigned n_ic = remap_func->i_ss.channels;
    unsigned n_oc = remap_func->o_ss.channels;
    unsigned i, nsamples;

    pa_assert(n_ic >= 1 && n_ic <= 8);
    pa_assert(n_oc >= 1 && n_oc <= 8);

    /* Force sample alignment as requested */
    out = out_buf + (8 - align);
    out_ref = out_buf_ref + (8 - align);
    in = in_buf + (8 - align);
    nsamples = SAMPLES - (8 - align);

    for (i = 0; i < nsamples * n_ic; i++)
        in[i] = 2.1f * (rand()/(float) RAND_MAX - 0.5f);

    if (correct) {
        remap_orig->do_remap(remap_orig, out_ref, in, nsamples);
        remap_func->do_remap(remap_func, out, in, nsamples);

        for (i = 0; i < nsamples * n_oc; i++) {
            if (fabsf(out[i] - out_ref[i]) > 0.0001f) {
                pa_log_debug("Correctness test failed: align=%d", align);
                pa_log_debug("%d: %.24f != %.24f\n", i,
                    out[i], out_ref[i]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing remap performance with %d sample alignment", align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            remap_func->do_remap(remap_func, out, in, nsamples);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            remap_orig->do_remap(remap_orig, out_ref, in, nsamples);
        } PA_RUNTIME_TEST_RUN_STOP
    }
}

static void run_remap_test_s16(
        pa_remap_t *remap_func,
        pa_remap_t *remap_orig,
        int align,
        bool correct,
        bool perf) {

    PA_DECLARE_ALIGNED(8, int16_t, out_buf_ref[SAMPLES*8]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, out_buf[SAMPLES*8]) = { 0 };
    PA_DECLARE_ALIGNED(8, int16_t, in_buf[SAMPLES*8]);
    int16_t *out, *out_ref;
    int16_t *in;
    unsigned n_ic = remap_func->i_ss.channels;
    unsigned n_oc = remap_func->o_ss.channels;
    unsigned i, nsamples;

    pa_assert(n_ic >= 1 && n_ic <= 8);
    pa_assert(n_oc >= 1 && n_oc <= 8);

    /* Force sample alignment as requested */
    out = out_buf + (8 - align);
    out_ref = out_buf_ref + (8 - align);
    in = in_buf + (8 - align);
    nsamples = SAMPLES - (8 - align);

    pa_random(in, nsamples * n_ic * sizeof(int16_t));

    if (correct) {
        remap_orig->do_remap(remap_orig, out_ref, in, nsamples);
        remap_func->do_remap(remap_func, out, in, nsamples);

        for (i = 0; i < nsamples * n_oc; i++) {
            if (abs(out[i] - out_ref[i]) > 3) {
                pa_log_debug("Correctness test failed: align=%d", align);
                pa_log_debug("%d: %d != %d\n", i, out[i], out_ref[i]);
                fail();
            }
        }
    }

    if (perf) {
        pa_log_debug("Testing remap performance with %d sample alignment", align);

        PA_RUNTIME_TEST_RUN_START("func", TIMES, TIMES2) {
            remap_func->do_remap(remap_func, out, in, nsamples);
        } PA_RUNTIME_TEST_RUN_STOP

        PA_RUNTIME_TEST_RUN_START("orig", TIMES, TIMES2) {
            remap_orig->do_remap(remap_orig, out_ref, in, nsamples);
        } PA_RUNTIME_TEST_RUN_STOP
    }
}

static void setup_remap_channels(
    pa_remap_t *m,
    pa_sample_format_t f,
    unsigned in_channels,
    unsigned out_channels,
    bool rearrange) {

    unsigned i, o;

    m->format = f;
    m->i_ss.channels = in_channels;
    m->o_ss.channels = out_channels;

    if (rearrange) {
        for (o = 0; o < out_channels; o++) {
            for (i = 0; i < in_channels; i++) {
                m->map_table_f[o][i] = (o == i) ? 1.0f : 0.0f;
                m->map_table_i[o][i] = (o == i) ? 0x10000 : 0;
            }
        }
    } else {
        for (o = 0; o < out_channels; o++) {
            for (i = 0; i < in_channels; i++) {
                m->map_table_f[o][i] = 1.0f / in_channels;
                m->map_table_i[o][i] = 0x10000 / in_channels;
            }
        }
    }
}

static void remap_test_channels(
    pa_remap_t *remap_func, pa_remap_t *remap_orig) {

    if (!remap_orig->do_remap) {
        pa_log_warn("No reference remapping function, abort test");
        return;
    }

    if (!remap_func->do_remap || remap_func->do_remap == remap_orig->do_remap) {
        pa_log_warn("No remapping function, abort test");
        return;
    }

    pa_assert(remap_func->format == remap_orig->format);

    switch (remap_func->format) {
    case PA_SAMPLE_FLOAT32NE:
        run_remap_test_float(remap_func, remap_orig, 0, true, false);
        run_remap_test_float(remap_func, remap_orig, 1, true, false);
        run_remap_test_float(remap_func, remap_orig, 2, true, false);
        run_remap_test_float(remap_func, remap_orig, 3, true, true);
        break;
    case PA_SAMPLE_S16NE:
        run_remap_test_s16(remap_func, remap_orig, 0, true, false);
        run_remap_test_s16(remap_func, remap_orig, 1, true, false);
        run_remap_test_s16(remap_func, remap_orig, 2, true, false);
        run_remap_test_s16(remap_func, remap_orig, 3, true, true);
        break;
    default:
        pa_assert_not_reached();
    }
}

static void remap_init_test_channels(
        pa_init_remap_func_t init_func,
        pa_init_remap_func_t orig_init_func,
        pa_sample_format_t f,
        unsigned in_channels,
        unsigned out_channels,
        bool rearrange) {

    pa_remap_t remap_orig, remap_func;

    setup_remap_channels(&remap_orig, f, in_channels, out_channels, rearrange);
    orig_init_func(&remap_orig);

    setup_remap_channels(&remap_func, f, in_channels, out_channels, rearrange);
    init_func(&remap_func);

    remap_test_channels(&remap_func, &remap_orig);
}

static void remap_init2_test_channels(
        pa_sample_format_t f,
        unsigned in_channels,
        unsigned out_channels,
        bool rearrange) {

    pa_cpu_info cpu_info = { PA_CPU_UNDEFINED, {}, false };
    pa_remap_t remap_orig, remap_func;

    cpu_info.force_generic_code = true;
    pa_remap_func_init(&cpu_info);
    setup_remap_channels(&remap_orig, f, in_channels, out_channels, rearrange);
    pa_init_remap_func(&remap_orig);

    cpu_info.force_generic_code = false;
    pa_remap_func_init(&cpu_info);
    setup_remap_channels(&remap_func, f, in_channels, out_channels, rearrange);
    pa_init_remap_func(&remap_func);

    remap_test_channels(&remap_func, &remap_orig);
}

START_TEST (remap_special_test) {
    pa_log_debug("Checking special remap (float, mono->stereo)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 1, 2, false);
    pa_log_debug("Checking special remap (float, mono->4-channel)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 1, 4, false);

    pa_log_debug("Checking special remap (s16, mono->stereo)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 1, 2, false);
    pa_log_debug("Checking special remap (s16, mono->4-channel)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 1, 4, false);

    pa_log_debug("Checking special remap (float, stereo->mono)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 2, 1, false);
    pa_log_debug("Checking special remap (float, 4-channel->mono)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 4, 1, false);

    pa_log_debug("Checking special remap (s16, stereo->mono)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 2, 1, false);
    pa_log_debug("Checking special remap (s16, 4-channel->mono)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 4, 1, false);
}
END_TEST

START_TEST (rearrange_special_test) {
    pa_log_debug("Checking special remap (s16, stereo rearrange)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 2, 2, true);
    pa_log_debug("Checking special remap (float, stereo rearrange)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 2, 2, true);

    pa_log_debug("Checking special remap (s16, 4-channel rearrange)");
    remap_init2_test_channels(PA_SAMPLE_S16NE, 4, 4, true);
    pa_log_debug("Checking special remap (float, 4-channel rearrange)");
    remap_init2_test_channels(PA_SAMPLE_FLOAT32NE, 4, 4, true);
}
END_TEST

#if defined (__i386__) || defined (__amd64__)
START_TEST (remap_mmx_test) {
    pa_cpu_x86_flag_t flags = 0;
    pa_init_remap_func_t init_func, orig_init_func;

    pa_cpu_get_x86_flags(&flags);
    if (!(flags & PA_CPU_X86_MMX)) {
        pa_log_info("MMX not supported. Skipping");
        return;
    }

    pa_log_debug("Checking MMX remap (float, mono->stereo)");
    orig_init_func = pa_get_init_remap_func();
    pa_remap_func_init_mmx(flags);
    init_func = pa_get_init_remap_func();
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 1, 2, false);

    pa_log_debug("Checking MMX remap (s16, mono->stereo)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 1, 2, false);
}
END_TEST

START_TEST (remap_sse2_test) {
    pa_cpu_x86_flag_t flags = 0;
    pa_init_remap_func_t init_func, orig_init_func;

    pa_cpu_get_x86_flags(&flags);
    if (!(flags & PA_CPU_X86_SSE2)) {
        pa_log_info("SSE2 not supported. Skipping");
        return;
    }

    pa_log_debug("Checking SSE2 remap (float, mono->stereo)");
    orig_init_func = pa_get_init_remap_func();
    pa_remap_func_init_sse(flags);
    init_func = pa_get_init_remap_func();
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 1, 2, false);

    pa_log_debug("Checking SSE2 remap (s16, mono->stereo)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 1, 2, false);
}
END_TEST
#endif /* defined (__i386__) || defined (__amd64__) */

#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
START_TEST (remap_neon_test) {
    pa_cpu_arm_flag_t flags = 0;
    pa_init_remap_func_t init_func, orig_init_func;

    pa_cpu_get_arm_flags(&flags);
    if (!(flags & PA_CPU_ARM_NEON)) {
        pa_log_info("NEON not supported. Skipping");
        return;
    }

    orig_init_func = pa_get_init_remap_func();
    pa_remap_func_init_neon(flags);
    init_func = pa_get_init_remap_func();

    pa_log_debug("Checking NEON remap (float, mono->stereo)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 1, 2, false);
    pa_log_debug("Checking NEON remap (float, mono->4-channel)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 1, 4, false);

    pa_log_debug("Checking NEON remap (s16, mono->stereo)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 1, 2, false);
    pa_log_debug("Checking NEON remap (s16, mono->4-channel)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 1, 4, false);

    pa_log_debug("Checking NEON remap (float, stereo->mono)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 2, 1, false);
    pa_log_debug("Checking NEON remap (float, 4-channel->mono)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 4, 1, false);

    pa_log_debug("Checking NEON remap (s16, stereo->mono)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 2, 1, false);
    pa_log_debug("Checking NEON remap (s16, 4-channel->mono)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 4, 1, false);

    pa_log_debug("Checking NEON remap (float, 4-channel->4-channel)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 4, 4, false);
    pa_log_debug("Checking NEON remap (s16, 4-channel->4-channel)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 4, 4, false);
}
END_TEST

START_TEST (rearrange_neon_test) {
    pa_cpu_arm_flag_t flags = 0;
    pa_init_remap_func_t init_func, orig_init_func;

    pa_cpu_get_arm_flags(&flags);
    if (!(flags & PA_CPU_ARM_NEON)) {
        pa_log_info("NEON not supported. Skipping");
        return;
    }

    orig_init_func = pa_get_init_remap_func();
    pa_remap_func_init_neon(flags);
    init_func = pa_get_init_remap_func();

    pa_log_debug("Checking NEON remap (float, stereo rearrange)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 2, 2, true);
    pa_log_debug("Checking NEON remap (s16, stereo rearrange)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 2, 2, true);

    pa_log_debug("Checking NEON remap (float, 4-channel rearrange)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_FLOAT32NE, 4, 4, true);
    pa_log_debug("Checking NEON remap (s16, 4-channel rearrange)");
    remap_init_test_channels(init_func, orig_init_func, PA_SAMPLE_S16NE, 4, 4, true);
}
END_TEST
#endif

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("CPU");

    tc = tcase_create("remap");
    tcase_add_test(tc, remap_special_test);
#if defined (__i386__) || defined (__amd64__)
    tcase_add_test(tc, remap_mmx_test);
    tcase_add_test(tc, remap_sse2_test);
#endif
#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
    tcase_add_test(tc, remap_neon_test);
#endif
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    tc = tcase_create("rearrange");
    tcase_add_test(tc, rearrange_special_test);
#if defined (__arm__) && defined (__linux__) && defined (HAVE_NEON)
    tcase_add_test(tc, rearrange_neon_test);
#endif
    tcase_set_timeout(tc, 120);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
