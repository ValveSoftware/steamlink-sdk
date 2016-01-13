/***
    This file is part of PulseAudio.

    Copyright 2010 Wim Taymans <wim.taymans@gmail.com>

    Based on module-virtual-sink.c
             module-virtual-source.c
             module-loopback.c

        Copyright 2010 Intel Corporation
        Contributor: Pierre-Louis Bossart <pierre-louis.bossart@intel.com>

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

#include <stdio.h>
#include <math.h>

#include "echo-cancel.h"

#include <pulse/xmalloc.h>
#include <pulse/timeval.h>
#include <pulse/rtclock.h>

#include <pulsecore/i18n.h>
#include <pulsecore/atomic.h>
#include <pulsecore/macro.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/ltdl-helper.h>

#include "module-echo-cancel-symdef.h"

PA_MODULE_AUTHOR("Wim Taymans");
PA_MODULE_DESCRIPTION("Echo Cancellation");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        _("source_name=<name for the source> "
          "source_properties=<properties for the source> "
          "source_master=<name of source to filter> "
          "sink_name=<name for the sink> "
          "sink_properties=<properties for the sink> "
          "sink_master=<name of sink to filter> "
          "adjust_time=<how often to readjust rates in s> "
          "adjust_threshold=<how much drift to readjust after in ms> "
          "format=<sample format> "
          "rate=<sample rate> "
          "channels=<number of channels> "
          "channel_map=<channel map> "
          "aec_method=<implementation to use> "
          "aec_args=<parameters for the AEC engine> "
          "save_aec=<save AEC data in /tmp> "
          "autoloaded=<set if this module is being loaded automatically> "
          "use_volume_sharing=<yes or no> "
        ));

/* NOTE: Make sure the enum and ec_table are maintained in the correct order */
typedef enum {
    PA_ECHO_CANCELLER_INVALID = -1,
    PA_ECHO_CANCELLER_NULL,
#ifdef HAVE_SPEEX
    PA_ECHO_CANCELLER_SPEEX,
#endif
#ifdef HAVE_ADRIAN_EC
    PA_ECHO_CANCELLER_ADRIAN,
#endif
#ifdef HAVE_WEBRTC
    PA_ECHO_CANCELLER_WEBRTC,
#endif
} pa_echo_canceller_method_t;

#ifdef HAVE_WEBRTC
#define DEFAULT_ECHO_CANCELLER "webrtc"
#else
#define DEFAULT_ECHO_CANCELLER "speex"
#endif

static const pa_echo_canceller ec_table[] = {
    {
        /* Null, Dummy echo canceller (just copies data) */
        .init                   = pa_null_ec_init,
        .run                    = pa_null_ec_run,
        .done                   = pa_null_ec_done,
    },
#ifdef HAVE_SPEEX
    {
        /* Speex */
        .init                   = pa_speex_ec_init,
        .run                    = pa_speex_ec_run,
        .done                   = pa_speex_ec_done,
    },
#endif
#ifdef HAVE_ADRIAN_EC
    {
        /* Adrian Andre's NLMS implementation */
        .init                   = pa_adrian_ec_init,
        .run                    = pa_adrian_ec_run,
        .done                   = pa_adrian_ec_done,
    },
#endif
#ifdef HAVE_WEBRTC
    {
        /* WebRTC's audio processing engine */
        .init                   = pa_webrtc_ec_init,
        .play                   = pa_webrtc_ec_play,
        .record                 = pa_webrtc_ec_record,
        .set_drift              = pa_webrtc_ec_set_drift,
        .run                    = pa_webrtc_ec_run,
        .done                   = pa_webrtc_ec_done,
    },
#endif
};

#define DEFAULT_RATE 32000
#define DEFAULT_CHANNELS 1
#define DEFAULT_ADJUST_TIME_USEC (1*PA_USEC_PER_SEC)
#define DEFAULT_ADJUST_TOLERANCE (5*PA_USEC_PER_MSEC)
#define DEFAULT_SAVE_AEC false
#define DEFAULT_AUTOLOADED false

#define MEMBLOCKQ_MAXLENGTH (16*1024*1024)

/* Can only be used in main context */
#define IS_ACTIVE(u) ((pa_source_get_state((u)->source) == PA_SOURCE_RUNNING) && \
                      (pa_sink_get_state((u)->sink) == PA_SINK_RUNNING))

/* This module creates a new (virtual) source and sink.
 *
 * The data sent to the new sink is kept in a memblockq before being
 * forwarded to the real sink_master.
 *
 * Data read from source_master is matched against the saved sink data and
 * echo canceled data is then pushed onto the new source.
 *
 * Both source and sink masters have their own threads to push/pull data
 * respectively. We however perform all our actions in the source IO thread.
 * To do this we send all played samples to the source IO thread where they
 * are then pushed into the memblockq.
 *
 * Alignment is performed in two steps:
 *
 * 1) when something happens that requires quick adjustment of the alignment of
 *    capture and playback samples, we perform a resync. This adjusts the
 *    position in the playback memblock to the requested sample. Quick
 *    adjustments include moving the playback samples before the capture
 *    samples (because else the echo canceller does not work) or when the
 *    playback pointer drifts too far away.
 *
 * 2) periodically check the difference between capture and playback. We use a
 *    low and high watermark for adjusting the alignment. Playback should always
 *    be before capture and the difference should not be bigger than one frame
 *    size. We would ideally like to resample the sink_input but most driver
 *    don't give enough accuracy to be able to do that right now.
 */

struct userdata;

struct pa_echo_canceller_msg {
    pa_msgobject parent;
    struct userdata *userdata;
};

PA_DEFINE_PRIVATE_CLASS(pa_echo_canceller_msg, pa_msgobject);
#define PA_ECHO_CANCELLER_MSG(o) (pa_echo_canceller_msg_cast(o))

struct snapshot {
    pa_usec_t sink_now;
    pa_usec_t sink_latency;
    size_t sink_delay;
    int64_t send_counter;

    pa_usec_t source_now;
    pa_usec_t source_latency;
    size_t source_delay;
    int64_t recv_counter;
    size_t rlen;
    size_t plen;
};

struct userdata {
    pa_core *core;
    pa_module *module;

    bool autoloaded;
    bool dead;
    bool save_aec;

    pa_echo_canceller *ec;
    uint32_t source_output_blocksize;
    uint32_t source_blocksize;
    uint32_t sink_blocksize;

    bool need_realign;

    /* to wakeup the source I/O thread */
    pa_asyncmsgq *asyncmsgq;
    pa_rtpoll_item *rtpoll_item_read, *rtpoll_item_write;

    pa_source *source;
    bool source_auto_desc;
    pa_source_output *source_output;
    pa_memblockq *source_memblockq; /* echo canceller needs fixed sized chunks */
    size_t source_skip;

    pa_sink *sink;
    bool sink_auto_desc;
    pa_sink_input *sink_input;
    pa_memblockq *sink_memblockq;
    int64_t send_counter;          /* updated in sink IO thread */
    int64_t recv_counter;
    size_t sink_skip;

    /* Bytes left over from previous iteration */
    size_t sink_rem;
    size_t source_rem;

    pa_atomic_t request_resync;

    pa_time_event *time_event;
    pa_usec_t adjust_time;
    int adjust_threshold;

    FILE *captured_file;
    FILE *played_file;
    FILE *canceled_file;
    FILE *drift_file;

    bool use_volume_sharing;

    struct {
        pa_cvolume current_volume;
    } thread_info;
};

static void source_output_snapshot_within_thread(struct userdata *u, struct snapshot *snapshot);

static const char* const valid_modargs[] = {
    "source_name",
    "source_properties",
    "source_master",
    "sink_name",
    "sink_properties",
    "sink_master",
    "adjust_time",
    "adjust_threshold",
    "format",
    "rate",
    "channels",
    "channel_map",
    "aec_method",
    "aec_args",
    "save_aec",
    "autoloaded",
    "use_volume_sharing",
    NULL
};

enum {
    SOURCE_OUTPUT_MESSAGE_POST = PA_SOURCE_OUTPUT_MESSAGE_MAX,
    SOURCE_OUTPUT_MESSAGE_REWIND,
    SOURCE_OUTPUT_MESSAGE_LATENCY_SNAPSHOT,
    SOURCE_OUTPUT_MESSAGE_APPLY_DIFF_TIME
};

enum {
    SINK_INPUT_MESSAGE_LATENCY_SNAPSHOT
};

enum {
    ECHO_CANCELLER_MESSAGE_SET_VOLUME,
};

