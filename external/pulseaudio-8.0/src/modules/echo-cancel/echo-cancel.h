/***
    This file is part of PulseAudio.

    Copyright 2010 Arun Raghavan <arun.raghavan@collabora.co.uk>

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

#ifndef fooechocancelhfoo
#define fooechocancelhfoo

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/sample.h>
#include <pulse/channelmap.h>
#include <pulsecore/core.h>
#include <pulsecore/macro.h>

#ifdef HAVE_SPEEX
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif

#include "adrian.h"

/* Common data structures */

typedef struct pa_echo_canceller_msg pa_echo_canceller_msg;

typedef struct pa_echo_canceller_params pa_echo_canceller_params;

struct pa_echo_canceller_params {
    union {
        struct {
            pa_sample_spec out_ss;
        } null;
#ifdef HAVE_SPEEX
        struct {
            SpeexEchoState *state;
            SpeexPreprocessState *pp_state;
        } speex;
#endif
#ifdef HAVE_ADRIAN_EC
        struct {
            uint32_t blocksize;
            AEC *aec;
        } adrian;
#endif
#ifdef HAVE_WEBRTC
        struct {
            /* This is a void* so that we don't have to convert this whole file
             * to C++ linkage. apm is a pointer to an AudioProcessing object */
            void *apm;
            uint32_t blocksize;
            pa_sample_spec sample_spec;
            bool agc;
        } webrtc;
#endif
        /* each canceller-specific structure goes here */
    } priv;

    /* Set this if canceller can do drift compensation. Also see set_drift()
     * below */
    bool drift_compensation;
};

typedef struct pa_echo_canceller pa_echo_canceller;

struct pa_echo_canceller {
    /* Initialise canceller engine. */
    bool   (*init)                      (pa_core *c,
                                         pa_echo_canceller *ec,
                                         pa_sample_spec *rec_ss,
                                         pa_channel_map *rec_map,
                                         pa_sample_spec *play_ss,
                                         pa_channel_map *play_map,
                                         pa_sample_spec *out_ss,
                                         pa_channel_map *out_map,
                                         uint32_t *nframes,
                                         const char *args);

    /* You should have only one of play()+record() or run() set. The first
     * works under the assumption that you'll handle buffering and matching up
     * samples yourself. If you set run(), module-echo-cancel will handle
     * synchronising the playback and record streams. */

    /* Feed the engine 'nframes' playback frames. */
    void        (*play)                 (pa_echo_canceller *ec, const uint8_t *play);
    /* Feed the engine 'nframes' record frames. nframes processed frames are
     * returned in out. */
    void        (*record)               (pa_echo_canceller *ec, const uint8_t *rec, uint8_t *out);
    /* Feed the engine nframes playback and record frames, with a reasonable
     * effort at keeping the two in sync. nframes processed frames are
     * returned in out. */
    void        (*run)                  (pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out);

    /* Optional callback to set the drift, expressed as the ratio of the
     * difference in number of playback and capture samples to the number of
     * capture samples, for some instant of time. This is used only if the
     * canceller signals that it supports drift compensation, and is called
     * before record(). The actual implementation needs to derive drift based
     * on point samples -- the individual values are not accurate enough to use
     * as-is. */
    /* NOTE: the semantics of this function might change in the future. */
    void        (*set_drift)            (pa_echo_canceller *ec, float drift);

    /* Free up resources. */
    void        (*done)                 (pa_echo_canceller *ec);

    /* Structure with common and engine-specific canceller parameters. */
    pa_echo_canceller_params params;

    /* msgobject that can be used to send messages back to the main thread */
    pa_echo_canceller_msg *msg;
};

/* Functions to be used by the canceller analog gain control routines */
void pa_echo_canceller_get_capture_volume(pa_echo_canceller *ec, pa_cvolume *v);
void pa_echo_canceller_set_capture_volume(pa_echo_canceller *ec, pa_cvolume *v);

/* Computes EC block size in frames (rounded down to nearest power-of-2) based
 * on sample rate and milliseconds. */
uint32_t pa_echo_canceller_blocksize_power2(unsigned rate, unsigned ms);

/* Null canceller functions */
bool pa_null_ec_init(pa_core *c, pa_echo_canceller *ec,
                     pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                     pa_sample_spec *play_ss, pa_channel_map *play_map,
                     pa_sample_spec *out_ss, pa_channel_map *out_map,
                     uint32_t *nframes, const char *args);
void pa_null_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out);
void pa_null_ec_done(pa_echo_canceller *ec);

#ifdef HAVE_SPEEX
/* Speex canceller functions */
bool pa_speex_ec_init(pa_core *c, pa_echo_canceller *ec,
                      pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                      pa_sample_spec *play_ss, pa_channel_map *play_map,
                      pa_sample_spec *out_ss, pa_channel_map *out_map,
                      uint32_t *nframes, const char *args);
void pa_speex_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out);
void pa_speex_ec_done(pa_echo_canceller *ec);
#endif

#ifdef HAVE_ADRIAN_EC
/* Adrian Andre's echo canceller */
bool pa_adrian_ec_init(pa_core *c, pa_echo_canceller *ec,
                       pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                       pa_sample_spec *play_ss, pa_channel_map *play_map,
                       pa_sample_spec *out_ss, pa_channel_map *out_map,
                       uint32_t *nframes, const char *args);
void pa_adrian_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out);
void pa_adrian_ec_done(pa_echo_canceller *ec);
#endif

#ifdef HAVE_WEBRTC
/* WebRTC canceller functions */
PA_C_DECL_BEGIN
bool pa_webrtc_ec_init(pa_core *c, pa_echo_canceller *ec,
                       pa_sample_spec *rec_ss, pa_channel_map *rec_map,
                       pa_sample_spec *play_ss, pa_channel_map *play_map,
                       pa_sample_spec *out_ss, pa_channel_map *out_map,
                       uint32_t *nframes, const char *args);
void pa_webrtc_ec_play(pa_echo_canceller *ec, const uint8_t *play);
void pa_webrtc_ec_record(pa_echo_canceller *ec, const uint8_t *rec, uint8_t *out);
void pa_webrtc_ec_set_drift(pa_echo_canceller *ec, float drift);
void pa_webrtc_ec_run(pa_echo_canceller *ec, const uint8_t *rec, const uint8_t *play, uint8_t *out);
void pa_webrtc_ec_done(pa_echo_canceller *ec);
PA_C_DECL_END
#endif

#endif /* fooechocancelhfoo */
