/***
    This file is part of PulseAudio.

    Copyright 2011 Collabora Ltd.

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

#include <pulse/cdecl.h>

PA_C_DECL_BEGIN
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>

#include <pulse/timeval.h>
#include "echo-cancel.h"
PA_C_DECL_END

#include <audio_processing.h>
#include <module_common_types.h>

#define BLOCK_SIZE_US 10000

#define DEFAULT_HIGH_PASS_FILTER true
#define DEFAULT_NOISE_SUPPRESSION true
#define DEFAULT_ANALOG_GAIN_CONTROL true
#define DEFAULT_DIGITAL_GAIN_CONTROL false
#define DEFAULT_MOBILE false
#define DEFAULT_ROUTING_MODE "speakerphone"
#define DEFAULT_COMFORT_NOISE true
#define DEFAULT_DRIFT_COMPENSATION false

static const char* const valid_modargs[] = {
    "high_pass_filter",
    "noise_suppression",
    "analog_gain_control",
    "digital_gain_control",
    "mobile",
    "routing_mode",
    "comfort_noise",
    "drift_compensation",
    NULL
};

static int routing_mode_from_string(const char *rmode) {
    if (pa_streq(rmode, "quiet-earpiece-or-headset"))
        return webrtc::EchoControlMobile::kQuietEarpieceOrHeadset;
    else if (pa_streq(rmode, "earpiece"))
        return webrtc::EchoControlMobile::kEarpiece;
    else if (pa_streq(rmode, "loud-earpiece"))
        return webrtc::EchoControlMobile::kLoudEarpiece;
    else if (pa_streq(rmode, "speakerphone"))
        return webrtc::EchoControlMobile::kSpeakerphone;
    else if (pa_streq(rmode, "loud-speakerphone"))
        return webrtc::EchoControlMobile::kLoudSpeakerphone;
    else
        return -1;
}

bool pa_webrtc_ec_init(pa_core *c, pa_echo_canceller *ec,
                       pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                       pa_sample_spec *play_ss, pa_channel_map *play_map,
                       pa_sample_spec *out_ss, pa_channel_map *out_map,
                       uint32_t *nframes, const char *args) {
    webrtc::AudioProcessing *apm = NULL;
    bool hpf, ns, agc, dgc, mobile, cn;
    int rm = -1;
    pa_modargs *ma;

    if (!(ma = pa_modargs_new(args, valid_modargs))) {
        pa_log("Failed to parse submodule arguments.");
        goto fail;
    }

    hpf = DEFAULT_HIGH_PASS_FILTER;
    if (pa_modargs_get_value_boolean(ma, "high_pass_filter", &hpf) < 0) {
        pa_log("Failed to parse high_pass_filter value");
        goto fail;
    }

    ns = DEFAULT_NOISE_SUPPRESSION;
    if (pa_modargs_get_value_boolean(ma, "noise_suppression", &ns) < 0) {
        pa_log("Failed to parse noise_suppression value");
        goto fail;
    }

    agc = DEFAULT_ANALOG_GAIN_CONTROL;
    if (pa_modargs_get_value_boolean(ma, "analog_gain_control", &agc) < 0) {
        pa_log("Failed to parse analog_gain_control value");
        goto fail;
    }

    dgc = agc ? false : DEFAULT_DIGITAL_GAIN_CONTROL;
    if (pa_modargs_get_value_boolean(ma, "digital_gain_control", &dgc) < 0) {
        pa_log("Failed to parse digital_gain_control value");
        goto fail;
    }

    if (agc && dgc) {
        pa_log("You must pick only one between analog and digital gain control");
        goto fail;
    }

    mobile = DEFAULT_MOBILE;
    if (pa_modargs_get_value_boolean(ma, "mobile", &mobile) < 0) {
        pa_log("Failed to parse mobile value");
        goto fail;
    }

    ec->params.drift_compensation = DEFAULT_DRIFT_COMPENSATION;
    if (pa_modargs_get_value_boolean(ma, "drift_compensation", &ec->params.drift_compensation) < 0) {
        pa_log("Failed to parse drift_compensation value");
        goto fail;
    }

    if (mobile) {
        if (ec->params.drift_compensation) {
            pa_log("Can't use drift_compensation in mobile mode");
            goto fail;
        }

        if ((rm = routing_mode_from_string(pa_modargs_get_value(ma, "routing_mode", DEFAULT_ROUTING_MODE))) < 0) {
            pa_log("Failed to parse routing_mode value");
            goto fail;
        }

        cn = DEFAULT_COMFORT_NOISE;
        if (pa_modargs_get_value_boolean(ma, "comfort_noise", &cn) < 0) {
            pa_log("Failed to parse cn value");
            goto fail;
        }
    } else {
        if (pa_modargs_get_value(ma, "comfort_noise", NULL) || pa_modargs_get_value(ma, "routing_mode", NULL)) {
            pa_log("The routing_mode and comfort_noise options are only valid with mobile=true");
            goto fail;
        }
    }

    apm = webrtc::AudioProcessing::Create(0);

    out_ss->format = PA_SAMPLE_S16NE;
    *play_ss = *out_ss;
    /* FIXME: the implementation actually allows a different number of
     * source/sink channels. Do we want to support that? */
    *play_map = *out_map;
    *rec_ss = *out_ss;
    *rec_map = *out_map;

    apm->set_sample_rate_hz(out_ss->rate);

    apm->set_num_channels(out_ss->channels, out_ss->channels);
    apm->set_num_reverse_channels(play_ss->channels);

    if (hpf)
        apm->high_pass_filter()->Enable(true);

    if (!mobile) {
        if (ec->params.drift_compensation) {
            apm->echo_cancellation()->set_device_sample_rate_hz(out_ss->rate);
            apm->echo_cancellation()->enable_drift_compensation(true);
        } else {
            apm->echo_cancellation()->enable_drift_compensation(false);
        }

        apm->echo_cancellation()->Enable(true);
    } else {
        apm->echo_control_mobile()->set_routing_mode(static_cast<webrtc::EchoControlMobile::RoutingMode>(rm));
        apm->echo_control_mobile()->enable_comfort_noise(cn);
        apm->echo_control_mobile()->Enable(true);
    }

    if (ns) {
        apm->noise_suppression()->set_level(webrtc::NoiseSuppression::kHigh);
        apm->noise_suppression()->Enable(true);
    }

    if (agc || dgc) {
        if (mobile && rm <= webrtc::EchoControlMobile::kEarpiece) {
            /* Maybe this should be a knob, but we've got a lot of knobs already */
            apm->gain_control()->set_mode(webrtc::GainControl::kFixedDigital);
            ec->params.priv.webrtc.agc = false;
        } else if (dgc) {
            apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveDigital);
            ec->params.priv.webrtc.agc = false;
        } else {
            apm->gain_control()->set_mode(webrtc::GainControl::kAdaptiveAnalog);
            if (apm->gain_control()->set_analog_level_limits(0, PA_VOLUME_NORM-1) != apm->kNoError) {
                pa_log("Failed to initialise AGC");
                goto fail;
            }
            ec->params.priv.webrtc.agc = true;
        }

        apm->gain_control()->Enable(true);
    }

    apm->voice_detection()->Enable(true);

    ec->params.priv.webrtc.apm = apm;
    ec->params.priv.webrtc.sample_spec = *out_ss;
    ec->params.priv.webrtc.blocksize = (uint64_t)pa_bytes_per_second(out_ss) * BLOCK_SIZE_US / PA_USEC_PER_SEC;
    *nframes = ec->params.priv.webrtc.blocksize / pa_frame_size(out_ss);

    pa_modargs_free(ma);
    return true;