static int64_t calc_diff(struct userdata *u, struct snapshot *snapshot) {
    int64_t diff_time, buffer_latency;
    pa_usec_t plen, rlen, source_delay, sink_delay, recv_counter, send_counter;

    /* get latency difference between playback and record */
    plen = pa_bytes_to_usec(snapshot->plen, &u->sink_input->sample_spec);
    rlen = pa_bytes_to_usec(snapshot->rlen, &u->source_output->sample_spec);
    if (plen > rlen)
        buffer_latency = plen - rlen;
    else
        buffer_latency = 0;

    source_delay = pa_bytes_to_usec(snapshot->source_delay, &u->source_output->sample_spec);
    sink_delay = pa_bytes_to_usec(snapshot->sink_delay, &u->sink_input->sample_spec);
    buffer_latency += source_delay + sink_delay;

    /* add the latency difference due to samples not yet transferred */
    send_counter = pa_bytes_to_usec(snapshot->send_counter, &u->sink->sample_spec);
    recv_counter = pa_bytes_to_usec(snapshot->recv_counter, &u->sink->sample_spec);
    if (recv_counter <= send_counter)
        buffer_latency += (int64_t) (send_counter - recv_counter);
    else
        buffer_latency += PA_CLIP_SUB(buffer_latency, (int64_t) (recv_counter - send_counter));

    /* capture and playback are perfectly aligned when diff_time is 0 */
    diff_time = (snapshot->sink_now + snapshot->sink_latency - buffer_latency) -
          (snapshot->source_now - snapshot->source_latency);

    pa_log_debug("Diff %lld (%lld - %lld + %lld) %lld %lld %lld %lld", (long long) diff_time,
        (long long) snapshot->sink_latency,
        (long long) buffer_latency, (long long) snapshot->source_latency,
        (long long) source_delay, (long long) sink_delay,
        (long long) (send_counter - recv_counter),
        (long long) (snapshot->sink_now - snapshot->source_now));

    return diff_time;
}

/* Called from main context */
static void time_callback(pa_mainloop_api *a, pa_time_event *e, const struct timeval *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t old_rate, base_rate, new_rate;
    int64_t diff_time;
    /*size_t fs*/
    struct snapshot latency_snapshot;

    pa_assert(u);
    pa_assert(a);
    pa_assert(u->time_event == e);
    pa_assert_ctl_context();

    if (!IS_ACTIVE(u))
        return;

    /* update our snapshots */
    pa_asyncmsgq_send(u->source_output->source->asyncmsgq, PA_MSGOBJECT(u->source_output), SOURCE_OUTPUT_MESSAGE_LATENCY_SNAPSHOT, &latency_snapshot, 0, NULL);
    pa_asyncmsgq_send(u->sink_input->sink->asyncmsgq, PA_MSGOBJECT(u->sink_input), SINK_INPUT_MESSAGE_LATENCY_SNAPSHOT, &latency_snapshot, 0, NULL);

    /* calculate drift between capture and playback */
    diff_time = calc_diff(u, &latency_snapshot);

    /*fs = pa_frame_size(&u->source_output->sample_spec);*/
    old_rate = u->sink_input->sample_spec.rate;
    base_rate = u->source_output->sample_spec.rate;

    if (diff_time < 0) {
        /* recording before playback, we need to adjust quickly. The echo
         * canceller does not work in this case. */
        pa_asyncmsgq_post(u->asyncmsgq, PA_MSGOBJECT(u->source_output), SOURCE_OUTPUT_MESSAGE_APPLY_DIFF_TIME,
            NULL, diff_time, NULL, NULL);
        /*new_rate = base_rate - ((pa_usec_to_bytes(-diff_time, &u->source_output->sample_spec) / fs) * PA_USEC_PER_SEC) / u->adjust_time;*/
        new_rate = base_rate;
    }
    else {
        if (diff_time > u->adjust_threshold) {
            /* diff too big, quickly adjust */
            pa_asyncmsgq_post(u->asyncmsgq, PA_MSGOBJECT(u->source_output), SOURCE_OUTPUT_MESSAGE_APPLY_DIFF_TIME,
                NULL, diff_time, NULL, NULL);
        }

        /* recording behind playback, we need to slowly adjust the rate to match */
        /*new_rate = base_rate + ((pa_usec_to_bytes(diff_time, &u->source_output->sample_spec) / fs) * PA_USEC_PER_SEC) / u->adjust_time;*/

        /* assume equal samplerates for now */
        new_rate = base_rate;
    }

    /* make sure we don't make too big adjustments because that sounds horrible */
    if (new_rate > base_rate * 1.1 || new_rate < base_rate * 0.9)
        new_rate = base_rate;

    if (new_rate != old_rate) {
        pa_log_info("Old rate %lu Hz, new rate %lu Hz", (unsigned long) old_rate, (unsigned long) new_rate);

        pa_sink_input_set_rate(u->sink_input, new_rate);
    }

    pa_core_rttime_restart(u->core, u->time_event, pa_rtclock_now() + u->adjust_time);
}

/* Called from source I/O thread context */
static int source_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {

        case PA_SOURCE_MESSAGE_GET_LATENCY:

            /* The source is _put() before the source output is, so let's
             * make sure we don't access it in that time. Also, the
             * source output is first shut down, the source second. */
            if (!PA_SOURCE_IS_LINKED(u->source->thread_info.state) ||
                !PA_SOURCE_OUTPUT_IS_LINKED(u->source_output->thread_info.state)) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            *((pa_usec_t*) data) =

                /* Get the latency of the master source */
                pa_source_get_latency_within_thread(u->source_output->source) +
                /* Add the latency internal to our source output on top */
                pa_bytes_to_usec(pa_memblockq_get_length(u->source_output->thread_info.delay_memblockq), &u->source_output->source->sample_spec) +
                /* and the buffering we do on the source */
                pa_bytes_to_usec(u->source_output_blocksize, &u->source_output->source->sample_spec);

            return 0;

        case PA_SOURCE_MESSAGE_SET_VOLUME_SYNCED:
            u->thread_info.current_volume = u->source->reference_volume;
            break;
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

/* Called from sink I/O thread context */
static int sink_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY:

            /* The sink is _put() before the sink input is, so let's
             * make sure we don't access it in that time. Also, the
             * sink input is first shut down, the sink second. */
            if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
                !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            *((pa_usec_t*) data) =

                /* Get the latency of the master sink */
                pa_sink_get_latency_within_thread(u->sink_input->sink) +

                /* Add the latency internal to our sink input on top */
                pa_bytes_to_usec(pa_memblockq_get_length(u->sink_input->thread_info.render_memblockq), &u->sink_input->sink->sample_spec);

            return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int source_set_state_cb(pa_source *s, pa_source_state_t state) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(state) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return 0;

    if (state == PA_SOURCE_RUNNING) {
        /* restart timer when both sink and source are active */
        if ((pa_sink_get_state(u->sink) == PA_SINK_RUNNING) && u->adjust_time)
            pa_core_rttime_restart(u->core, u->time_event, pa_rtclock_now() + u->adjust_time);

        pa_atomic_store(&u->request_resync, 1);
        pa_source_output_cork(u->source_output, false);
    } else if (state == PA_SOURCE_SUSPENDED) {
        pa_source_output_cork(u->source_output, true);
    }

    return 0;
}

/* Called from main context */
static int sink_set_state_cb(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return 0;

    if (state == PA_SINK_RUNNING) {
        /* restart timer when both sink and source are active */
        if ((pa_source_get_state(u->source) == PA_SOURCE_RUNNING) && u->adjust_time)
            pa_core_rttime_restart(u->core, u->time_event, pa_rtclock_now() + u->adjust_time);

        pa_atomic_store(&u->request_resync, 1);
        pa_sink_input_cork(u->sink_input, false);
    } else if (state == PA_SINK_SUSPENDED) {
        pa_sink_input_cork(u->sink_input, true);
    }

    return 0;
}

/* Called from source I/O thread context */
static void source_update_requested_latency_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(u->source->thread_info.state) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(u->source_output->thread_info.state))
        return;

    pa_log_debug("Source update requested latency");

    /* Just hand this one over to the master source */
    pa_source_output_set_requested_latency_within_thread(
            u->source_output,
            pa_source_get_requested_latency_within_thread(s));
}

/* Called from sink I/O thread context */
static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    pa_log_debug("Sink update requested latency");

    /* Just hand this one over to the master sink */
    pa_sink_input_set_requested_latency_within_thread(
            u->sink_input,
            pa_sink_get_requested_latency_within_thread(s));
}

