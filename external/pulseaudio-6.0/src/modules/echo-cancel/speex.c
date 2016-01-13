/***
    This file is part of PulseAudio.

    Copyright 2010 Wim Taymans <wim.taymans@gmail.com>

    Contributor: Arun Raghavan <arun.raghavan@collabora.co.uk>

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

#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include "echo-cancel.h"

/* should be between 10-20 ms */
#define DEFAULT_FRAME_SIZE_MS 20
/* should be between 100-500 ms */
#define DEFAULT_FILTER_SIZE_MS 200
#define DEFAULT_AGC_ENABLED true
#define DEFAULT_DENOISE_ENABLED true
#define DEFAULT_ECHO_SUPPRESS_ENABLED true
#define DEFAULT_ECHO_SUPPRESS_ATTENUATION 0

static const char* const valid_modargs[] = {
    "frame_size_ms",
    "filter_size_ms",
    "agc",
    "denoise",
    "echo_suppress",
    "echo_suppress_attenuation",
    "echo_suppress_attenuation_active",
    NULL
};

static void pa_speex_ec_fixate_spec(pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                                    pa_sample_spec *play_ss, pa_channel_map *play_map,
                                    pa_sample_spec *out_ss, pa_channel_map *out_map) {
    out_ss->format = PA_SAMPLE_S16NE;

    *play_ss = *out_ss;
    *play_map = *out_map;
    *rec_ss = *out_ss;
    *rec_map = *out_map;
}

static bool pa_speex_ec_preprocessor_init(pa_echo_canceller *ec, pa_sample_spec *out_ss, uint32_t nframes, pa_modargs *ma) {
    bool agc;
    bool denoise;
    bool echo_suppress;
    int32_t echo_suppress_attenuation;
    int32_t echo_suppress_attenuation_active;

    agc = DEFAULT_AGC_ENABLED;
    if (pa_modargs_get_value_boolean(ma, "agc", &agc) < 0) {
        pa_log("Failed to parse agc value");
        goto fail;
    }

    denoise = DEFAULT_DENOISE_ENABLED;
    if (pa_modargs_get_value_boolean(ma, "denoise", &denoise) < 0) {
        pa_log("Failed to parse denoise value");
        goto fail;
    }

    echo_suppress = DEFAULT_ECHO_SUPPRESS_ENABLED;
    if (pa_modargs_get_value_boolean(ma, "echo_suppress", &echo_suppress) < 0) {
        pa_log("Failed to parse echo_suppress value");
        goto fail;
    }

    echo_suppress_attenuation = DEFAULT_ECHO_SUPPRESS_ATTENUATION;
    if (pa_modargs_get_value_s32(ma, "echo_suppress_attenuation", &echo_suppress_attenuation) < 0) {
        pa_log("Failed to parse echo_suppress_attenuation value");
        goto fail;
    }
    if (echo_suppress_attenuation > 0) {
        pa_log("echo_suppress_attenuation should be a negative dB value");
        goto fail;
    }

    echo_suppress_attenuation_active = DEFAULT_ECHO_SUPPRESS_ATTENUATION;
    if (pa_modargs_get_value_s32(ma, "echo_suppress_attenuation_active", &echo_suppress_attenuation_active) < 0) {
        pa_log("Failed to parse echo_suppress_attenuation_active value");
        goto fail;
    }
    if (echo_suppress_attenuation_active > 0) {
        pa_log("echo_suppress_attenuation_active should be a negative dB value");
        goto fail;
    }

    if (agc || denoise || echo_suppress) {
        spx_int32_t tmp;

        if (out_ss->channels != 1) {
            pa_log("AGC, denoising and echo suppression only work with channels=1");
            goto fail;
        }

        ec->params.priv.speex.pp_state = speex_preprocess_state_init(nframes, out_ss->rate);

        tmp = agc;
        speex_preprocess_ctl(ec->params.priv.speex.pp_state, SPEEX_PREPROCESS_SET_AGC, &tmp);

        tmp = denoise;
        speex_preprocess_ctl(ec->params.priv.speex.pp_state, SPEEX_PREPROCESS_SET_DENOISE, &tmp);

        if (echo_suppress) {
            if (echo_suppress_attenuation)
                speex_preprocess_ctl(ec->params.priv.speex.pp_state, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS,
                                     &echo_suppress_attenuation);

            if (echo_suppress_attenuation_active) {
                speex_preprocess_ctl(ec->params.priv.speex.pp_state, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE,
                                     &echo_suppress_attenuation_active);
            }

            speex_preprocess_ctl(ec->params.priv.speex.pp_state, SPEEX_PREPROCESS_SET_ECHO_STATE,
                                 ec->params.priv.speex.state);
        }

        pa_log_info("Loaded speex preprocessor with params: agc=%s, denoise=%s, echo_suppress=%s", pa_yes_no(agc),
                    pa_yes_no(denoise), pa_yes_no(echo_suppress));
    } else
        pa_log_info("All preprocessing options are disabled");

    return true;

fail:
    return false;
}

