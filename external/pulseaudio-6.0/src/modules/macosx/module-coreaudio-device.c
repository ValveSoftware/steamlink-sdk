/***
  This file is part of PulseAudio.

  Copyright 2009,2010 Daniel Mack <daniel@caiaq.de>

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

/* TODO:
    - implement hardware volume controls
    - handle audio device stream format changes (will require changes to the core)
    - add an "off" mode that removes all sinks and sources
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/xmalloc.h>

#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/module.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/llist.h>
#include <pulsecore/card.h>
#include <pulsecore/strbuf.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/i18n.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreAudio/AudioHardware.h>

#include "module-coreaudio-device-symdef.h"

#define DEFAULT_FRAMES_PER_IOPROC 512

PA_MODULE_AUTHOR("Daniel Mack");
PA_MODULE_DESCRIPTION("CoreAudio device");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE("object_id=<the CoreAudio device id> "
                "ioproc_frames=<audio frames per IOProc call> ");

static const char* const valid_modargs[] = {
    "object_id",
    "ioproc_frames",
    NULL
};

enum {
    CA_MESSAGE_RENDER = PA_SINK_MESSAGE_MAX,
};

typedef struct coreaudio_sink coreaudio_sink;
typedef struct coreaudio_source coreaudio_source;

struct userdata {
    AudioObjectID object_id;
    AudioDeviceIOProcID proc_id;

    pa_thread_mq thread_mq;
    pa_asyncmsgq *async_msgq;

    pa_rtpoll *rtpoll;
    pa_thread *thread;

    pa_module *module;
    pa_card *card;
    bool running;

    char *device_name, *vendor_name;

    const AudioBufferList *render_input_data;
    AudioBufferList       *render_output_data;

    AudioStreamBasicDescription stream_description;

    PA_LLIST_HEAD(coreaudio_sink, sinks);
    PA_LLIST_HEAD(coreaudio_source, sources);
};

struct coreaudio_sink {
    pa_sink *pa_sink;
    struct userdata *userdata;

    char *name;
    unsigned int channel_idx;
    bool active;

    pa_channel_map map;
    pa_sample_spec ss;

    PA_LLIST_FIELDS(coreaudio_sink);
};

struct coreaudio_source {
    pa_source *pa_source;
    struct userdata *userdata;

    char *name;
    unsigned int channel_idx;
    bool active;

    pa_channel_map map;
    pa_sample_spec ss;

    PA_LLIST_FIELDS(coreaudio_source);
};

static int card_set_profile(pa_card *c, pa_card_profile *new_profile) {
    return 0;
}

static OSStatus io_render_proc (AudioDeviceID          device,
                                const AudioTimeStamp  *now,
                                const AudioBufferList *inputData,
                                const AudioTimeStamp  *inputTime,
                                AudioBufferList       *outputData,
                                const AudioTimeStamp  *outputTime,
                                void                  *clientData) {
    struct userdata *u = clientData;

    pa_assert(u);
    pa_assert(device == u->object_id);

    u->render_input_data = inputData;
    u->render_output_data = outputData;

    if (u->sinks)
        pa_assert_se(pa_asyncmsgq_send(u->async_msgq, PA_MSGOBJECT(u->sinks->pa_sink),
                                        CA_MESSAGE_RENDER, NULL, 0, NULL) == 0);

    if (u->sources)
        pa_assert_se(pa_asyncmsgq_send(u->async_msgq, PA_MSGOBJECT(u->sources->pa_source),
                                        CA_MESSAGE_RENDER, NULL, 0, NULL) == 0);

    return 0;
}

static OSStatus ca_stream_format_changed(AudioObjectID objectID,
                                         UInt32 numberAddresses,
                                         const AudioObjectPropertyAddress addresses[],
                                         void *clientData) {
    struct userdata *u = clientData;
    UInt32 i;

    pa_assert(u);

    /* REVISIT: PA can't currently handle external format change requests.
     * Hence, we set the original format back in this callback to avoid horrible audio artefacts.
     * The device settings will appear to be 'locked' for any application as long as the PA daemon is running.
     * Once we're able to propagate such events up in the core, this needs to be changed. */

    for (i = 0; i < numberAddresses; i++)
        AudioObjectSetPropertyData(objectID, addresses + i, 0, NULL, sizeof(u->stream_description), &u->stream_description);

    return 0;
}