/* Called from sink I/O thread context */
static void sink_request_rewind_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    pa_log_debug("Sink request rewind %lld", (long long) s->thread_info.rewind_nbytes);

    /* Just hand this one over to the master sink */
    pa_sink_input_request_rewind(u->sink_input,
                                 s->thread_info.rewind_nbytes, true, false, false);
}

/* Called from main context */
static void source_set_volume_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(pa_source_get_state(s)) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return;

    pa_source_output_set_volume(u->source_output, &s->real_volume, s->save_volume, true);
}

/* Called from main context */
static void sink_set_volume_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return;

    pa_sink_input_set_volume(u->sink_input, &s->real_volume, s->save_volume, true);
}

/* Called from main context. */
static void source_get_volume_cb(pa_source *s) {
    struct userdata *u;
    pa_cvolume v;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(pa_source_get_state(s)) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return;

    pa_source_output_get_volume(u->source_output, &v, true);

    if (pa_cvolume_equal(&s->real_volume, &v))
        /* no change */
        return;

    s->real_volume = v;
    pa_source_set_soft_volume(s, NULL);
}

/* Called from main context */
static void source_set_mute_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(pa_source_get_state(s)) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return;

    pa_source_output_set_mute(u->source_output, s->muted, s->save_muted);
}

/* Called from main context */
static void sink_set_mute_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return;

    pa_sink_input_set_mute(u->sink_input, s->muted, s->save_muted);
}

/* Called from source I/O thread context. */
static void apply_diff_time(struct userdata *u, int64_t diff_time) {
    int64_t diff;

    if (diff_time < 0) {
        diff = pa_usec_to_bytes(-diff_time, &u->sink_input->sample_spec);

        if (diff > 0) {
            /* add some extra safety samples to compensate for jitter in the
             * timings */
            diff += 10 * pa_frame_size (&u->sink_input->sample_spec);

            pa_log("Playback after capture (%lld), drop sink %lld", (long long) diff_time, (long long) diff);

            u->sink_skip = diff;
            u->source_skip = 0;
        }
    } else if (diff_time > 0) {
        diff = pa_usec_to_bytes(diff_time, &u->source_output->sample_spec);

        if (diff > 0) {
            pa_log("Playback too far ahead (%lld), drop source %lld", (long long) diff_time, (long long) diff);

            u->source_skip = diff;
            u->sink_skip = 0;
        }
    }
}

/* Called from source I/O thread context. */
static void do_resync(struct userdata *u) {
    int64_t diff_time;
    struct snapshot latency_snapshot;

    pa_log("Doing resync");

    /* update our snapshot */
    source_output_snapshot_within_thread(u, &latency_snapshot);
    pa_asyncmsgq_send(u->sink_input->sink->asyncmsgq, PA_MSGOBJECT(u->sink_input), SINK_INPUT_MESSAGE_LATENCY_SNAPSHOT, &latency_snapshot, 0, NULL);

    /* calculate drift between capture and playback */
    diff_time = calc_diff(u, &latency_snapshot);

    /* and adjust for the drift */
    apply_diff_time(u, diff_time);
}

/* 1. Calculate drift at this point, pass to canceller
 * 2. Push out playback samples in blocksize chunks
 * 3. Push out capture samples in blocksize chunks
 * 4. ???
 * 5. Profit
 *
 * Called from source I/O thread context.
 */
static void do_push_drift_comp(struct userdata *u) {
    size_t rlen, plen;
    pa_memchunk rchunk, pchunk, cchunk;
    uint8_t *rdata, *pdata, *cdata;
    float drift;
    int unused PA_GCC_UNUSED;

    rlen = pa_memblockq_get_length(u->source_memblockq);
    plen = pa_memblockq_get_length(u->sink_memblockq);

    /* Estimate snapshot drift as follows:
     *   pd: amount of data consumed since last time
     *   rd: amount of data consumed since last time
     *
     *   drift = (pd - rd) / rd;
     *
     * We calculate pd and rd as the memblockq length less the number of
     * samples left from the last iteration (to avoid double counting
     * those remainder samples.
     */
    drift = ((float)(plen - u->sink_rem) - (rlen - u->source_rem)) / ((float)(rlen - u->source_rem));
    u->sink_rem = plen % u->sink_blocksize;
    u->source_rem = rlen % u->source_output_blocksize;

    /* Now let the canceller work its drift compensation magic */
    u->ec->set_drift(u->ec, drift);

    if (u->save_aec) {
        if (u->drift_file)
            fprintf(u->drift_file, "d %a\n", drift);
    }

    /* Send in the playback samples first */
    while (plen >= u->sink_blocksize) {
        pa_memblockq_peek_fixed_size(u->sink_memblockq, u->sink_blocksize, &pchunk);
        pdata = pa_memblock_acquire(pchunk.memblock);
        pdata += pchunk.index;

        u->ec->play(u->ec, pdata);

        if (u->save_aec) {
            if (u->drift_file)
                fprintf(u->drift_file, "p %d\n", u->sink_blocksize);
            if (u->played_file)
                unused = fwrite(pdata, 1, u->sink_blocksize, u->played_file);
        }

        pa_memblock_release(pchunk.memblock);
        pa_memblockq_drop(u->sink_memblockq, u->sink_blocksize);
        pa_memblock_unref(pchunk.memblock);

        plen -= u->sink_blocksize;
    }

    /* And now the capture samples */
    while (rlen >= u->source_output_blocksize) {
        pa_memblockq_peek_fixed_size(u->source_memblockq, u->source_output_blocksize, &rchunk);

        rdata = pa_memblock_acquire(rchunk.memblock);
        rdata += rchunk.index;

        cchunk.index = 0;
        cchunk.length = u->source_output_blocksize;
        cchunk.memblock = pa_memblock_new(u->source->core->mempool, cchunk.length);
        cdata = pa_memblock_acquire(cchunk.memblock);

        u->ec->record(u->ec, rdata, cdata);

        if (u->save_aec) {
            if (u->drift_file)
                fprintf(u->drift_file, "c %d\n", u->source_output_blocksize);
            if (u->captured_file)
                unused = fwrite(rdata, 1, u->source_output_blocksize, u->captured_file);
            if (u->canceled_file)
                unused = fwrite(cdata, 1, u->source_output_blocksize, u->canceled_file);
        }

        pa_memblock_release(cchunk.memblock);
        pa_memblock_release(rchunk.memblock);

        pa_memblock_unref(rchunk.memblock);

        pa_source_post(u->source, &cchunk);
        pa_memblock_unref(cchunk.memblock);

        pa_memblockq_drop(u->source_memblockq, u->source_output_blocksize);
        rlen -= u->source_output_blocksize;
    }
}

/* This one's simpler than the drift compensation case -- we just iterate over
 * the capture buffer, and pass the canceller blocksize bytes of playback and
 * capture data.
 *
 * Called from source I/O thread context. */
static void do_push(struct userdata *u) {
    size_t rlen, plen;
    pa_memchunk rchunk, pchunk, cchunk;
    uint8_t *rdata, *pdata, *cdata;
    int unused PA_GCC_UNUSED;

    rlen = pa_memblockq_get_length(u->source_memblockq);
    plen = pa_memblockq_get_length(u->sink_memblockq);

    while (rlen >= u->source_output_blocksize) {

        /* take fixed blocks from recorded and played samples */
        pa_memblockq_peek_fixed_size(u->source_memblockq, u->source_output_blocksize, &rchunk);
        pa_memblockq_peek_fixed_size(u->sink_memblockq, u->sink_blocksize, &pchunk);

        /* we ran out of played data and pchunk has been filled with silence bytes */
        if (plen < u->sink_blocksize)
            pa_memblockq_seek(u->sink_memblockq, u->sink_blocksize - plen, PA_SEEK_RELATIVE, true);

        rdata = pa_memblock_acquire(rchunk.memblock);
        rdata += rchunk.index;
        pdata = pa_memblock_acquire(pchunk.memblock);
        pdata += pchunk.index;

        cchunk.index = 0;
        cchunk.length = u->source_blocksize;
        cchunk.memblock = pa_memblock_new(u->source->core->mempool, cchunk.length);
        cdata = pa_memblock_acquire(cchunk.memblock);

        if (u->save_aec) {
            if (u->captured_file)
                unused = fwrite(rdata, 1, u->source_output_blocksize, u->captured_file);
            if (u->played_file)
                unused = fwrite(pdata, 1, u->sink_blocksize, u->played_file);
        }

        /* perform echo cancellation */
        u->ec->run(u->ec, rdata, pdata, cdata);

        if (u->save_aec) {
            if (u->canceled_file)
                unused = fwrite(cdata, 1, u->source_blocksize, u->canceled_file);
        }

        pa_memblock_release(cchunk.memblock);
        pa_memblock_release(pchunk.memblock);
        pa_memblock_release(rchunk.memblock);

        /* drop consumed source samples */
        pa_memblockq_drop(u->source_memblockq, u->source_output_blocksize);
        pa_memblock_unref(rchunk.memblock);
        rlen -= u->source_output_blocksize;

        /* drop consumed sink samples */
        pa_memblockq_drop(u->sink_memblockq, u->sink_blocksize);
        pa_memblock_unref(pchunk.memblock);

        if (plen >= u->sink_blocksize)
            plen -= u->sink_blocksize;
        else
            plen = 0;

        /* forward the (echo-canceled) data to the virtual source */
        pa_source_post(u->source, &cchunk);
        pa_memblock_unref(cchunk.memblock);
    }
}

