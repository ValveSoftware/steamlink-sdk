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

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <check.h>

#include <pulse/pulseaudio.h>
#include <pulse/mainloop.h>

#define NSTREAMS 4
#define SINE_HZ 440
#define SAMPLE_HZ 8000

static pa_context *context = NULL;
static pa_stream *streams[NSTREAMS];
static pa_mainloop_api *mainloop_api = NULL;
static const char *bname;

static float data[SAMPLE_HZ]; /* one second space */

static int n_streams_ready = 0;

static const pa_buffer_attr buffer_attr = {
    .maxlength = SAMPLE_HZ*sizeof(float)*NSTREAMS, /* exactly space for the entire play time */
    .tlength = (uint32_t) -1,
    .prebuf = 0, /* Setting prebuf to 0 guarantees us the streams will run synchronously, no matter what */
    .minreq = (uint32_t) -1,
    .fragsize = 0
};

static void nop_free_cb(void *p) {}

static void underflow_cb(struct pa_stream *s, void *userdata) {
    int i = (int) (long) userdata;

    fprintf(stderr, "Stream %i finished\n", i);

    if (++n_streams_ready >= 2*NSTREAMS) {
        fprintf(stderr, "We're done\n");
        mainloop_api->quit(mainloop_api, 0);
    }
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata) {
    fail_unless(s != NULL);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY: {

            int r, i = (int) (long) userdata;

            fprintf(stderr, "Writing data to stream %i.\n", i);

            r = pa_stream_write(s, data, sizeof(data), nop_free_cb, (int64_t) sizeof(data) * (int64_t) i, PA_SEEK_ABSOLUTE);
            fail_unless(r == 0);

            /* Be notified when this stream is drained */
            pa_stream_set_underflow_callback(s, underflow_cb, userdata);

            /* All streams have been set up, let's go! */
            if (++n_streams_ready >= NSTREAMS) {
                fprintf(stderr, "Uncorking\n");
                pa_operation_unref(pa_stream_cork(s, 0, NULL, NULL));
            }

            break;
        }

        default:
        case PA_STREAM_FAILED:
            fprintf(stderr, "Stream error: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            fail();
    }
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) {
    fail_unless(c != NULL);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {

            int i;
            fprintf(stderr, "Connection established.\n");

            for (i = 0; i < NSTREAMS; i++) {
                char name[64];
                pa_format_info *formats[1];

                formats[0] = pa_format_info_new();
                formats[0]->encoding = PA_ENCODING_PCM;
                pa_format_info_set_sample_format(formats[0], PA_SAMPLE_FLOAT32);
                pa_format_info_set_rate(formats[0], SAMPLE_HZ);
                pa_format_info_set_channels(formats[0], 1);

                fprintf(stderr, "Creating stream %i\n", i);

                snprintf(name, sizeof(name), "stream #%i", i);

                streams[i] = pa_stream_new_extended(c, name, formats, 1, NULL);
                fail_unless(streams[i] != NULL);
                pa_stream_set_state_callback(streams[i], stream_state_callback, (void*) (long) i);
                pa_stream_connect_playback(streams[i], NULL, &buffer_attr, PA_STREAM_START_CORKED, NULL, i == 0 ? NULL : streams[0]);

                pa_format_info_free(formats[0]);
            }

            break;
        }

        case PA_CONTEXT_TERMINATED:
            mainloop_api->quit(mainloop_api, 0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            fprintf(stderr, "Context error: %s\n", pa_strerror(pa_context_errno(c)));
            fail();
    }
}

START_TEST (extended_test) {
    pa_mainloop* m = NULL;
    int i, ret = 1;

    for (i = 0; i < SAMPLE_HZ; i++)
        data[i] = (float) sin(((double) i/SAMPLE_HZ)*2*M_PI*SINE_HZ)/2;

    for (i = 0; i < NSTREAMS; i++)
        streams[i] = NULL;

    /* Set up a new main loop */
    m = pa_mainloop_new();
    fail_unless(m != NULL);

    mainloop_api = pa_mainloop_get_api(m);

    context = pa_context_new(mainloop_api, bname);
    fail_unless(context != NULL);

    pa_context_set_state_callback(context, context_state_callback, NULL);

    /* Connect the context */
    if (pa_context_connect(context, NULL, 0, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed.\n");
        goto quit;
    }

    if (pa_mainloop_run(m, &ret) < 0)
        fprintf(stderr, "pa_mainloop_run() failed.\n");

quit:
    pa_context_unref(context);

    for (i = 0; i < NSTREAMS; i++)
        if (streams[i])
            pa_stream_unref(streams[i]);

    pa_mainloop_free(m);

    fail_unless(ret == 0);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    bname = argv[0];

    s = suite_create("Extended");
    tc = tcase_create("extended");
    tcase_add_test(tc, extended_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