static pa_usec_t get_latency_us(pa_object *o) {
    struct userdata *u;
    pa_sample_spec *ss;
    bool is_source;
    UInt32 v = 0, total = 0;
    UInt32 err, size = sizeof(v);
    AudioObjectPropertyAddress property_address;
    AudioObjectID stream_id;

    if (pa_sink_isinstance(o)) {
        coreaudio_sink *sink = PA_SINK(o)->userdata;

        u = sink->userdata;
        ss = &sink->ss;
        is_source = false;
    } else if (pa_source_isinstance(o)) {
        coreaudio_source *source = PA_SOURCE(o)->userdata;

        u = source->userdata;
        ss = &source->ss;
        is_source = true;
    } else
        pa_assert_not_reached();

    pa_assert(u);

    property_address.mScope = is_source ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    /* get the device latency */
    property_address.mSelector = kAudioDevicePropertyLatency;
    size = sizeof(v);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, &v);
    if (!err)
        total += v;
    else
        pa_log_warn("Failed to get device latency: %i", err);

    /* the IOProc buffer size */
    property_address.mSelector = kAudioDevicePropertyBufferFrameSize;
    size = sizeof(v);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, &v);
    if (!err)
        total += v;
    else
        pa_log_warn("Failed to get buffer frame size: %i", err);

    /* IOProc safety offset - this value is the same for both directions, hence we divide it by 2 */
    property_address.mSelector = kAudioDevicePropertySafetyOffset;
    size = sizeof(v);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, &v);
    if (!err)
        total += v / 2;
    else
        pa_log_warn("Failed to get safety offset: %i", err);

    /* get the stream latency.
     * FIXME: this assumes the stream latency is the same for all streams */
    property_address.mSelector = kAudioDevicePropertyStreams;
    size = sizeof(stream_id);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, &stream_id);
    if (!err) {
        property_address.mSelector = kAudioStreamPropertyLatency;
        property_address.mScope = kAudioObjectPropertyScopeGlobal;
        size = sizeof(v);
        err = AudioObjectGetPropertyData(stream_id, &property_address, 0, NULL, &size, &v);
        if (!err)
            total += v;
        else
            pa_log_warn("Failed to get stream latency: %i", err);
    } else
        pa_log_warn("Failed to get streams: %i", err);

    return pa_bytes_to_usec(total * pa_frame_size(ss), ss);
}

static void ca_device_check_device_state(struct userdata *u) {
    coreaudio_sink *ca_sink;
    coreaudio_source *ca_source;
    bool active = false;

    pa_assert(u);

    for (ca_sink = u->sinks; ca_sink; ca_sink = ca_sink->next)
        if (ca_sink->active)
            active = true;

    for (ca_source = u->sources; ca_source; ca_source = ca_source->next)
        if (ca_source->active)
            active = true;

    if (active && !u->running)
        AudioDeviceStart(u->object_id, u->proc_id);
    else if (!active && u->running)
        AudioDeviceStop(u->object_id, u->proc_id);

    u->running = active;
}