/* Called from source I/O thread context. */
static void source_output_push_cb(pa_source_output *o, const pa_memchunk *chunk) {
    struct userdata *u;
    size_t rlen, plen, to_skip;
    pa_memchunk rchunk;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output))) {
        pa_log("Push when no link?");
        return;
    }

    if (PA_UNLIKELY(u->source->thread_info.state != PA_SOURCE_RUNNING ||
                    u->sink->thread_info.state != PA_SINK_RUNNING)) {
        pa_source_post(u->source, chunk);
        return;
    }

    /* handle queued messages, do any message sending of our own */
    while (pa_asyncmsgq_process_one(u->asyncmsgq) > 0)
        ;

    pa_memblockq_push_align(u->source_memblockq, chunk);

    rlen = pa_memblockq_get_length(u->source_memblockq);
    plen = pa_memblockq_get_length(u->sink_memblockq);

    /* Let's not do anything else till we have enough data to process */
    if (rlen < u->source_output_blocksize)
        return;

    /* See if we need to drop samples in order to sync */
    if (pa_atomic_cmpxchg (&u->request_resync, 1, 0)) {
        do_resync(u);
    }

    /* Okay, skip cancellation for skipped source samples if needed. */
    if (PA_UNLIKELY(u->source_skip)) {
        /* The slightly tricky bit here is that we drop all but modulo
         * blocksize bytes and then adjust for that last bit on the sink side.
         * We do this because the source data is coming at a fixed rate, which
         * means the only way to try to catch up is drop sink samples and let
         * the canceller cope up with this. */
        to_skip = rlen >= u->source_skip ? u->source_skip : rlen;
        to_skip -= to_skip % u->source_output_blocksize;

        if (to_skip) {
            pa_memblockq_peek_fixed_size(u->source_memblockq, to_skip, &rchunk);
            pa_source_post(u->source, &rchunk);

            pa_memblock_unref(rchunk.memblock);
            pa_memblockq_drop(u->source_memblockq, to_skip);

            rlen -= to_skip;
            u->source_skip -= to_skip;
        }

        if (rlen && u->source_skip % u->source_output_blocksize) {
            u->sink_skip += (uint64_t) (u->source_output_blocksize - (u->source_skip % u->source_output_blocksize)) * u->sink_blocksize / u->source_output_blocksize;
            u->source_skip -= (u->source_skip % u->source_output_blocksize);
        }
    }

    /* And for the sink, these samples have been played back already, so we can
     * just drop them and get on with it. */
    if (PA_UNLIKELY(u->sink_skip)) {
        to_skip = plen >= u->sink_skip ? u->sink_skip : plen;

        pa_memblockq_drop(u->sink_memblockq, to_skip);

        plen -= to_skip;
        u->sink_skip -= to_skip;
    }

    /* process and push out samples */
    if (u->ec->params.drift_compensation)
        do_push_drift_comp(u);
    else
        do_push(u);
}

/* Called from sink I/O thread context. */
static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert(chunk);
    pa_assert_se(u = i->userdata);

    if (u->sink->thread_info.rewind_requested)
        pa_sink_process_rewind(u->sink, 0);

    pa_sink_render_full(u->sink, nbytes, chunk);

    if (i->thread_info.underrun_for > 0) {
        pa_log_debug("Handling end of underrun.");
        pa_atomic_store(&u->request_resync, 1);
    }

    /* let source thread handle the chunk. pass the sample count as well so that
     * the source IO thread can update the right variables. */
    pa_asyncmsgq_post(u->asyncmsgq, PA_MSGOBJECT(u->source_output), SOURCE_OUTPUT_MESSAGE_POST,
        NULL, 0, chunk, NULL);
    u->send_counter += chunk->length;

    return 0;
}

/* Called from source I/O thread context. */
static void source_output_process_rewind_cb(pa_source_output *o, size_t nbytes) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_process_rewind(u->source, nbytes);

    /* go back on read side, we need to use older sink data for this */
    pa_memblockq_rewind(u->sink_memblockq, nbytes);

    /* manipulate write index */
    pa_memblockq_seek(u->source_memblockq, -nbytes, PA_SEEK_RELATIVE, true);

    pa_log_debug("Source rewind (%lld) %lld", (long long) nbytes,
        (long long) pa_memblockq_get_length (u->source_memblockq));
}

/* Called from sink I/O thread context. */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink process rewind %lld", (long long) nbytes);

    pa_sink_process_rewind(u->sink, nbytes);

    pa_asyncmsgq_post(u->asyncmsgq, PA_MSGOBJECT(u->source_output), SOURCE_OUTPUT_MESSAGE_REWIND, NULL, (int64_t) nbytes, NULL, NULL);
    u->send_counter -= nbytes;
}

/* Called from source I/O thread context. */
static void source_output_snapshot_within_thread(struct userdata *u, struct snapshot *snapshot) {
    size_t delay, rlen, plen;
    pa_usec_t now, latency;

    now = pa_rtclock_now();
    latency = pa_source_get_latency_within_thread(u->source_output->source);
    delay = pa_memblockq_get_length(u->source_output->thread_info.delay_memblockq);

    delay = (u->source_output->thread_info.resampler ? pa_resampler_request(u->source_output->thread_info.resampler, delay) : delay);
    rlen = pa_memblockq_get_length(u->source_memblockq);
    plen = pa_memblockq_get_length(u->sink_memblockq);

    snapshot->source_now = now;
    snapshot->source_latency = latency;
    snapshot->source_delay = delay;
    snapshot->recv_counter = u->recv_counter;
    snapshot->rlen = rlen + u->sink_skip;
    snapshot->plen = plen + u->source_skip;
}

/* Called from source I/O thread context. */
static int source_output_process_msg_cb(pa_msgobject *obj, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE_OUTPUT(obj)->userdata;

    switch (code) {

        case SOURCE_OUTPUT_MESSAGE_POST:

            pa_source_output_assert_io_context(u->source_output);

            if (u->source_output->source->thread_info.state == PA_SOURCE_RUNNING)
                pa_memblockq_push_align(u->sink_memblockq, chunk);
            else
                pa_memblockq_flush_write(u->sink_memblockq, true);

            u->recv_counter += (int64_t) chunk->length;

            return 0;

        case SOURCE_OUTPUT_MESSAGE_REWIND:
            pa_source_output_assert_io_context(u->source_output);

            /* manipulate write index, never go past what we have */
            if (PA_SOURCE_IS_OPENED(u->source_output->source->thread_info.state))
                pa_memblockq_seek(u->sink_memblockq, -offset, PA_SEEK_RELATIVE, true);
            else
                pa_memblockq_flush_write(u->sink_memblockq, true);

            pa_log_debug("Sink rewind (%lld)", (long long) offset);

            u->recv_counter -= offset;

            return 0;

        case SOURCE_OUTPUT_MESSAGE_LATENCY_SNAPSHOT: {
            struct snapshot *snapshot = (struct snapshot *) data;

            source_output_snapshot_within_thread(u, snapshot);
            return 0;
        }

        case SOURCE_OUTPUT_MESSAGE_APPLY_DIFF_TIME:
            apply_diff_time(u, offset);
            return 0;

    }

    return pa_source_output_process_msg(obj, code, data, offset, chunk);
}

