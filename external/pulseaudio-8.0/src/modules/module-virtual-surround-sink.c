/***
    This file is part of PulseAudio.

    Copyright 2010 Intel Corporation
    Contributor: Pierre-Louis Bossart <pierre-louis.bossart@intel.com>
    Copyright 2012 Niels Ole Salscheider <niels_ole@salscheider-online.de>

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

#include <pulse/gccmacro.h>
#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/ltdl-helper.h>
#include <pulsecore/sound-file.h>
#include <pulsecore/resampler.h>

#include <math.h>

#include "module-virtual-surround-sink-symdef.h"

PA_MODULE_AUTHOR("Niels Ole Salscheider");
PA_MODULE_DESCRIPTION(_("Virtual surround sink"));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        _("sink_name=<name for the sink> "
          "sink_properties=<properties for the sink> "
          "master=<name of sink to filter> "
          "format=<sample format> "
          "rate=<sample rate> "
          "channels=<number of channels> "
          "channel_map=<channel map> "
          "use_volume_sharing=<yes or no> "
          "force_flat_volume=<yes or no> "
          "hrir=/path/to/left_hrir.wav "
        ));

#define MEMBLOCKQ_MAXLENGTH (16*1024*1024)

struct userdata {
    pa_module *module;

    /* FIXME: Uncomment this and take "autoloaded" as a modarg if this is a filter */
    /* bool autoloaded; */

    pa_sink *sink;
    pa_sink_input *sink_input;

    pa_memblockq *memblockq;

    bool auto_desc;
    unsigned channels;
    unsigned hrir_channels;

    unsigned fs, sink_fs;

    unsigned *mapping_left;
    unsigned *mapping_right;

    unsigned hrir_samples;
    float *hrir_data;

    float *input_buffer;
    int input_buffer_offset;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "master",
    "format",
    "rate",
    "channels",
    "channel_map",
    "use_volume_sharing",
    "force_flat_volume",
    "hrir",
    NULL
};

/* Called from I/O thread context */
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
static int sink_set_state_cb(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return 0;

    pa_sink_input_cork(u->sink_input, state == PA_SINK_SUSPENDED);
    return 0;
}

/* Called from I/O thread context */
static void sink_request_rewind_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_request_rewind(u->sink_input,
                                 s->thread_info.rewind_nbytes +
                                 pa_memblockq_get_length(u->memblockq), true, false, false);
}

/* Called from I/O thread context */
static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_set_requested_latency_within_thread(
            u->sink_input,
            pa_sink_get_requested_latency_within_thread(s));
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

/* Called from I/O thread context */
static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;
    float *src, *dst;
    unsigned n;
    pa_memchunk tchunk;

    unsigned j, k, l;
    float sum_right, sum_left;
    float current_sample;

    pa_sink_input_assert_ref(i);
    pa_assert(chunk);
    pa_assert_se(u = i->userdata);

    /* Hmm, process any rewind request that might be queued up */
    pa_sink_process_rewind(u->sink, 0);

    while (pa_memblockq_peek(u->memblockq, &tchunk) < 0) {
        pa_memchunk nchunk;

        pa_sink_render(u->sink, nbytes * u->sink_fs / u->fs, &nchunk);
        pa_memblockq_push(u->memblockq, &nchunk);
        pa_memblock_unref(nchunk.memblock);
    }

    tchunk.length = PA_MIN(nbytes * u->sink_fs / u->fs, tchunk.length);
    pa_assert(tchunk.length > 0);

    n = (unsigned) (tchunk.length / u->sink_fs);

    pa_assert(n > 0);

    chunk->index = 0;
    chunk->length = n * u->fs;
    chunk->memblock = pa_memblock_new(i->sink->core->mempool, chunk->length);

    pa_memblockq_drop(u->memblockq, n * u->sink_fs);

    src = pa_memblock_acquire_chunk(&tchunk);
    dst = pa_memblock_acquire(chunk->memblock);

    for (l = 0; l < n; l++) {
        memcpy(((char*) u->input_buffer) + u->input_buffer_offset * u->sink_fs, ((char *) src) + l * u->sink_fs, u->sink_fs);

        sum_right = 0;
        sum_left = 0;

        /* fold the input buffer with the impulse response */
        for (j = 0; j < u->hrir_samples; j++) {
            for (k = 0; k < u->channels; k++) {
                current_sample = u->input_buffer[((u->input_buffer_offset + j) % u->hrir_samples) * u->channels + k];

                sum_left += current_sample * u->hrir_data[j * u->hrir_channels + u->mapping_left[k]];
                sum_right += current_sample * u->hrir_data[j * u->hrir_channels + u->mapping_right[k]];
            }
        }

        dst[2 * l] = PA_CLAMP_UNLIKELY(sum_left, -1.0f, 1.0f);
        dst[2 * l + 1] = PA_CLAMP_UNLIKELY(sum_right, -1.0f, 1.0f);

        u->input_buffer_offset--;
        if (u->input_buffer_offset < 0)
            u->input_buffer_offset += u->hrir_samples;
    }

    pa_memblock_release(tchunk.memblock);
    pa_memblock_release(chunk->memblock);

    pa_memblock_unref(tchunk.memblock);

    return 0;
}