static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    coreaudio_sink *sink = PA_SINK(o)->userdata;
    struct userdata *u = sink->userdata;
    unsigned int i;
    pa_memchunk audio_chunk;

    switch (code) {
        case CA_MESSAGE_RENDER: {
            /* audio out */
            for (i = 0; i < u->render_output_data->mNumberBuffers; i++) {
                AudioBuffer *buf = u->render_output_data->mBuffers + i;

                pa_assert(sink);

                if (PA_SINK_IS_OPENED(sink->pa_sink->thread_info.state)) {
                    audio_chunk.memblock = pa_memblock_new_fixed(u->module->core->mempool, buf->mData, buf->mDataByteSize, false);
                    audio_chunk.length = buf->mDataByteSize;
                    audio_chunk.index = 0;

                    pa_sink_render_into_full(sink->pa_sink, &audio_chunk);
                    pa_memblock_unref_fixed(audio_chunk.memblock);
                }

                sink = sink->next;
            }

            return 0;
        }

        case PA_SINK_MESSAGE_GET_LATENCY: {
            *((pa_usec_t *) data) = get_latency_us(PA_OBJECT(o));
            return 0;
        }
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    coreaudio_source *source = PA_SOURCE(o)->userdata;
    struct userdata *u = source->userdata;
    unsigned int i;
    pa_memchunk audio_chunk;

    switch (code) {
        case CA_MESSAGE_RENDER: {
            /* audio in */
            for (i = 0; i < u->render_input_data->mNumberBuffers; i++) {
                const AudioBuffer *buf = u->render_input_data->mBuffers + i;

                pa_assert(source);

                if (PA_SOURCE_IS_OPENED(source->pa_source->thread_info.state)) {
                    audio_chunk.memblock = pa_memblock_new_fixed(u->module->core->mempool, buf->mData, buf->mDataByteSize, true);
                    audio_chunk.length = buf->mDataByteSize;
                    audio_chunk.index = 0;

                    pa_source_post(source->pa_source, &audio_chunk);
                    pa_memblock_unref_fixed(audio_chunk.memblock);
                }

                source = source->next;
            }

            return 0;
        }

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            *((pa_usec_t *) data) = get_latency_us(PA_OBJECT(o));
            return 0;
        }
    }

    return pa_source_process_msg(o, code, data, offset, chunk);;
}

static int ca_sink_set_state(pa_sink *s, pa_sink_state_t state) {
    coreaudio_sink *sink = s->userdata;

    switch (state) {
        case PA_SINK_SUSPENDED:
        case PA_SINK_IDLE:
            sink->active = false;
            break;

        case PA_SINK_RUNNING:
            sink->active = true;
            break;

        case PA_SINK_UNLINKED:
        case PA_SINK_INIT:
        case PA_SINK_INVALID_STATE:
            ;
    }

    ca_device_check_device_state(sink->userdata);

    return 0;
}