/* Called from sink I/O thread context. */
static int sink_input_process_msg_cb(pa_msgobject *obj, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK_INPUT(obj)->userdata;

    switch (code) {

        case SINK_INPUT_MESSAGE_LATENCY_SNAPSHOT: {
            size_t delay;
            pa_usec_t now, latency;
            struct snapshot *snapshot = (struct snapshot *) data;

            pa_sink_input_assert_io_context(u->sink_input);

            now = pa_rtclock_now();
            latency = pa_sink_get_latency_within_thread(u->sink_input->sink);
            delay = pa_memblockq_get_length(u->sink_input->thread_info.render_memblockq);

            delay = (u->sink_input->thread_info.resampler ? pa_resampler_request(u->sink_input->thread_info.resampler, delay) : delay);

            snapshot->sink_now = now;
            snapshot->sink_latency = latency;
            snapshot->sink_delay = delay;
            snapshot->send_counter = u->send_counter;
            return 0;
        }
    }

    return pa_sink_input_process_msg(obj, code, data, offset, chunk);
}

/* Called from sink I/O thread context. */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink input update max rewind %lld", (long long) nbytes);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_memblockq_set_maxrewind(u->sink_memblockq, nbytes);
    pa_sink_set_max_rewind_within_thread(u->sink, nbytes);
}

/* Called from source I/O thread context. */
static void source_output_update_max_rewind_cb(pa_source_output *o, size_t nbytes) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_se(u = o->userdata);

    pa_log_debug("Source output update max rewind %lld", (long long) nbytes);

    pa_source_set_max_rewind_within_thread(u->source, nbytes);
}

/* Called from sink I/O thread context. */
static void sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink input update max request %lld", (long long) nbytes);

    pa_sink_set_max_request_within_thread(u->sink, nbytes);
}

/* Called from sink I/O thread context. */
static void sink_input_update_sink_requested_latency_cb(pa_sink_input *i) {
    struct userdata *u;
    pa_usec_t latency;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    latency = pa_sink_get_requested_latency_within_thread(i->sink);

    pa_log_debug("Sink input update requested latency %lld", (long long) latency);
}

/* Called from source I/O thread context. */
static void source_output_update_source_requested_latency_cb(pa_source_output *o) {
    struct userdata *u;
    pa_usec_t latency;

    pa_source_output_assert_ref(o);
    pa_assert_se(u = o->userdata);

    latency = pa_source_get_requested_latency_within_thread(o->source);

    pa_log_debug("Source output update requested latency %lld", (long long) latency);
}

/* Called from sink I/O thread context. */
static void sink_input_update_sink_latency_range_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink input update latency range %lld %lld",
        (long long) i->sink->thread_info.min_latency,
        (long long) i->sink->thread_info.max_latency);

    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
}

/* Called from source I/O thread context. */
static void source_output_update_source_latency_range_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_se(u = o->userdata);

    pa_log_debug("Source output update latency range %lld %lld",
        (long long) o->source->thread_info.min_latency,
        (long long) o->source->thread_info.max_latency);

    pa_source_set_latency_range_within_thread(u->source, o->source->thread_info.min_latency, o->source->thread_info.max_latency);
}

/* Called from sink I/O thread context. */
static void sink_input_update_sink_fixed_latency_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink input update fixed latency %lld",
        (long long) i->sink->thread_info.fixed_latency);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
}

/* Called from source I/O thread context. */
static void source_output_update_source_fixed_latency_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_se(u = o->userdata);

    pa_log_debug("Source output update fixed latency %lld",
        (long long) o->source->thread_info.fixed_latency);

    pa_source_set_fixed_latency_within_thread(u->source, o->source->thread_info.fixed_latency);
}

/* Called from source I/O thread context. */
static void source_output_attach_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_set_rtpoll(u->source, o->source->thread_info.rtpoll);
    pa_source_set_latency_range_within_thread(u->source, o->source->thread_info.min_latency, o->source->thread_info.max_latency);
    pa_source_set_fixed_latency_within_thread(u->source, o->source->thread_info.fixed_latency);
    pa_source_set_max_rewind_within_thread(u->source, pa_source_output_get_max_rewind(o));

    pa_log_debug("Source output %d attach", o->index);

    pa_source_attach_within_thread(u->source);

    u->rtpoll_item_read = pa_rtpoll_item_new_asyncmsgq_read(
            o->source->thread_info.rtpoll,
            PA_RTPOLL_LATE,
            u->asyncmsgq);
}

/* Called from sink I/O thread context. */
static void sink_input_attach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_rtpoll(u->sink, i->sink->thread_info.rtpoll);
    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);

    /* (8.1) IF YOU NEED A FIXED BLOCK SIZE ADD THE LATENCY FOR ONE
     * BLOCK MINUS ONE SAMPLE HERE. SEE (7) */
    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);

    /* (8.2) IF YOU NEED A FIXED BLOCK SIZE ROUND
     * pa_sink_input_get_max_request(i) UP TO MULTIPLES OF IT
     * HERE. SEE (6) */
    pa_sink_set_max_request_within_thread(u->sink, pa_sink_input_get_max_request(i));

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i));

    pa_log_debug("Sink input %d attach", i->index);

    u->rtpoll_item_write = pa_rtpoll_item_new_asyncmsgq_write(
            i->sink->thread_info.rtpoll,
            PA_RTPOLL_LATE,
            u->asyncmsgq);

    pa_sink_attach_within_thread(u->sink);
}

/* Called from source I/O thread context. */
static void source_output_detach_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_detach_within_thread(u->source);
    pa_source_set_rtpoll(u->source, NULL);

    pa_log_debug("Source output %d detach", o->index);

    if (u->rtpoll_item_read) {
        pa_rtpoll_item_free(u->rtpoll_item_read);
        u->rtpoll_item_read = NULL;
    }
}

/* Called from sink I/O thread context. */
static void sink_input_detach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_detach_within_thread(u->sink);

    pa_sink_set_rtpoll(u->sink, NULL);

    pa_log_debug("Sink input %d detach", i->index);

    if (u->rtpoll_item_write) {
        pa_rtpoll_item_free(u->rtpoll_item_write);
        u->rtpoll_item_write = NULL;
    }
}

/* Called from source I/O thread context. */
static void source_output_state_change_cb(pa_source_output *o, pa_source_output_state_t state) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_log_debug("Source output %d state %d", o->index, state);
}

/* Called from sink I/O thread context. */
static void sink_input_state_change_cb(pa_sink_input *i, pa_sink_input_state_t state) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Sink input %d state %d", i->index, state);

    /* If we are added for the first time, ask for a rewinding so that
     * we are heard right-away. */
    if (PA_SINK_INPUT_IS_LINKED(state) &&
        i->thread_info.state == PA_SINK_INPUT_INIT) {
        pa_log_debug("Requesting rewind due to state change.");
        pa_sink_input_request_rewind(i, 0, false, true, true);
    }
}

/* Called from main context. */
static void source_output_kill_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_ctl_context();
    pa_assert_se(u = o->userdata);

    u->dead = true;

    /* The order here matters! We first kill the source output, followed
     * by the source. That means the source callbacks must be protected
     * against an unconnected source output! */
    pa_source_output_unlink(u->source_output);
    pa_source_unlink(u->source);

    pa_source_output_unref(u->source_output);
    u->source_output = NULL;

    pa_source_unref(u->source);
    u->source = NULL;

    pa_log_debug("Source output kill %d", o->index);

    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    u->dead = true;

    /* The order here matters! We first kill the sink input, followed
     * by the sink. That means the sink callbacks must be protected
     * against an unconnected sink input! */
    pa_sink_input_unlink(u->sink_input);
    pa_sink_unlink(u->sink);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    pa_sink_unref(u->sink);
    u->sink = NULL;

    pa_log_debug("Sink input kill %d", i->index);

    pa_module_unload_request(u->module, true);
}

/* Called from main context. */
static bool source_output_may_move_to_cb(pa_source_output *o, pa_source *dest) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_ctl_context();
    pa_assert_se(u = o->userdata);

    if (u->dead || u->autoloaded)
        return false;

    return (u->source != dest) && (u->sink != dest->monitor_of);
}

/* Called from main context */
static bool sink_input_may_move_to_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->dead || u->autoloaded)
        return false;

    return u->sink != dest;
}

