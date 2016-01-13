/***
  This file is part of PulseAudio.

  Copyright 2013 Collabora Ltd.
  Author: Arun Raghavan <arun.raghavan@collabora.co.uk>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <check.h>

#include "lo-test-util.h"

#define SAMPLE_HZ 44100
#define CHANNELS 2
#define N_OUT (SAMPLE_HZ * 1)

static float out[N_OUT][CHANNELS];

pa_lo_test_context test_ctx;
static const char *context_name = NULL;

static struct timeval tv_out, tv_in;

static void nop_free_cb(void *p) {
}

static void write_cb(pa_stream *s, size_t nbytes, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;
    static int ppos = 0;
    int r, nsamp;

    /* Get the real requested bytes since the last write might have been
     * incomplete if it caused a wrap around */
    nbytes = pa_stream_writable_size(s);
    nsamp = nbytes / ctx->fs;

    if (ppos + nsamp > N_OUT) {
        /* Wrap-around, write to end and exit. Next iteration will fill up the
         * rest */
        nbytes = (N_OUT - ppos) * ctx->fs;
    }

    if (ppos == 0)
        pa_gettimeofday(&tv_out);

    r = pa_stream_write(s, &out[ppos][0], nbytes, nop_free_cb, 0, PA_SEEK_RELATIVE);
    fail_unless(r == 0);

    ppos = (ppos + nbytes / ctx->fs) % N_OUT;
}

#define WINDOW (2 * CHANNELS)

static void read_cb(pa_stream *s, size_t nbytes, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;
    static float last = 0.0f;
    const float *in;
    float cur;
    int r;
    unsigned int i = 0;
    size_t l;

    r = pa_stream_peek(s, (const void **)&in, &l);
    fail_unless(r == 0);

    if (l == 0)
        return;

#if 0
    {
        static int fd = -1;

        if (fd == -1) {
            fd = open("loopback.raw", O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
            fail_if(fd < 0);
        }

        r = write(fd, in, l);
    }
#endif

    do {
#if 0
        {
            int j;
            fprintf(stderr, "%g (", pa_rms(in, WINDOW));
            for (j = 0; j < WINDOW; j++)
                fprintf(stderr, "%g ", in[j]);
            fprintf(stderr, ")\n");
        }
#endif
        if (i + (ctx->ss * WINDOW) < l)
            cur = pa_rms(in, WINDOW);
        else
            cur = pa_rms(in, (l - i) / ctx->ss);

        /* We leave the definition of 0 generous since the window might
         * straddle the 0->1 transition, raising the average power. We keep the
         * definition of 1 tight in this case and detect the transition in the
         * next round. */
        if (cur - last > 0.4f) {
            pa_gettimeofday(&tv_in);
            fprintf(stderr, "Latency %llu\n", (unsigned long long) pa_timeval_diff(&tv_in, &tv_out));
        }

        last = cur;
        in += WINDOW;
        i += ctx->ss * WINDOW;
    } while (i + (ctx->ss * WINDOW) <= l);

    pa_stream_drop(s);
}

START_TEST (loopback_test) {
    int i, pulse_hz = SAMPLE_HZ / 1000;

    test_ctx.context_name = context_name;

    test_ctx.sample_spec.format = PA_SAMPLE_FLOAT32,
    test_ctx.sample_spec.rate = SAMPLE_HZ,
    test_ctx.sample_spec.channels = CHANNELS,

    test_ctx.play_latency = 25;
    test_ctx.rec_latency = 5;

    test_ctx.read_cb = read_cb;
    test_ctx.write_cb = write_cb;

    /* Generate a square pulse */
    for (i = 0; i < N_OUT; i++)
        if (i < pulse_hz)
            out[i][0] = out[i][1] = 1.0f;
        else
            out[i][0] = out[i][1] = 0.0f;

    fail_unless(pa_lo_test_init(&test_ctx) == 0);
    fail_unless(pa_lo_test_run(&test_ctx) == 0);
    pa_lo_test_deinit(&test_ctx);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    context_name = argv[0];

    s = suite_create("Loopback latency");
    tc = tcase_create("loopback latency");
    tcase_add_test(tc, loopback_test);
    tcase_set_timeout(tc, 5 * 60);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