static int ca_device_create_sink(pa_module *m, AudioBuffer *buf, int channel_idx) {
    OSStatus err;
    UInt32 size;
    struct userdata *u = m->userdata;
    pa_sink_new_data new_data;
    pa_sink_flags_t flags = PA_SINK_LATENCY | PA_SINK_HARDWARE;
    coreaudio_sink *ca_sink;
    pa_sink *sink;
    unsigned int i;
    char tmp[255];
    pa_strbuf *strbuf;
    AudioObjectPropertyAddress property_address;

    ca_sink = pa_xnew0(coreaudio_sink, 1);
    ca_sink->map.channels = buf->mNumberChannels;
    ca_sink->ss.channels = buf->mNumberChannels;
    ca_sink->channel_idx = channel_idx;

    /* build a name for this stream */
    strbuf = pa_strbuf_new();

    for (i = 0; i < buf->mNumberChannels; i++) {
        property_address.mSelector = kAudioObjectPropertyElementName;
        property_address.mScope = kAudioDevicePropertyScopeOutput;
        property_address.mElement = channel_idx + i + 1;
        size = sizeof(tmp);
        err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, tmp);
        if (err || !strlen(tmp))
            snprintf(tmp, sizeof(tmp), "Channel %d", (int) property_address.mElement);

        if (i > 0)
            pa_strbuf_puts(strbuf, ", ");

        pa_strbuf_puts(strbuf, tmp);
    }

    ca_sink->name = pa_strbuf_tostring_free(strbuf);

    pa_log_debug("Stream name is >%s<", ca_sink->name);

    /* default to mono streams */
    for (i = 0; i < ca_sink->map.channels; i++)
        ca_sink->map.map[i] = PA_CHANNEL_POSITION_MONO;

    if (buf->mNumberChannels == 2) {
        ca_sink->map.map[0] = PA_CHANNEL_POSITION_LEFT;
        ca_sink->map.map[1] = PA_CHANNEL_POSITION_RIGHT;
    }

    ca_sink->ss.rate = u->stream_description.mSampleRate;
    ca_sink->ss.format = PA_SAMPLE_FLOAT32LE;

    pa_sink_new_data_init(&new_data);
    new_data.card = u->card;
    new_data.driver = __FILE__;
    new_data.module = u->module;
    new_data.namereg_fail = false;
    pa_sink_new_data_set_name(&new_data, ca_sink->name);
    pa_sink_new_data_set_channel_map(&new_data, &ca_sink->map);
    pa_sink_new_data_set_sample_spec(&new_data, &ca_sink->ss);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_STRING, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_PRODUCT_NAME, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_DESCRIPTION, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_ACCESS_MODE, "mmap");
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_API, "CoreAudio");
    pa_proplist_setf(new_data.proplist, PA_PROP_DEVICE_BUFFERING_BUFFER_SIZE, "%lu", (unsigned long) buf->mDataByteSize);

    if (u->vendor_name)
        pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_VENDOR_NAME, u->vendor_name);

    sink = pa_sink_new(m->core, &new_data, flags);
    pa_sink_new_data_done(&new_data);

    if (!sink) {
        pa_log("unable to create sink.");
        return -1;
    }

    sink->parent.process_msg = sink_process_msg;
    sink->userdata = ca_sink;
    sink->set_state = ca_sink_set_state;

    pa_sink_set_asyncmsgq(sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(sink, u->rtpoll);

    ca_sink->pa_sink = sink;
    ca_sink->userdata = u;

    PA_LLIST_PREPEND(coreaudio_sink, u->sinks, ca_sink);

    return 0;
}

static int ca_source_set_state(pa_source *s, pa_source_state_t state) {
    coreaudio_source *source = s->userdata;

    switch (state) {
        case PA_SOURCE_SUSPENDED:
        case PA_SOURCE_IDLE:
            source->active = false;
            break;

        case PA_SOURCE_RUNNING:
            source->active = true;
            break;

        case PA_SOURCE_UNLINKED:
        case PA_SOURCE_INIT:
        case PA_SOURCE_INVALID_STATE:
            ;
    }

    ca_device_check_device_state(source->userdata);

    return 0;
}

