/***
    This file is part of PulseAudio.

    Copyright 2010 Arun Raghavan <arun.raghavan@collabora.co.uk>

    Contributor: Wim Taymans <wim.taymans@gmail.com>

    The actual implementation is taken from the sources at
    http://andreadrian.de/intercom/ - for the license, look for
    adrian-license.txt in the same directory as this file.

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

#include <pulse/xmalloc.h>

#include <pulsecore/modargs.h>

#include "echo-cancel.h"

/* should be between 10-20 ms */
#define DEFAULT_FRAME_SIZE_MS 20

static const char* const valid_modargs[] = {
    "frame_size_ms",
    NULL
};

static void pa_adrian_ec_fixate_spec(pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                                     pa_sample_spec *play_ss, pa_channel_map *play_map,
                                     pa_sample_spec *out_ss, pa_channel_map *out_map) {
    out_ss->format = PA_SAMPLE_S16NE;
    out_ss->channels = 1;
    pa_channel_map_init_mono(out_map);

    *play_ss = *out_ss;
    *play_map = *out_map;
    *rec_ss = *out_ss;
    *rec_map = *out_map;
}

bool pa_adrian_ec_init(pa_core *c, pa_echo_canceller *ec,
                       pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                       pa_sample_spec *play_ss, pa_channel_map *play_map,
                       pa_sample_spec *out_ss, pa_channel_map *out_map,
                       uint32_t *nframes, const char *args) {
    int rate, have_vector = 0;
    uint32_t frame_size_ms;
    pa_modargs *ma;

    if (!(ma = pa_modargs_new(args, valid_modargs))) {
        pa_log("Failed to parse submodule arguments.");
        goto fail;
    }

    frame_size_ms = DEFAULT_FRAME_SIZE_MS;
    if (pa_modargs_get_value_u32(ma, "frame_size_ms", &frame_size_ms) < 0 || frame_size_ms < 1 || frame_size_ms > 200) {
        pa_log("Invalid frame_size_ms specification");
        goto fail;
    }

    pa_adrian_ec_fixate_spec(rec_ss, rec_map, play_ss, play_map, out_ss, out_map);

    rate = out_ss->rate;
    *nframes = (rate * frame_size_ms) / 1000;
    ec->params.priv.adrian.blocksize = (*nframes) * pa_frame_size(out_ss);

    pa_log_debug ("Using nframes %d, blocksize %u, channels %d, rate %d", *nframes, ec->params.priv.adrian.blocksize, out_ss->channels, out_ss->rate);

    /* For now we only support SSE */
    if (c->cpu_info.cpu_type == PA_CPU_X86 && (c->cpu_info.flags.x86 & PA_CPU_X86_SSE))
        have_vector = 1;

    ec->params.priv.adrian.aec = AEC_init(rate, have_vector);
    if (!ec->params.priv.adrian.aec)
        goto fail;

    pa_modargs_free(ma);
    return true;

fail:
    if (ma)
        pa_modargs_free(ma);
    return false;
}

void pa_adrian_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out) {
    unsigned int i;

    for (i = 0; i < ec->params.priv.adrian.blocksize; i += 2) {
        /* We know it's S16NE mono data */
        int r = *(int16_t *)(rec + i);
        int p = *(int16_t *)(play + i);
        *(int16_t *)(out + i) = (int16_t) AEC_doAEC(ec->params.priv.adrian.aec, r, p);
    }
}

void pa_adrian_ec_done(pa_echo_canceller *ec) {
    if (ec->params.priv.adrian.aec) {
        AEC_done(ec->params.priv.adrian.aec);
        ec->params.priv.adrian.aec = NULL;
    }
}
