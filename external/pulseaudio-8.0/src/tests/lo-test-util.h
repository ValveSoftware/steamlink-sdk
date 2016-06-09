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

#include <pulse/pulseaudio.h>

typedef struct pa_lo_test_context {
    /* Tests need to set these */
    const char *context_name;

    pa_sample_spec sample_spec;
    int play_latency; /* ms */
    int rec_latency; /* ms */

    pa_stream_request_cb_t write_cb, read_cb;

    /* These are set by lo_test_init() */
    pa_mainloop *mainloop;
    pa_context *context;

    pa_stream *play_stream, *rec_stream;

    int ss, fs; /* sample size, frame size for convenience */
} pa_lo_test_context;

/* Initialise the test parameters, connect */
int pa_lo_test_init(pa_lo_test_context *ctx);
/* Start running the test */
int pa_lo_test_run(pa_lo_test_context *ctx);
/* Clean up */
void pa_lo_test_deinit(pa_lo_test_context *ctx);

/* Return RMS for the given signal. Assumes the data is a single channel for
 * simplicity */
float pa_rms(const float *s, int n);