static int ca_device_create_source(pa_module *m, AudioBuffer *buf, int channel_idx) {
    OSStatus err;
    UInt32 size;
    struct userdata *u = m->userdata;
    pa_source_new_data new_data;
    pa_source_flags_t flags = PA_SOURCE_LATENCY | PA_SOURCE_HARDWARE;
    coreaudio_source *ca_source;
    pa_source *source;
    unsigned int i;
    char tmp[255];
    pa_strbuf *strbuf;
    AudioObjectPropertyAddress property_address;

    ca_source = pa_xnew0(coreaudio_source, 1);
    ca_source->map.channels = buf->mNumberChannels;
    ca_source->ss.channels = buf->mNumberChannels;
    ca_source->channel_idx = channel_idx;

    /* build a name for this stream */
    strbuf = pa_strbuf_new();

    for (i = 0; i < buf->mNumberChannels; i++) {
        property_address.mSelector = kAudioObjectPropertyElementName;
        property_address.mScope = kAudioDevicePropertyScopeInput;
        property_address.mElement = channel_idx + i + 1;
        size = sizeof(tmp);
        err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, tmp);
        if (err || !strlen(tmp))
            snprintf(tmp, sizeof(tmp), "Channel %d", (int) property_address.mElement);

        if (i > 0)
            pa_strbuf_puts(strbuf, ", ");

        pa_strbuf_puts(strbuf, tmp);
    }

    ca_source->name = pa_strbuf_tostring_free(strbuf);

    pa_log_debug("Stream name is >%s<", ca_source->name);

    /* default to mono streams */
    for (i = 0; i < ca_source->map.channels; i++)
        ca_source->map.map[i] = PA_CHANNEL_POSITION_MONO;

    if (buf->mNumberChannels == 2) {
        ca_source->map.map[0] = PA_CHANNEL_POSITION_LEFT;
        ca_source->map.map[1] = PA_CHANNEL_POSITION_RIGHT;
    }

    ca_source->ss.rate = u->stream_description.mSampleRate;
    ca_source->ss.format = PA_SAMPLE_FLOAT32LE;

    pa_source_new_data_init(&new_data);
    new_data.card = u->card;
    new_data.driver = __FILE__;
    new_data.module = u->module;
    new_data.namereg_fail = false;
    pa_source_new_data_set_name(&new_data, ca_source->name);
    pa_source_new_data_set_channel_map(&new_data, &ca_source->map);
    pa_source_new_data_set_sample_spec(&new_data, &ca_source->ss);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_STRING, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_PRODUCT_NAME, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_DESCRIPTION, u->device_name);
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_ACCESS_MODE, "mmap");
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_API, "CoreAudio");
    pa_proplist_setf(new_data.proplist, PA_PROP_DEVICE_BUFFERING_BUFFER_SIZE, "%lu", (unsigned long) buf->mDataByteSize);

    if (u->vendor_name)
        pa_proplist_sets(new_data.proplist, PA_PROP_DEVICE_VENDOR_NAME, u->vendor_name);

    source = pa_source_new(m->core, &new_data, flags);
    pa_source_new_data_done(&new_data);

    if (!source) {
        pa_log("unable to create source.");
        return -1;
    }

    source->parent.process_msg = source_process_msg;
    source->userdata = ca_source;
    source->set_state = ca_source_set_state;

    pa_source_set_asyncmsgq(source, u->thread_mq.inq);
    pa_source_set_rtpoll(source, u->rtpoll);

    ca_source->pa_source = source;
    ca_source->userdata = u;

    PA_LLIST_PREPEND(coreaudio_source, u->sources, ca_source);

    return 0;
}