/* Called from I/O thread context */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;
    size_t amount = 0;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->sink->thread_info.rewind_nbytes > 0) {
        size_t max_rewrite;

        max_rewrite = nbytes * u->sink_fs / u->fs + pa_memblockq_get_length(u->memblockq);
        amount = PA_MIN(u->sink->thread_info.rewind_nbytes * u->sink_fs / u->fs, max_rewrite);
        u->sink->thread_info.rewind_nbytes = 0;

        if (amount > 0) {
            pa_memblockq_seek(u->memblockq, - (int64_t) amount, PA_SEEK_RELATIVE, true);

            /* Reset the input buffer */
            memset(u->input_buffer, 0, u->hrir_samples * u->sink_fs);
            u->input_buffer_offset = 0;
        }
    }

    pa_sink_process_rewind(u->sink, amount);
    pa_memblockq_rewind(u->memblockq, nbytes * u->sink_fs / u->fs);
}

/* Called from I/O thread context */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_memblockq_set_maxrewind(u->memblockq, nbytes * u->sink_fs / u->fs);
    pa_sink_set_max_rewind_within_thread(u->sink, nbytes * u->sink_fs / u->fs);
}

/* Called from I/O thread context */
static void sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_max_request_within_thread(u->sink, nbytes * u->sink_fs / u->fs);
}

/* Called from I/O thread context */
static void sink_input_update_sink_latency_range_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
}

/* Called from I/O thread context */
static void sink_input_update_sink_fixed_latency_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
}

/* Called from I/O thread context */
static void sink_input_detach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_detach_within_thread(u->sink);

    pa_sink_set_rtpoll(u->sink, NULL);
}

/* Called from I/O thread context */
static void sink_input_attach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_rtpoll(u->sink, i->sink->thread_info.rtpoll);
    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);

    pa_sink_set_max_request_within_thread(u->sink, pa_sink_input_get_max_request(i) * u->sink_fs / u->fs);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i) * u->sink_fs / u->fs);

    pa_sink_attach_within_thread(u->sink);
}

/* Called from main context */
static void sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* The order here matters! We first kill the sink input, followed
     * by the sink. That means the sink callbacks must be protected
     * against an unconnected sink input! */
    pa_sink_input_unlink(u->sink_input);
    pa_sink_unlink(u->sink);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    pa_sink_unref(u->sink);
    u->sink = NULL;

    pa_module_unload_request(u->module, true);
}