/* Called from main context. */
static void source_output_moving_cb(pa_source_output *o, pa_source *dest) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_ctl_context();
    pa_assert_se(u = o->userdata);

    if (dest) {
        pa_source_set_asyncmsgq(u->source, dest->asyncmsgq);
        pa_source_update_flags(u->source, PA_SOURCE_LATENCY|PA_SOURCE_DYNAMIC_LATENCY, dest->flags);
    } else
        pa_source_set_asyncmsgq(u->source, NULL);

    if (u->source_auto_desc && dest) {
        const char *y, *z;
        pa_proplist *pl;

        pl = pa_proplist_new();
        y = pa_proplist_gets(u->sink_input->sink->proplist, PA_PROP_DEVICE_DESCRIPTION);
        z = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "%s (echo cancelled with %s)", z ? z : dest->name,
                y ? y : u->sink_input->sink->name);

        pa_source_update_proplist(u->source, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

/* Called from main context */
static void sink_input_moving_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (dest) {
        pa_sink_set_asyncmsgq(u->sink, dest->asyncmsgq);
        pa_sink_update_flags(u->sink, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY, dest->flags);
    } else
        pa_sink_set_asyncmsgq(u->sink, NULL);

    if (u->sink_auto_desc && dest) {
        const char *y, *z;
        pa_proplist *pl;

        pl = pa_proplist_new();
        y = pa_proplist_gets(u->source_output->source->proplist, PA_PROP_DEVICE_DESCRIPTION);
        z = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "%s (echo cancelled with %s)", z ? z : dest->name,
                         y ? y : u->source_output->source->name);

        pa_sink_update_proplist(u->sink, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

/* Called from main context */
static void sink_input_volume_changed_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_volume_changed(u->sink, &i->volume);
}

/* Called from main context */
static void sink_input_mute_changed_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_mute_changed(u->sink, i->muted);
}

/* Called from main context */
static int canceller_process_msg_cb(pa_msgobject *o, int code, void *userdata, int64_t offset, pa_memchunk *chunk) {
    struct pa_echo_canceller_msg *msg;
    struct userdata *u;

    pa_assert(o);

    msg = PA_ECHO_CANCELLER_MSG(o);
    u = msg->userdata;

    switch (code) {
        case ECHO_CANCELLER_MESSAGE_SET_VOLUME: {
            pa_cvolume *v = (pa_cvolume *) userdata;

            if (u->use_volume_sharing)
                pa_source_set_volume(u->source, v, true, false);
            else
                pa_source_output_set_volume(u->source_output, v, false, true);

            break;
        }

        default:
            pa_assert_not_reached();
            break;
    }

    return 0;
}

/* Called by the canceller, so source I/O thread context. */
void pa_echo_canceller_get_capture_volume(pa_echo_canceller *ec, pa_cvolume *v) {
    *v = ec->msg->userdata->thread_info.current_volume;
}

/* Called by the canceller, so source I/O thread context. */
void pa_echo_canceller_set_capture_volume(pa_echo_canceller *ec, pa_cvolume *v) {
    if (!pa_cvolume_equal(&ec->msg->userdata->thread_info.current_volume, v)) {
        pa_cvolume *vol = pa_xnewdup(pa_cvolume, v, 1);

        pa_asyncmsgq_post(pa_thread_mq_get()->outq, PA_MSGOBJECT(ec->msg), ECHO_CANCELLER_MESSAGE_SET_VOLUME, vol, 0, NULL,
                pa_xfree);
    }
}

uint32_t pa_echo_canceller_blocksize_power2(unsigned rate, unsigned ms) {
    unsigned nframes = (rate * ms) / 1000;
    uint32_t y = 1 << ((8 * sizeof(uint32_t)) - 2);

    pa_assert(rate >= 4000);
    pa_assert(ms >= 1);

    /* nframes should be a power of 2, round down to nearest power of two */
    while (y > nframes)
        y >>= 1;

    pa_assert(y >= 1);
    return y;
}

static pa_echo_canceller_method_t get_ec_method_from_string(const char *method) {
    if (pa_streq(method, "null"))
        return PA_ECHO_CANCELLER_NULL;
#ifdef HAVE_SPEEX
    if (pa_streq(method, "speex"))
        return PA_ECHO_CANCELLER_SPEEX;
#endif
#ifdef HAVE_ADRIAN_EC
    if (pa_streq(method, "adrian"))
        return PA_ECHO_CANCELLER_ADRIAN;
#endif
#ifdef HAVE_WEBRTC
    if (pa_streq(method, "webrtc"))
        return PA_ECHO_CANCELLER_WEBRTC;
#endif
    return PA_ECHO_CANCELLER_INVALID;
}

/* Common initialisation bits between module-echo-cancel and the standalone
 * test program.
 *
 * Called from main context. */