static int ca_device_create_streams(pa_module *m, bool direction_in) {
    OSStatus err;
    UInt32 size, i, channel_idx;
    struct userdata *u = m->userdata;
    AudioBufferList *buffer_list;
    AudioObjectPropertyAddress property_address;

    property_address.mScope = direction_in ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    /* get current stream format */
    size = sizeof(AudioStreamBasicDescription);
    property_address.mSelector = kAudioDevicePropertyStreamFormat;
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, &u->stream_description);
    if (err) {
        /* no appropriate streams found - silently bail. */
        return -1;
    }

    if (u->stream_description.mFormatID != kAudioFormatLinearPCM) {
        pa_log("Unsupported audio format '%c%c%c%c'",
            (char) (u->stream_description.mFormatID >> 24),
            (char) (u->stream_description.mFormatID >> 16) & 0xff,
            (char) (u->stream_description.mFormatID >> 8) & 0xff,
            (char) (u->stream_description.mFormatID & 0xff));
        return -1;
    }

    /* get stream configuration */
    size = 0;
    property_address.mSelector = kAudioDevicePropertyStreamConfiguration;
    err = AudioObjectGetPropertyDataSize(u->object_id, &property_address, 0, NULL, &size);
    if (err) {
        pa_log("Failed to get kAudioDevicePropertyStreamConfiguration (%s).", direction_in ? "input" : "output");
        return -1;
    }

    if (!size)
        return 0;

    buffer_list = (AudioBufferList *) pa_xmalloc(size);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, buffer_list);

    if (!err) {
        pa_log_debug("Sample rate: %f", u->stream_description.mSampleRate);
        pa_log_debug("%d bytes per packet",   (unsigned int) u->stream_description.mBytesPerPacket);
        pa_log_debug("%d frames per packet",  (unsigned int) u->stream_description.mFramesPerPacket);
        pa_log_debug("%d bytes per frame",    (unsigned int) u->stream_description.mBytesPerFrame);
        pa_log_debug("%d channels per frame", (unsigned int) u->stream_description.mChannelsPerFrame);
        pa_log_debug("%d bits per channel",   (unsigned int) u->stream_description.mBitsPerChannel);

        for (channel_idx = 0, i = 0; i < buffer_list->mNumberBuffers; i++) {
            AudioBuffer *buf = buffer_list->mBuffers + i;

            if (direction_in)
                ca_device_create_source(m, buf, channel_idx);
            else
                ca_device_create_sink(m, buf, channel_idx);

            channel_idx += buf->mNumberChannels;
        }
    }

    pa_xfree(buffer_list);
    return 0;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);
    pa_assert(u->module);
    pa_assert(u->module->core);

    pa_log_debug("Thread starting up");

    if (u->module->core->realtime_scheduling)
        pa_make_realtime(u->module->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    for (;;) {
        coreaudio_sink *ca_sink;
        int ret;

        PA_LLIST_FOREACH(ca_sink, u->sinks) {
            if (PA_UNLIKELY(ca_sink->pa_sink->thread_info.rewind_requested))
                pa_sink_process_rewind(ca_sink->pa_sink, 0);
        }

        ret = pa_rtpoll_run(u->rtpoll);

        if (ret < 0)
            goto fail;

        if (ret == 0)
            goto finish;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->module->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

int pa__init(pa_module *m) {
    OSStatus err;
    UInt32 size, frames;
    struct userdata *u = NULL;
    pa_modargs *ma = NULL;
    char tmp[64];
    pa_card_new_data card_new_data;
    pa_card_profile *p;
    coreaudio_sink *ca_sink;
    coreaudio_source *ca_source;
    AudioObjectPropertyAddress property_address;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;

    if (pa_modargs_get_value_u32(ma, "object_id", (unsigned int *) &u->object_id) != 0) {
        pa_log("Failed to parse object_id argument.");
        goto fail;
    }

    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    /* get device product name */
    property_address.mSelector = kAudioDevicePropertyDeviceName;
    size = sizeof(tmp);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, tmp);
    if (err) {
        pa_log("Failed to get kAudioDevicePropertyDeviceName (err = %08x).", (int) err);
        goto fail;
    }

    u->device_name = pa_xstrdup(tmp);

    pa_card_new_data_init(&card_new_data);
    pa_proplist_sets(card_new_data.proplist, PA_PROP_DEVICE_STRING, tmp);
    card_new_data.driver = __FILE__;
    pa_card_new_data_set_name(&card_new_data, tmp);
    pa_log_info("Initializing module for CoreAudio device '%s' (id %d)", tmp, (unsigned int) u->object_id);

    /* get device vendor name (may fail) */
    property_address.mSelector = kAudioDevicePropertyDeviceManufacturer;
    size = sizeof(tmp);
    err = AudioObjectGetPropertyData(u->object_id, &property_address, 0, NULL, &size, tmp);
    if (!err)
        u->vendor_name = pa_xstrdup(tmp);

    /* add on profile */
    p = pa_card_profile_new("on", _("On"), 0);
    pa_hashmap_put(card_new_data.profiles, p->name, p);

    /* create the card object */
    u->card = pa_card_new(m->core, &card_new_data);
    if (!u->card) {
        pa_log("Unable to create card.\n");
        goto fail;
    }

    pa_card_new_data_done(&card_new_data);
    u->card->userdata = u;
    u->card->set_profile = card_set_profile;

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);
    u->async_msgq = pa_asyncmsgq_new(0);
    pa_rtpoll_item_new_asyncmsgq_read(u->rtpoll, PA_RTPOLL_EARLY-1, u->async_msgq);

    PA_LLIST_HEAD_INIT(coreaudio_sink, u->sinks);

    /* create sinks */
    ca_device_create_streams(m, false);

    /* create sources */
    ca_device_create_streams(m, true);

    /* create the message thread */
    if (!(u->thread = pa_thread_new(u->device_name, thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    /* register notification callback for stream format changes */
    property_address.mSelector = kAudioDevicePropertyStreamFormat;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    AudioObjectAddPropertyListener(u->object_id, &property_address, ca_stream_format_changed, u);

    /* set number of frames in IOProc */
    frames = DEFAULT_FRAMES_PER_IOPROC;
    pa_modargs_get_value_u32(ma, "ioproc_frames", (unsigned int *) &frames);

    property_address.mSelector = kAudioDevicePropertyBufferFrameSize;
    AudioObjectSetPropertyData(u->object_id, &property_address, 0, NULL, sizeof(frames), &frames);
    pa_log_debug("%u frames per IOProc\n", (unsigned int) frames);

    /* create one ioproc for both directions */
    err = AudioDeviceCreateIOProcID(u->object_id, io_render_proc, u, &u->proc_id);
    if (err) {
        pa_log("AudioDeviceCreateIOProcID() failed (err = %08x\n).", (int) err);
        goto fail;
    }

    for (ca_sink = u->sinks; ca_sink; ca_sink = ca_sink->next)
        pa_sink_put(ca_sink->pa_sink);

    for (ca_source = u->sources; ca_source; ca_source = ca_source->next)
        pa_source_put(ca_source->pa_source);

    pa_modargs_free(ma);

    return 0;

fail:
    if (u)
        pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;
    coreaudio_sink *ca_sink;
    coreaudio_source *ca_source;
    AudioObjectPropertyAddress property_address;

    pa_assert(m);

    u = m->userdata;
    pa_assert(u);

    /* unlink sinks */
    for (ca_sink = u->sinks; ca_sink; ca_sink = ca_sink->next)
        if (ca_sink->pa_sink)
            pa_sink_unlink(ca_sink->pa_sink);

    /* unlink sources */
    for (ca_source = u->sources; ca_source; ca_source = ca_source->next)
        if (ca_source->pa_source)
            pa_source_unlink(ca_source->pa_source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
        pa_thread_mq_done(&u->thread_mq);
        pa_asyncmsgq_unref(u->async_msgq);
    }

    /* free sinks */
    for (ca_sink = u->sinks; ca_sink;) {
        coreaudio_sink *next = ca_sink->next;

        if (ca_sink->pa_sink)
            pa_sink_unref(ca_sink->pa_sink);

        pa_xfree(ca_sink->name);
        pa_xfree(ca_sink);
        ca_sink = next;
    }

    /* free sources */
    for (ca_source = u->sources; ca_source;) {
        coreaudio_source *next = ca_source->next;

        if (ca_source->pa_source)
            pa_source_unref(ca_source->pa_source);

        pa_xfree(ca_source->name);
        pa_xfree(ca_source);
        ca_source = next;
    }

    if (u->proc_id) {
        AudioDeviceStop(u->object_id, u->proc_id);
        AudioDeviceDestroyIOProcID(u->object_id, u->proc_id);
    }

    property_address.mSelector = kAudioDevicePropertyStreamFormat;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &property_address, ca_stream_format_changed, u);

    pa_xfree(u->device_name);
    pa_xfree(u->vendor_name);
    pa_rtpoll_free(u->rtpoll);
    pa_card_free(u->card);

    pa_xfree(u);
}
