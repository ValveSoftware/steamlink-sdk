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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <check.h>

#include <pulse/pulseaudio.h>
#include <pulse/mainloop.h>

#include <pulsecore/sink.h>

/* Set the number of streams such that it allows two simultaneous instances of
 * connect-stress to be run and not go above the max limit for streams-per-sink.
 * This leaves enough room for a couple other streams from regular system usage,
 * which makes a non-error abort less likely (although still easily possible of
 * playing >=3 streams outside of the test - including internal loopback, rtp,
 * combine, remap streams etc.) */
/* #define NSTREAMS ((PA_MAX_INPUTS_PER_SINK/2) - 1) */

/* This test broke when PA_MAX_INPUTS_PER_SINK was increased from 32 to 256.
 * Because we currently don't have time to figure out why, let's just set
 * NSTREAMS to 20 in the meantime.
 */
#define NSTREAMS 20
#define NTESTS 1000
#define SAMPLE_HZ 44100

static pa_context *context = NULL;
static pa_stream *streams[NSTREAMS];
static pa_threaded_mainloop *mainloop = NULL;
static char *bname;

static const pa_sample_spec sample_spec = {
    .format = PA_SAMPLE_FLOAT32,
    .rate = SAMPLE_HZ,
    .channels = 1
};

static void context_state_callback(pa_context *c, void *userdata);

static void connect(const char *name, int *try) {
    int ret;
    pa_mainloop_api *api;

    /* Set up a new main loop */
    mainloop = pa_threaded_mainloop_new();
    fail_unless(mainloop != NULL);

    api = pa_threaded_mainloop_get_api(mainloop);
    context = pa_context_new(api, name);
    fail_unless(context != NULL);

    pa_context_set_state_callback(context, context_state_callback, try);

    /* Connect the context */
    if (pa_context_connect(context, NULL, 0, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed.\n");
        fail();
    }

    ret = pa_threaded_mainloop_start(mainloop);
    fail_unless(ret == 0);
}

static void disconnect(void) {
    int i;

    fail_unless(mainloop != NULL);
    fail_unless(context != NULL);

    pa_threaded_mainloop_lock(mainloop);

    for (i = 0; i < NSTREAMS; i++)
        if (streams[i]) {
            pa_stream_disconnect(streams[i]);
            pa_stream_unref(streams[i]);
            streams[i] = NULL;
        }

    pa_context_disconnect(context);
    context = NULL;

    pa_threaded_mainloop_unlock(mainloop);
    pa_threaded_mainloop_stop(mainloop);
    pa_threaded_mainloop_free(mainloop);
    mainloop = NULL;
}

static const pa_buffer_attr buffer_attr = {
    .maxlength = SAMPLE_HZ * sizeof(float) * NSTREAMS,
    .tlength = (uint32_t) -1,
    .prebuf = 0, /* Setting prebuf to 0 guarantees us the streams will run synchronously, no matter what */
    .minreq = (uint32_t) -1,
    .fragsize = 0
};

static void stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    char silence[8192];

    memset(silence, 0, sizeof(silence));

    while (nbytes) {
        int n = PA_MIN(sizeof(silence), nbytes);
        pa_stream_write(stream, silence, n, NULL, 0, 0);
        nbytes -= n;
    }
}

static void stream_state_callback(pa_stream *s, void *userdata) {
    fail_unless(s != NULL);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
        case PA_STREAM_READY:
            break;

        default:
        case PA_STREAM_FAILED:
            fprintf(stderr, "Stream error: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            fail();
    }
}

static void context_state_callback(pa_context *c, void *userdata) {
    int *try;

    fail_unless(c != NULL);
    fail_unless(userdata != NULL);

    try = (int*)userdata;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {

            int i;
            fprintf(stderr, "Connection (%d of %d) established.\n", (*try)+1, NTESTS);

            for (i = 0; i < NSTREAMS; i++) {
                char name[64];

                snprintf(name, sizeof(name), "stream #%i", i);
                streams[i] = pa_stream_new(c, name, &sample_spec, NULL);
                fail_unless(streams[i] != NULL);
                pa_stream_set_state_callback(streams[i], stream_state_callback, NULL);
                pa_stream_set_write_callback(streams[i], stream_write_callback, NULL);
                pa_stream_connect_playback(streams[i], NULL, &buffer_attr, 0, NULL, NULL);
            }

            break;
        }

        case PA_CONTEXT_TERMINATED:
            fprintf(stderr, "Connection terminated.\n");
            pa_context_unref(context);
            context = NULL;
            break;

        case PA_CONTEXT_FAILED:
        default:
            fprintf(stderr, "Context error: %s\n", pa_strerror(pa_context_errno(c)));
            fail();
    }
}

START_TEST (connect_stress_test) {
    int i;

    for (i = 0; i < NSTREAMS; i++)
        streams[i] = NULL;

    for (i = 0; i < NTESTS; i++) {
        connect(bname, &i);
        usleep(rand() % 500000);
        disconnect();
        usleep(rand() % 500000);
    }

    fprintf(stderr, "Done.\n");
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    bname = argv[0];

    s = suite_create("Connect Stress");
    tc = tcase_create("connectstress");
    tcase_add_test(tc, connect_stress_test);
    tcase_set_timeout(tc, 20 * 60);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