/* Called from IO thread context */
static void sink_input_state_change_cb(pa_sink_input *i, pa_sink_input_state_t state) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* If we are added for the first time, ask for a rewinding so that
     * we are heard right-away. */
    if (PA_SINK_INPUT_IS_LINKED(state) &&
        i->thread_info.state == PA_SINK_INPUT_INIT) {
        pa_log_debug("Requesting rewind due to state change.");
        pa_sink_input_request_rewind(i, 0, false, true, true);
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

    if (u->auto_desc && dest) {
        const char *z;
        pa_proplist *pl;

        pl = pa_proplist_new();
        z = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "Virtual Surround Sink %s on %s",
                         pa_proplist_gets(u->sink->proplist, "device.vsurroundsink.name"), z ? z : dest->name);

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

static pa_channel_position_t mirror_channel(pa_channel_position_t channel) {
    switch (channel) {
        case PA_CHANNEL_POSITION_FRONT_LEFT:
            return PA_CHANNEL_POSITION_FRONT_RIGHT;

        case PA_CHANNEL_POSITION_FRONT_RIGHT:
            return PA_CHANNEL_POSITION_FRONT_LEFT;

        case PA_CHANNEL_POSITION_REAR_LEFT:
            return PA_CHANNEL_POSITION_REAR_RIGHT;

        case PA_CHANNEL_POSITION_REAR_RIGHT:
            return PA_CHANNEL_POSITION_REAR_LEFT;

        case PA_CHANNEL_POSITION_SIDE_LEFT:
            return PA_CHANNEL_POSITION_SIDE_RIGHT;

        case PA_CHANNEL_POSITION_SIDE_RIGHT:
            return PA_CHANNEL_POSITION_SIDE_LEFT;

        case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
            return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;

        case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
            return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;

        case PA_CHANNEL_POSITION_TOP_FRONT_LEFT:
            return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;

        case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT:
            return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;

        case PA_CHANNEL_POSITION_TOP_REAR_LEFT:
            return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;

        case PA_CHANNEL_POSITION_TOP_REAR_RIGHT:
            return PA_CHANNEL_POSITION_TOP_REAR_LEFT;

        default:
            return channel;
    }
}

static void normalize_hrir(struct userdata *u) {
    /* normalize hrir to avoid audible clipping
     *
     * The following heuristic tries to avoid audible clipping. It cannot avoid
     * clipping in the worst case though, because the scaling factor would
     * become too large resulting in a too quiet signal.
     * The idea of the heuristic is to avoid clipping when a single click is
     * played back on all channels. The scaling factor describes the additional
     * factor that is necessary to avoid clipping for "normal" signals.
     *
     * This algorithm doesn't pretend to be perfect, it's just something that
     * appears to work (not too quiet, no audible clipping) on the material that
     * it has been tested on. If you find a real-world example where this
     * algorithm results in audible clipping, please write a patch that adjusts
     * the scaling factor constants or improves the algorithm (or if you can't
     * write a patch, at least report the problem to the PulseAudio mailing list
     * or bug tracker). */

    const float scaling_factor = 2.5;

    float hrir_sum, hrir_max;
    unsigned i, j;

    hrir_max = 0;
    for (i = 0; i < u->hrir_samples; i++) {
        hrir_sum = 0;
        for (j = 0; j < u->hrir_channels; j++)
            hrir_sum += fabs(u->hrir_data[i * u->hrir_channels + j]);

        if (hrir_sum > hrir_max)
            hrir_max = hrir_sum;
    }

    for (i = 0; i < u->hrir_samples; i++) {
        for (j = 0; j < u->hrir_channels; j++)
            u->hrir_data[i * u->hrir_channels + j] /= hrir_max * scaling_factor;
    }
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss, sink_input_ss;
    pa_channel_map map, sink_input_map;
    pa_modargs *ma;
    pa_sink *master=NULL;
    pa_sink_input_new_data sink_input_data;
    pa_sink_new_data sink_data;
    bool use_volume_sharing = true;
    bool force_flat_volume = false;
    pa_memchunk silence;

    const char *hrir_file;
    unsigned i, j, found_channel_left, found_channel_right;
    float *hrir_data;

    pa_sample_spec hrir_ss;
    pa_channel_map hrir_map;

    pa_sample_spec hrir_temp_ss;
    pa_memchunk hrir_temp_chunk, hrir_temp_chunk_resampled;
    pa_resampler *resampler;

    size_t hrir_copied_length, hrir_total_length;

    hrir_temp_chunk.memblock = NULL;
    hrir_temp_chunk_resampled.memblock = NULL;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }

    pa_assert(master);

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;

    /* Initialize hrir and input buffer */
    /* this is the hrir file for the left ear! */
    if (!(hrir_file = pa_modargs_get_value(ma, "hrir", NULL))) {
        pa_log("The mandatory 'hrir' module argument is missing.");
        goto fail;
    }

    if (pa_sound_file_load(master->core->mempool, hrir_file, &hrir_temp_ss, &hrir_map, &hrir_temp_chunk, NULL) < 0) {
        pa_log("Cannot load hrir file.");
        goto fail;
    }

    /* sample spec / map of hrir */
    hrir_ss.format = PA_SAMPLE_FLOAT32;
    hrir_ss.rate = master->sample_spec.rate;
    hrir_ss.channels = hrir_temp_ss.channels;

    /* sample spec of sink */
    ss = hrir_ss;
    map = hrir_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }
    ss.format = PA_SAMPLE_FLOAT32;
    hrir_ss.rate = ss.rate;
    u->channels = ss.channels;

    if (pa_modargs_get_value_boolean(ma, "use_volume_sharing", &use_volume_sharing) < 0) {
        pa_log("use_volume_sharing= expects a boolean argument");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "force_flat_volume", &force_flat_volume) < 0) {
        pa_log("force_flat_volume= expects a boolean argument");
        goto fail;
    }

    if (use_volume_sharing && force_flat_volume) {
        pa_log("Flat volume can't be forced when using volume sharing.");
        goto fail;
    }

    /* sample spec / map of sink input */
    pa_channel_map_init_stereo(&sink_input_map);
    sink_input_ss.channels = 2;
    sink_input_ss.format = PA_SAMPLE_FLOAT32;
    sink_input_ss.rate = ss.rate;

    u->sink_fs = pa_frame_size(&ss);
    u->fs = pa_frame_size(&sink_input_ss);

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if (!(sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sink_data.name = pa_sprintf_malloc("%s.vsurroundsink", master->name);
    pa_sink_new_data_set_sample_spec(&sink_data, &ss);
    pa_sink_new_data_set_channel_map(&sink_data, &map);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "filter");
    pa_proplist_sets(sink_data.proplist, "device.vsurroundsink.name", sink_data.name);

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    if ((u->auto_desc = !pa_proplist_contains(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *z;

        z = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Virtual Surround Sink %s on %s", sink_data.name, z ? z : master->name);
    }

    u->sink = pa_sink_new(m->core, &sink_data, (master->flags & (PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY))
                                               | (use_volume_sharing ? PA_SINK_SHARE_VOLUME_WITH_MASTER : 0));
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
    if (!use_volume_sharing) {
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        pa_sink_enable_decibel_volume(u->sink, true);
    }
    /* Normally this flag would be enabled automatically be we can force it. */
    if (force_flat_volume)
        u->sink->flags |= PA_SINK_FLAT_VOLUME;
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);

    /* Create sink input */
    pa_sink_input_new_data_init(&sink_input_data);
    sink_input_data.driver = __FILE__;
    sink_input_data.module = m;
    pa_sink_input_new_data_set_sink(&sink_input_data, master, false);
    sink_input_data.origin_sink = u->sink;
    pa_proplist_setf(sink_input_data.proplist, PA_PROP_MEDIA_NAME, "Virtual Surround Sink Stream from %s", pa_proplist_gets(u->sink->proplist, PA_PROP_DEVICE_DESCRIPTION));
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sink_input_data, &sink_input_ss);
    pa_sink_input_new_data_set_channel_map(&sink_input_data, &sink_input_map);

    pa_sink_input_new(&u->sink_input, m->core, &sink_input_data);
    pa_sink_input_new_data_done(&sink_input_data);

    if (!u->sink_input)
        goto fail;

    u->sink_input->pop = sink_input_pop_cb;
    u->sink_input->process_rewind = sink_input_process_rewind_cb;
    u->sink_input->update_max_rewind = sink_input_update_max_rewind_cb;
    u->sink_input->update_max_request = sink_input_update_max_request_cb;
    u->sink_input->update_sink_latency_range = sink_input_update_sink_latency_range_cb;
    u->sink_input->update_sink_fixed_latency = sink_input_update_sink_fixed_latency_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->attach = sink_input_attach_cb;
    u->sink_input->detach = sink_input_detach_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->moving = sink_input_moving_cb;
    u->sink_input->volume_changed = use_volume_sharing ? NULL : sink_input_volume_changed_cb;
    u->sink_input->mute_changed = sink_input_mute_changed_cb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;

    pa_sink_input_get_silence(u->sink_input, &silence);
    u->memblockq = pa_memblockq_new("module-virtual-surround-sink memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0, &sink_input_ss, 1, 1, 0, &silence);
    pa_memblock_unref(silence.memblock);

    /* resample hrir */
    resampler = pa_resampler_new(u->sink->core->mempool, &hrir_temp_ss, &hrir_map, &hrir_ss, &hrir_map, u->sink->core->lfe_crossover_freq,
                                 PA_RESAMPLER_SRC_SINC_BEST_QUALITY, PA_RESAMPLER_NO_REMAP);

    u->hrir_samples = hrir_temp_chunk.length / pa_frame_size(&hrir_temp_ss) * hrir_ss.rate / hrir_temp_ss.rate;
    if (u->hrir_samples > 64) {
        u->hrir_samples = 64;
        pa_log("The (resampled) hrir contains more than 64 samples. Only the first 64 samples will be used to limit processor usage.");
    }

    hrir_total_length = u->hrir_samples * pa_frame_size(&hrir_ss);
    u->hrir_channels = hrir_ss.channels;

    u->hrir_data = (float *) pa_xmalloc(hrir_total_length);
    hrir_copied_length = 0;

    /* add silence to the hrir until we get enough samples out of the resampler */
    while (hrir_copied_length < hrir_total_length) {
        pa_resampler_run(resampler, &hrir_temp_chunk, &hrir_temp_chunk_resampled);
        if (hrir_temp_chunk.memblock != hrir_temp_chunk_resampled.memblock) {
            /* Silence input block */
            pa_silence_memblock(hrir_temp_chunk.memblock, &hrir_temp_ss);
        }

        if (hrir_temp_chunk_resampled.memblock) {
            /* Copy hrir data */
            hrir_data = (float *) pa_memblock_acquire(hrir_temp_chunk_resampled.memblock);

            if (hrir_total_length - hrir_copied_length >= hrir_temp_chunk_resampled.length) {
                memcpy(u->hrir_data + hrir_copied_length, hrir_data, hrir_temp_chunk_resampled.length);
                hrir_copied_length += hrir_temp_chunk_resampled.length;
            } else {
                memcpy(u->hrir_data + hrir_copied_length, hrir_data, hrir_total_length - hrir_copied_length);
                hrir_copied_length = hrir_total_length;
            }

            pa_memblock_release(hrir_temp_chunk_resampled.memblock);
            pa_memblock_unref(hrir_temp_chunk_resampled.memblock);
            hrir_temp_chunk_resampled.memblock = NULL;
        }
    }

    pa_resampler_free(resampler);

    pa_memblock_unref(hrir_temp_chunk.memblock);
    hrir_temp_chunk.memblock = NULL;

    if (hrir_map.channels < map.channels) {
        pa_log("hrir file does not have enough channels!");
        goto fail;
    }

    normalize_hrir(u);

    /* create mapping between hrir and input */
    u->mapping_left = (unsigned *) pa_xnew0(unsigned, u->channels);
    u->mapping_right = (unsigned *) pa_xnew0(unsigned, u->channels);
    for (i = 0; i < map.channels; i++) {
        found_channel_left = 0;
        found_channel_right = 0;

        for (j = 0; j < hrir_map.channels; j++) {
            if (hrir_map.map[j] == map.map[i]) {
                u->mapping_left[i] = j;
                found_channel_left = 1;
            }

            if (hrir_map.map[j] == mirror_channel(map.map[i])) {
                u->mapping_right[i] = j;
                found_channel_right = 1;
            }
        }

        if (!found_channel_left) {
            pa_log("Cannot find mapping for channel %s", pa_channel_position_to_string(map.map[i]));
            goto fail;
        }
        if (!found_channel_right) {
            pa_log("Cannot find mapping for channel %s", pa_channel_position_to_string(mirror_channel(map.map[i])));
            goto fail;
        }
    }

    u->input_buffer = pa_xmalloc0(u->hrir_samples * u->sink_fs);
    u->input_buffer_offset = 0;

    pa_sink_put(u->sink);
    pa_sink_input_put(u->sink_input);

    pa_modargs_free(ma);
    return 0;

fail:
    if (hrir_temp_chunk.memblock)
        pa_memblock_unref(hrir_temp_chunk.memblock);

    if (hrir_temp_chunk_resampled.memblock)
        pa_memblock_unref(hrir_temp_chunk_resampled.memblock);

    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink);
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    /* See comments in sink_input_kill_cb() above regarding
     * destruction order! */

    if (u->sink_input)
        pa_sink_input_unlink(u->sink_input);

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->sink_input)
        pa_sink_input_unref(u->sink_input);

    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->memblockq)
        pa_memblockq_free(u->memblockq);

    if (u->hrir_data)
        pa_xfree(u->hrir_data);

    if (u->input_buffer)
        pa_xfree(u->input_buffer);

    if (u->mapping_left)
        pa_xfree(u->mapping_left);
    if (u->mapping_right)
        pa_xfree(u->mapping_right);

    pa_xfree(u);
}