bool pa_speex_ec_init(pa_core *c, pa_echo_canceller *ec,
                      pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                      pa_sample_spec *play_ss, pa_channel_map *play_map,
                      pa_sample_spec *out_ss, pa_channel_map *out_map,
                      uint32_t *nframes, const char *args) {
    int rate;
    uint32_t frame_size_ms, filter_size_ms;
    pa_modargs *ma;

    if (!(ma = pa_modargs_new(args, valid_modargs))) {
        pa_log("Failed to parse submodule arguments.");
        goto fail;
    }

    filter_size_ms = DEFAULT_FILTER_SIZE_MS;
    if (pa_modargs_get_value_u32(ma, "filter_size_ms", &filter_size_ms) < 0 || filter_size_ms < 1 || filter_size_ms > 2000) {
        pa_log("Invalid filter_size_ms specification");
        goto fail;
    }

    frame_size_ms = DEFAULT_FRAME_SIZE_MS;
    if (pa_modargs_get_value_u32(ma, "frame_size_ms", &frame_size_ms) < 0 || frame_size_ms < 1 || frame_size_ms > 200) {
        pa_log("Invalid frame_size_ms specification");
        goto fail;
    }

    pa_speex_ec_fixate_spec(rec_ss, rec_map, play_ss, play_map, out_ss, out_map);

    rate = out_ss->rate;
    *nframes = pa_echo_canceller_blocksize_power2(rate, frame_size_ms);

    pa_log_debug ("Using nframes %d, channels %d, rate %d", *nframes, out_ss->channels, out_ss->rate);
    ec->params.priv.speex.state = speex_echo_state_init_mc(*nframes, (rate * filter_size_ms) / 1000, out_ss->channels, out_ss->channels);

    if (!ec->params.priv.speex.state)
        goto fail;

    speex_echo_ctl(ec->params.priv.speex.state, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);

    if (!pa_speex_ec_preprocessor_init(ec, out_ss, *nframes, ma))
        goto fail;

    pa_modargs_free(ma);
    return true;

fail:
    if (ma)
        pa_modargs_free(ma);
    if (ec->params.priv.speex.pp_state) {
        speex_preprocess_state_destroy(ec->params.priv.speex.pp_state);
        ec->params.priv.speex.pp_state = NULL;
    }
    if (ec->params.priv.speex.state) {
        speex_echo_state_destroy(ec->params.priv.speex.state);
        ec->params.priv.speex.state = NULL;
    }
    return false;
}

void pa_speex_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out) {
    speex_echo_cancellation(ec->params.priv.speex.state, (const spx_int16_t *) rec, (const spx_int16_t *) play,
                            (spx_int16_t *) out);

    /* preprecessor is run after AEC. This is not a mistake! */
    if (ec->params.priv.speex.pp_state)
        speex_preprocess_run(ec->params.priv.speex.pp_state, (spx_int16_t *) out);
}

void pa_speex_ec_done(pa_echo_canceller *ec) {
    if (ec->params.priv.speex.pp_state) {
        speex_preprocess_state_destroy(ec->params.priv.speex.pp_state);
        ec->params.priv.speex.pp_state = NULL;
    }

    if (ec->params.priv.speex.state) {
        speex_echo_state_destroy(ec->params.priv.speex.state);
        ec->params.priv.speex.state = NULL;
    }
}