static int init_common(pa_modargs *ma, struct userdata *u, pa_sample_spec *source_ss, pa_channel_map *source_map) {
    const char *ec_string;
    pa_echo_canceller_method_t ec_method;

    if (pa_modargs_get_sample_spec_and_channel_map(ma, source_ss, source_map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    u->ec = pa_xnew0(pa_echo_canceller, 1);
    if (!u->ec) {
        pa_log("Failed to alloc echo canceller");
        goto fail;
    }

    ec_string = pa_modargs_get_value(ma, "aec_method", DEFAULT_ECHO_CANCELLER);
    if ((ec_method = get_ec_method_from_string(ec_string)) < 0) {
        pa_log("Invalid echo canceller implementation '%s'", ec_string);
        goto fail;
    }

    pa_log_info("Using AEC engine: %s", ec_string);

    u->ec->init = ec_table[ec_method].init;
    u->ec->play = ec_table[ec_method].play;
    u->ec->record = ec_table[ec_method].record;
    u->ec->set_drift = ec_table[ec_method].set_drift;
    u->ec->run = ec_table[ec_method].run;
    u->ec->done = ec_table[ec_method].done;

    return 0;

fail:
    return -1;
}

/* Called from main context. */
int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec source_output_ss, source_ss, sink_ss;
    pa_channel_map source_output_map, source_map, sink_map;
    pa_modargs *ma;
    pa_source *source_master=NULL;
    pa_sink *sink_master=NULL;
    pa_source_output_new_data source_output_data;
    pa_sink_input_new_data sink_input_data;
    pa_source_new_data source_data;
    pa_sink_new_data sink_data;
    pa_memchunk silence;
    uint32_t temp;
    uint32_t nframes = 0;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(source_master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "source_master", NULL), PA_NAMEREG_SOURCE))) {
        pa_log("Master source not found");
        goto fail;
    }
    pa_assert(source_master);

    if (!(sink_master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "sink_master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }
    pa_assert(sink_master);

    if (source_master->monitor_of == sink_master) {
        pa_log("Can't cancel echo between a sink and its monitor");
        goto fail;
    }

    source_ss = source_master->sample_spec;
    source_ss.rate = DEFAULT_RATE;
    source_ss.channels = DEFAULT_CHANNELS;
    pa_channel_map_init_auto(&source_map, source_ss.channels, PA_CHANNEL_MAP_DEFAULT);

    sink_ss = sink_master->sample_spec;
    sink_map = sink_master->channel_map;

    u = pa_xnew0(struct userdata, 1);
    if (!u) {
        pa_log("Failed to alloc userdata");
        goto fail;
    }
    u->core = m->core;
    u->module = m;
    m->userdata = u;
    u->dead = false;

    u->use_volume_sharing = true;
    if (pa_modargs_get_value_boolean(ma, "use_volume_sharing", &u->use_volume_sharing) < 0) {
        pa_log("use_volume_sharing= expects a boolean argument");
        goto fail;
    }

    temp = DEFAULT_ADJUST_TIME_USEC / PA_USEC_PER_SEC;
    if (pa_modargs_get_value_u32(ma, "adjust_time", &temp) < 0) {
        pa_log("Failed to parse adjust_time value");
        goto fail;
    }

    if (temp != DEFAULT_ADJUST_TIME_USEC / PA_USEC_PER_SEC)
        u->adjust_time = temp * PA_USEC_PER_SEC;
    else
        u->adjust_time = DEFAULT_ADJUST_TIME_USEC;

    temp = DEFAULT_ADJUST_TOLERANCE / PA_USEC_PER_MSEC;
    if (pa_modargs_get_value_u32(ma, "adjust_threshold", &temp) < 0) {
        pa_log("Failed to parse adjust_threshold value");
        goto fail;
    }

    if (temp != DEFAULT_ADJUST_TOLERANCE / PA_USEC_PER_MSEC)
        u->adjust_threshold = temp * PA_USEC_PER_MSEC;
    else
        u->adjust_threshold = DEFAULT_ADJUST_TOLERANCE;

    u->save_aec = DEFAULT_SAVE_AEC;
    if (pa_modargs_get_value_boolean(ma, "save_aec", &u->save_aec) < 0) {
        pa_log("Failed to parse save_aec value");
        goto fail;
    }

    u->autoloaded = DEFAULT_AUTOLOADED;
    if (pa_modargs_get_value_boolean(ma, "autoloaded", &u->autoloaded) < 0) {
        pa_log("Failed to parse autoloaded value");
        goto fail;
    }

    if (init_common(ma, u, &source_ss, &source_map) < 0)
        goto fail;

    u->asyncmsgq = pa_asyncmsgq_new(0);
    u->need_realign = true;

    source_output_ss = source_ss;
    source_output_map = source_map;

    if (sink_ss.rate != source_ss.rate) {
        pa_log_info("Sample rates of play and out stream differ. Adjusting rate of play stream.");
        sink_ss.rate = source_ss.rate;
    }

    pa_assert(u->ec->init);
    if (!u->ec->init(u->core, u->ec, &source_output_ss, &source_output_map, &sink_ss, &sink_map, &source_ss, &source_map, &nframes, pa_modargs_get_value(ma, "aec_args", NULL))) {
        pa_log("Failed to init AEC engine");
        goto fail;
    }

    pa_assert(source_output_ss.rate == source_ss.rate);
    pa_assert(sink_ss.rate == source_ss.rate);

    u->source_output_blocksize = nframes * pa_frame_size(&source_output_ss);
    u->source_blocksize = nframes * pa_frame_size(&source_ss);
    u->sink_blocksize = nframes * pa_frame_size(&sink_ss);

    if (u->ec->params.drift_compensation)
        pa_assert(u->ec->set_drift);

    /* Create source */
    pa_source_new_data_init(&source_data);
    source_data.driver = __FILE__;
    source_data.module = m;
    if (!(source_data.name = pa_xstrdup(pa_modargs_get_value(ma, "source_name", NULL))))
        source_data.name = pa_sprintf_malloc("%s.echo-cancel", source_master->name);
    pa_source_new_data_set_sample_spec(&source_data, &source_ss);
    pa_source_new_data_set_channel_map(&source_data, &source_map);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, source_master->name);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_CLASS, "filter");
    if (!u->autoloaded)
        pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");

    if (pa_modargs_get_proplist(ma, "source_properties", source_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&source_data);
        goto fail;
    }

    if ((u->source_auto_desc = !pa_proplist_contains(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *y, *z;

        y = pa_proplist_gets(sink_master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        z = pa_proplist_gets(source_master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "%s (echo cancelled with %s)",
                z ? z : source_master->name, y ? y : sink_master->name);
    }

    u->source = pa_source_new(m->core, &source_data, (source_master->flags & (PA_SOURCE_LATENCY | PA_SOURCE_DYNAMIC_LATENCY))
                                                     | (u->use_volume_sharing ? PA_SOURCE_SHARE_VOLUME_WITH_MASTER : 0));
    pa_source_new_data_done(&source_data);

    if (!u->source) {
        pa_log("Failed to create source.");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg_cb;
    u->source->set_state = source_set_state_cb;
    u->source->update_requested_latency = source_update_requested_latency_cb;
    pa_source_set_set_mute_callback(u->source, source_set_mute_cb);
    if (!u->use_volume_sharing) {
        pa_source_set_get_volume_callback(u->source, source_get_volume_cb);
        pa_source_set_set_volume_callback(u->source, source_set_volume_cb);
        pa_source_enable_decibel_volume(u->source, true);
    }
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, source_master->asyncmsgq);

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if (!(sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sink_data.name = pa_sprintf_malloc("%s.echo-cancel", sink_master->name);
    pa_sink_new_data_set_sample_spec(&sink_data, &sink_ss);
    pa_sink_new_data_set_channel_map(&sink_data, &sink_map);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, sink_master->name);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "filter");
    if (!u->autoloaded)
        pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    if ((u->sink_auto_desc = !pa_proplist_contains(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *y, *z;

        y = pa_proplist_gets(source_master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        z = pa_proplist_gets(sink_master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "%s (echo cancelled with %s)",
                z ? z : sink_master->name, y ? y : source_master->name);
    }

    u->sink = pa_sink_new(m->core, &sink_data, (sink_master->flags & (PA_SINK_LATENCY | PA_SINK_DYNAMIC_LATENCY))
                                               | (u->use_volume_sharing ? PA_SINK_SHARE_VOLUME_WITH_MASTER : 0));
    pa_sink_new_data_done(&sink_data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg_cb;
    u->sink->set_state = sink_set_state_cb;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    u->sink->request_rewind = sink_request_rewind_cb;
    pa_sink_set_set_mute_callback(u->sink, sink_set_mute_cb);
    if (!u->use_volume_sharing) {
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        pa_sink_enable_decibel_volume(u->sink, true);
    }
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, sink_master->asyncmsgq);

    /* Create source output */
    pa_source_output_new_data_init(&source_output_data);
    source_output_data.driver = __FILE__;
    source_output_data.module = m;
    pa_source_output_new_data_set_source(&source_output_data, source_master, false);
    source_output_data.destination_source = u->source;

    pa_proplist_sets(source_output_data.proplist, PA_PROP_MEDIA_NAME, "Echo-Cancel Source Stream");
    pa_proplist_sets(source_output_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_source_output_new_data_set_sample_spec(&source_output_data, &source_output_ss);
    pa_source_output_new_data_set_channel_map(&source_output_data, &source_output_map);

    pa_source_output_new(&u->source_output, m->core, &source_output_data);
    pa_source_output_new_data_done(&source_output_data);

    if (!u->source_output)
        goto fail;

    u->source_output->parent.process_msg = source_output_process_msg_cb;
    u->source_output->push = source_output_push_cb;
    u->source_output->process_rewind = source_output_process_rewind_cb;
    u->source_output->update_max_rewind = source_output_update_max_rewind_cb;
    u->source_output->update_source_requested_latency = source_output_update_source_requested_latency_cb;
    u->source_output->update_source_latency_range = source_output_update_source_latency_range_cb;
    u->source_output->update_source_fixed_latency = source_output_update_source_fixed_latency_cb;
    u->source_output->kill = source_output_kill_cb;
    u->source_output->attach = source_output_attach_cb;
    u->source_output->detach = source_output_detach_cb;
    u->source_output->state_change = source_output_state_change_cb;
    u->source_output->may_move_to = source_output_may_move_to_cb;
    u->source_output->moving = source_output_moving_cb;
    u->source_output->userdata = u;

    u->source->output_from_master = u->source_output;

    /* Create sink input */
    pa_sink_input_new_data_init(&sink_input_data);
    sink_input_data.driver = __FILE__;
    sink_input_data.module = m;
    pa_sink_input_new_data_set_sink(&sink_input_data, sink_master, false);
    sink_input_data.origin_sink = u->sink;
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_NAME, "Echo-Cancel Sink Stream");
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sink_input_data, &sink_ss);
    pa_sink_input_new_data_set_channel_map(&sink_input_data, &sink_map);
    sink_input_data.flags = PA_SINK_INPUT_VARIABLE_RATE;

    pa_sink_input_new(&u->sink_input, m->core, &sink_input_data);
    pa_sink_input_new_data_done(&sink_input_data);

    if (!u->sink_input)
        goto fail;

    u->sink_input->parent.process_msg = sink_input_process_msg_cb;
    u->sink_input->pop = sink_input_pop_cb;
    u->sink_input->process_rewind = sink_input_process_rewind_cb;
    u->sink_input->update_max_rewind = sink_input_update_max_rewind_cb;
    u->sink_input->update_max_request = sink_input_update_max_request_cb;
    u->sink_input->update_sink_requested_latency = sink_input_update_sink_requested_latency_cb;
    u->sink_input->update_sink_latency_range = sink_input_update_sink_latency_range_cb;
    u->sink_input->update_sink_fixed_latency = sink_input_update_sink_fixed_latency_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->attach = sink_input_attach_cb;
    u->sink_input->detach = sink_input_detach_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->may_move_to = sink_input_may_move_to_cb;
    u->sink_input->moving = sink_input_moving_cb;
    if (!u->use_volume_sharing)
        u->sink_input->volume_changed = sink_input_volume_changed_cb;
    u->sink_input->mute_changed = sink_input_mute_changed_cb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;

    pa_sink_input_get_silence(u->sink_input, &silence);

    u->source_memblockq = pa_memblockq_new("module-echo-cancel source_memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0,
        &source_output_ss, 1, 1, 0, &silence);
    u->sink_memblockq = pa_memblockq_new("module-echo-cancel sink_memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0,
        &sink_ss, 0, 1, 0, &silence);

    pa_memblock_unref(silence.memblock);

    if (!u->source_memblockq || !u->sink_memblockq) {
        pa_log("Failed to create memblockq.");
        goto fail;
    }

    if (u->adjust_time > 0 && !u->ec->params.drift_compensation)
        u->time_event = pa_core_rttime_new(m->core, pa_rtclock_now() + u->adjust_time, time_callback, u);
    else if (u->ec->params.drift_compensation) {
        pa_log_info("Canceller does drift compensation -- built-in compensation will be disabled");
        u->adjust_time = 0;
        /* Perform resync just once to give the canceller a leg up */
        pa_atomic_store(&u->request_resync, 1);
    }

    if (u->save_aec) {
        pa_log("Creating AEC files in /tmp");
        u->captured_file = fopen("/tmp/aec_rec.sw", "wb");
        if (u->captured_file == NULL)
            perror ("fopen failed");
        u->played_file = fopen("/tmp/aec_play.sw", "wb");
        if (u->played_file == NULL)
            perror ("fopen failed");
        u->canceled_file = fopen("/tmp/aec_out.sw", "wb");
        if (u->canceled_file == NULL)
            perror ("fopen failed");
        if (u->ec->params.drift_compensation) {
            u->drift_file = fopen("/tmp/aec_drift.txt", "w");
            if (u->drift_file == NULL)
                perror ("fopen failed");
        }
    }

    u->ec->msg = pa_msgobject_new(pa_echo_canceller_msg);
    u->ec->msg->parent.process_msg = canceller_process_msg_cb;
    u->ec->msg->userdata = u;

    u->thread_info.current_volume = u->source->reference_volume;

    pa_sink_put(u->sink);
    pa_source_put(u->source);

    pa_sink_input_put(u->sink_input);
    pa_source_output_put(u->source_output);
    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

/* Called from main context. */
int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink) + pa_source_linked_by(u->source);
}

/* Called from main context. */
void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    u->dead = true;

    /* See comments in source_output_kill_cb() above regarding
     * destruction order! */

    if (u->time_event)
        u->core->mainloop->time_free(u->time_event);

    if (u->source_output)
        pa_source_output_unlink(u->source_output);
    if (u->sink_input)
        pa_sink_input_unlink(u->sink_input);

    if (u->source)
        pa_source_unlink(u->source);
    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->source_output)
        pa_source_output_unref(u->source_output);
    if (u->sink_input)
        pa_sink_input_unref(u->sink_input);

    if (u->source)
        pa_source_unref(u->source);
    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->source_memblockq)
        pa_memblockq_free(u->source_memblockq);
    if (u->sink_memblockq)
        pa_memblockq_free(u->sink_memblockq);

    if (u->ec) {
        if (u->ec->done)
            u->ec->done(u->ec);

        pa_xfree(u->ec);
    }

    if (u->asyncmsgq)
        pa_asyncmsgq_unref(u->asyncmsgq);

    if (u->save_aec) {
        if (u->played_file)
            fclose(u->played_file);
        if (u->captured_file)
            fclose(u->captured_file);
        if (u->canceled_file)
            fclose(u->canceled_file);
        if (u->drift_file)
            fclose(u->drift_file);
    }

    pa_xfree(u);
}