fail:
    if (ma)
        pa_modargs_free(ma);
    if (apm)
        webrtc::AudioProcessing::Destroy(apm);

    return false;
}

void pa_webrtc_ec_play(pa_echo_canceller *ec, const uint8_t *play) {
    webrtc::AudioProcessing *apm = (webrtc::AudioProcessing*)ec->params.priv.webrtc.apm;
    webrtc::AudioFrame play_frame;
    const pa_sample_spec *ss = &ec->params.priv.webrtc.sample_spec;

    play_frame._audioChannel = ss->channels;
    play_frame._frequencyInHz = ss->rate;
    play_frame._payloadDataLengthInSamples = ec->params.priv.webrtc.blocksize / pa_frame_size(ss);
    memcpy(play_frame._payloadData, play, ec->params.priv.webrtc.blocksize);

    apm->AnalyzeReverseStream(&play_frame);
}

void pa_webrtc_ec_record(pa_echo_canceller *ec, const uint8_t *rec, uint8_t *out) {
    webrtc::AudioProcessing *apm = (webrtc::AudioProcessing*)ec->params.priv.webrtc.apm;
    webrtc::AudioFrame out_frame;
    const pa_sample_spec *ss = &ec->params.priv.webrtc.sample_spec;
    pa_cvolume v;

    out_frame._audioChannel = ss->channels;
    out_frame._frequencyInHz = ss->rate;
    out_frame._payloadDataLengthInSamples = ec->params.priv.webrtc.blocksize / pa_frame_size(ss);
    memcpy(out_frame._payloadData, rec, ec->params.priv.webrtc.blocksize);

    if (ec->params.priv.webrtc.agc) {
        pa_cvolume_init(&v);
        pa_echo_canceller_get_capture_volume(ec, &v);
        apm->gain_control()->set_stream_analog_level(pa_cvolume_avg(&v));
    }

    apm->set_stream_delay_ms(0);
    apm->ProcessStream(&out_frame);

    if (ec->params.priv.webrtc.agc) {
        pa_cvolume_set(&v, ss->channels, apm->gain_control()->stream_analog_level());
        pa_echo_canceller_set_capture_volume(ec, &v);
    }

    memcpy(out, out_frame._payloadData, ec->params.priv.webrtc.blocksize);
}

void pa_webrtc_ec_set_drift(pa_echo_canceller *ec, float drift) {
    webrtc::AudioProcessing *apm = (webrtc::AudioProcessing*)ec->params.priv.webrtc.apm;
    const pa_sample_spec *ss = &ec->params.priv.webrtc.sample_spec;

    apm->echo_cancellation()->set_stream_drift_samples(drift * ec->params.priv.webrtc.blocksize / pa_frame_size(ss));
}

void pa_webrtc_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out) {
    pa_webrtc_ec_play(ec, play);
    pa_webrtc_ec_record(ec, rec, out);
}

void pa_webrtc_ec_done(pa_echo_canceller *ec) {
    if (ec->params.priv.webrtc.apm) {
        webrtc::AudioProcessing::Destroy((webrtc::AudioProcessing*)ec->params.priv.webrtc.apm);
        ec->params.priv.webrtc.apm = NULL;
    }
}
