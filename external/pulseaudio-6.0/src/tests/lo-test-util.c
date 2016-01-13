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

#include <math.h>

#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>

#include "lo-test-util.h"

/* Keep the frequency high so RMS over ranges of a few ms remains relatively
 * high as well */
#define TONE_HZ 4410

static void nop_free_cb(void *p) {
}

static void underflow_cb(struct pa_stream *s, void *userdata) {
    pa_log_warn("Underflow\n");
}

static void overflow_cb(struct pa_stream *s, void *userdata) {
    pa_log_warn("Overlow\n");
}

/*
 * We run a simple volume calibration so that we know we can detect the signal
 * being played back. We start with the playback stream at 100% volume, and
 * capture at 0.
 *
 * First, we then play a sine wave and increase the capture volume till the
 * signal is clearly received.
 *
 * Next, we play back silence and make sure that the level is low enough to
 * distinguish from when playback is happening.
 *
 * Finally, we hand off to the real read/write callbacks to run the actual
 * test.
 */

enum {
    CALIBRATION_ONE,
    CALIBRATION_ZERO,
    CALIBRATION_DONE,
};

static int cal_state = CALIBRATION_ONE;

static void calibrate_write_cb(pa_stream *s, size_t nbytes, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;
    int i, nsamp = nbytes / ctx->fs;
    float tmp[nsamp][2];
    static int count = 0;

    /* Write out a sine tone */
    for (i = 0; i < nsamp; i++)
        tmp[i][0] = tmp[i][1] = cal_state == CALIBRATION_ONE ? sinf(count++ * TONE_HZ * 2 * M_PI / ctx->sample_spec.rate) : 0.0f;

    pa_assert_se(pa_stream_write(s, &tmp, nbytes, nop_free_cb, 0, PA_SEEK_RELATIVE) == 0);

    if (cal_state == CALIBRATION_DONE)
        pa_stream_set_write_callback(s, ctx->write_cb, ctx);
}