#ifdef ECHO_CANCEL_TEST
/*
 * Stand-alone test program for running in the canceller on pre-recorded files.
 */
int main(int argc, char* argv[]) {
    struct userdata u;
    pa_sample_spec source_output_ss, source_ss, sink_ss;
    pa_channel_map source_output_map, source_map, sink_map;
    pa_modargs *ma = NULL;
    uint8_t *rdata = NULL, *pdata = NULL, *cdata = NULL;
    int unused PA_GCC_UNUSED;
    int ret = 0, i;
    char c;
    float drift;
    uint32_t nframes;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    pa_memzero(&u, sizeof(u));

    if (argc < 4 || argc > 7) {
        goto usage;
    }

    u.captured_file = fopen(argv[2], "rb");
    if (u.captured_file == NULL) {
        perror ("Could not open capture file");
        goto fail;
    }
    u.played_file = fopen(argv[1], "rb");
    if (u.played_file == NULL) {
        perror ("Could not open play file");
        goto fail;
    }
    u.canceled_file = fopen(argv[3], "wb");
    if (u.canceled_file == NULL) {
        perror ("Could not open canceled file");
        goto fail;
    }

    u.core = pa_xnew0(pa_core, 1);
    u.core->cpu_info.cpu_type = PA_CPU_X86;
    u.core->cpu_info.flags.x86 |= PA_CPU_X86_SSE;

    if (!(ma = pa_modargs_new(argc > 4 ? argv[4] : NULL, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    source_ss.format = PA_SAMPLE_S16LE;
    source_ss.rate = DEFAULT_RATE;
    source_ss.channels = DEFAULT_CHANNELS;
    pa_channel_map_init_auto(&source_map, source_ss.channels, PA_CHANNEL_MAP_DEFAULT);

    sink_ss.format = PA_SAMPLE_S16LE;
    sink_ss.rate = DEFAULT_RATE;
    sink_ss.channels = DEFAULT_CHANNELS;
    pa_channel_map_init_auto(&sink_map, sink_ss.channels, PA_CHANNEL_MAP_DEFAULT);

    if (init_common(ma, &u, &source_ss, &source_map) < 0)
        goto fail;

    source_output_ss = source_ss;
    source_output_map = source_map;

    if (!u.ec->init(u.core, u.ec, &source_output_ss, &source_output_map, &sink_ss, &sink_map, &source_ss, &source_map, &nframes,
                     pa_modargs_get_value(ma, "aec_args", NULL))) {
        pa_log("Failed to init AEC engine");
        goto fail;
    }
    u.source_output_blocksize = nframes * pa_frame_size(&source_output_ss);
    u.source_blocksize = nframes * pa_frame_size(&source_ss);
    u.sink_blocksize = nframes * pa_frame_size(&sink_ss);

    if (u.ec->params.drift_compensation) {
        if (argc < 6) {
            pa_log("Drift compensation enabled but drift file not specified");
            goto fail;
        }

        u.drift_file = fopen(argv[5], "rt");

        if (u.drift_file == NULL) {
            perror ("Could not open drift file");
            goto fail;
        }
    }

    rdata = pa_xmalloc(u.source_output_blocksize);
    pdata = pa_xmalloc(u.sink_blocksize);
    cdata = pa_xmalloc(u.source_blocksize);

    if (!u.ec->params.drift_compensation) {
        while (fread(rdata, u.source_output_blocksize, 1, u.captured_file) > 0) {
            if (fread(pdata, u.sink_blocksize, 1, u.played_file) == 0) {
                perror("Played file ended before captured file");
                goto fail;
            }

            u.ec->run(u.ec, rdata, pdata, cdata);

            unused = fwrite(cdata, u.source_blocksize, 1, u.canceled_file);
        }
    } else {
        while (fscanf(u.drift_file, "%c", &c) > 0) {
            switch (c) {
                case 'd':
                    if (!fscanf(u.drift_file, "%a", &drift)) {
                        perror("Drift file incomplete");
                        goto fail;
                    }

                    u.ec->set_drift(u.ec, drift);

                    break;

                case 'c':
                    if (!fscanf(u.drift_file, "%d", &i)) {
                        perror("Drift file incomplete");
                        goto fail;
                    }

                    if (fread(rdata, i, 1, u.captured_file) <= 0) {
                        perror("Captured file ended prematurely");
                        goto fail;
                    }

                    u.ec->record(u.ec, rdata, cdata);

                    unused = fwrite(cdata, i, 1, u.canceled_file);

                    break;

                case 'p':
                    if (!fscanf(u.drift_file, "%d", &i)) {
                        perror("Drift file incomplete");
                        goto fail;
                    }

                    if (fread(pdata, i, 1, u.played_file) <= 0) {
                        perror("Played file ended prematurely");
                        goto fail;
                    }

                    u.ec->play(u.ec, pdata);

                    break;
            }
        }

        if (fread(rdata, i, 1, u.captured_file) > 0)
            pa_log("All capture data was not consumed");
        if (fread(pdata, i, 1, u.played_file) > 0)
            pa_log("All playback data was not consumed");
    }

    u.ec->done(u.ec);

out:
    if (u.captured_file)
        fclose(u.captured_file);
    if (u.played_file)
        fclose(u.played_file);
    if (u.canceled_file)
        fclose(u.canceled_file);
    if (u.drift_file)
        fclose(u.drift_file);

    pa_xfree(rdata);
    pa_xfree(pdata);
    pa_xfree(cdata);

    pa_xfree(u.ec);
    pa_xfree(u.core);

    if (ma)
        pa_modargs_free(ma);

    return ret;

usage:
    pa_log("Usage: %s play_file rec_file out_file [module args] [drift_file]", argv[0]);

fail:
    ret = -1;
    goto out;
}
#endif /* ECHO_CANCEL_TEST */