static void calibrate_read_cb(pa_stream *s, size_t nbytes, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;
    static double v = 0;
    static int skip = 0, confirm;

    pa_cvolume vol;
    pa_operation *o;
    int nsamp;
    float *in;
    size_t l;

    pa_assert_se(pa_stream_peek(s, (const void **)&in, &l) == 0);

    nsamp = l / ctx->fs;

    /* For each state or volume step change, throw out a few samples so we know
     * we're seeing the changed samples. */
    if (skip++ < 100)
        goto out;
    else
        skip = 0;

    switch (cal_state) {
        case CALIBRATION_ONE:
            /* Try to detect the sine wave. RMS is 0.5, */
            if (pa_rms(in, nsamp) < 0.40f) {
                confirm = 0;
                v += 0.02f;

                if (v > 1.0) {
                    pa_log_error("Capture signal too weak at 100%% volume (%g). Giving up.\n", pa_rms(in, nsamp));
                    pa_assert_not_reached();
                }

                pa_cvolume_set(&vol, ctx->sample_spec.channels, v * PA_VOLUME_NORM);
                o = pa_context_set_source_output_volume(ctx->context, pa_stream_get_index(s), &vol, NULL, NULL);
                pa_assert(o != NULL);
                pa_operation_unref(o);
            } else {
                /* Make sure the signal strength is steadily above our threshold */
                if (++confirm > 5) {
#if 0
                    pa_log_debug(stderr, "Capture volume = %g (%g)\n", v, pa_rms(in, nsamp));
#endif
                    cal_state = CALIBRATION_ZERO;
                }
            }

            break;

        case CALIBRATION_ZERO:
            /* Now make sure silence doesn't trigger a false positive because
             * of noise. */
            if (pa_rms(in, nsamp) > 0.1f) {
                fprintf(stderr, "Too much noise on capture (%g). Giving up.\n", pa_rms(in, nsamp));
                pa_assert_not_reached();
            }

            cal_state = CALIBRATION_DONE;
            pa_stream_set_read_callback(s, ctx->read_cb, ctx);

            break;

        default:
            break;
    }

out:
    pa_stream_drop(s);
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY: {
            pa_cvolume vol;
            pa_operation *o;

            /* Set volumes for calibration */
            if (s == ctx->play_stream) {
                pa_cvolume_set(&vol, ctx->sample_spec.channels, PA_VOLUME_NORM);
                o = pa_context_set_sink_input_volume(ctx->context, pa_stream_get_index(s), &vol, NULL, NULL);
            } else {
                pa_cvolume_set(&vol, ctx->sample_spec.channels, pa_sw_volume_from_linear(0.0));
                o = pa_context_set_source_output_volume(ctx->context, pa_stream_get_index(s), &vol, NULL, NULL);
            }

            if (!o) {
                pa_log_error("Could not set stream volume: %s\n", pa_strerror(pa_context_errno(ctx->context)));
                pa_assert_not_reached();
            } else
                pa_operation_unref(o);

            break;
        }

        case PA_STREAM_FAILED:
        default:
            pa_log_error("Stream error: %s\n", pa_strerror(pa_context_errno(ctx->context)));
            pa_assert_not_reached();
    }
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) {
    pa_lo_test_context *ctx = (pa_lo_test_context *) userdata;
    pa_mainloop_api *api;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            pa_buffer_attr buffer_attr;

            pa_make_realtime(4);

            /* Create playback stream */
            buffer_attr.maxlength = -1;
            buffer_attr.tlength = ctx->sample_spec.rate * ctx->fs * ctx->play_latency / 1000;
            buffer_attr.prebuf = 0; /* Setting prebuf to 0 guarantees us the stream will run synchronously, no matter what */
            buffer_attr.minreq = -1;
            buffer_attr.fragsize = -1;

            ctx->play_stream = pa_stream_new(c, "loopback: play", &ctx->sample_spec, NULL);
            pa_assert(ctx->play_stream != NULL);
            pa_stream_set_state_callback(ctx->play_stream, stream_state_callback, ctx);
            pa_stream_set_write_callback(ctx->play_stream, calibrate_write_cb, ctx);
            pa_stream_set_underflow_callback(ctx->play_stream, underflow_cb, userdata);

            pa_stream_connect_playback(ctx->play_stream, getenv("TEST_SINK"), &buffer_attr,
                    PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL);

            /* Create capture stream */
            buffer_attr.maxlength = -1;
            buffer_attr.tlength = (uint32_t) -1;
            buffer_attr.prebuf = 0;
            buffer_attr.minreq = (uint32_t) -1;
            buffer_attr.fragsize = ctx->sample_spec.rate * ctx->fs * ctx->rec_latency / 1000;

            ctx->rec_stream = pa_stream_new(c, "loopback: rec", &ctx->sample_spec, NULL);
            pa_assert(ctx->rec_stream != NULL);
            pa_stream_set_state_callback(ctx->rec_stream, stream_state_callback, ctx);
            pa_stream_set_read_callback(ctx->rec_stream, calibrate_read_cb, ctx);
            pa_stream_set_overflow_callback(ctx->rec_stream, overflow_cb, userdata);

            pa_stream_connect_record(ctx->rec_stream, getenv("TEST_SOURCE"), &buffer_attr,
                    PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);

            break;
        }

        case PA_CONTEXT_TERMINATED:
            api = pa_mainloop_get_api(ctx->mainloop);
            api->quit(api, 0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            pa_log_error("Context error: %s\n", pa_strerror(pa_context_errno(c)));
            pa_assert_not_reached();
    }
}

int pa_lo_test_init(pa_lo_test_context *ctx) {
    /* FIXME: need to deal with non-float samples at some point */
    pa_assert(ctx->sample_spec.format == PA_SAMPLE_FLOAT32);

    ctx->ss = pa_sample_size(&ctx->sample_spec);
    ctx->fs = pa_frame_size(&ctx->sample_spec);

    ctx->mainloop = pa_mainloop_new();
    ctx->context = pa_context_new(pa_mainloop_get_api(ctx->mainloop), ctx->context_name);

    pa_context_set_state_callback(ctx->context, context_state_callback, ctx);

    /* Connect the context */
    if (pa_context_connect(ctx->context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        pa_log_error("pa_context_connect() failed.\n");
        goto quit;
    }

    return 0;

quit:
    pa_context_unref(ctx->context);
    pa_mainloop_free(ctx->mainloop);

    return -1;
}

int pa_lo_test_run(pa_lo_test_context *ctx) {
    int ret;

    if (pa_mainloop_run(ctx->mainloop, &ret) < 0) {
        pa_log_error("pa_mainloop_run() failed.\n");
        return -1;
    }

    return 0;
}

void pa_lo_test_deinit(pa_lo_test_context *ctx) {
    if (ctx->play_stream) {
        pa_stream_disconnect(ctx->play_stream);
        pa_stream_unref(ctx->play_stream);
    }

    if (ctx->rec_stream) {
        pa_stream_disconnect(ctx->rec_stream);
        pa_stream_unref(ctx->rec_stream);
    }

    if (ctx->context)
        pa_context_unref(ctx->context);

    if (ctx->mainloop)
        pa_mainloop_free(ctx->mainloop);
}

float pa_rms(const float *s, int n) {
    float sq = 0;
    int i;

    for (i = 0; i < n; i++)
        sq += s[i] * s[i];

    return sqrtf(sq / n);
}
