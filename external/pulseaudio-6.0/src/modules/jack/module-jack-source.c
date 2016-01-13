/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering

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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <jack/jack.h>

#include <pulse/xmalloc.h>

#include <pulsecore/source.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>

#include "module-jack-source-symdef.h"

/* See module-jack-sink for a few comments how this module basically
 * works */

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("JACK Source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "source_name=<name for the source> "
        "source_properties=<properties for the source> "
        "server_name=<jack server name> "
        "client_name=<jack client name> "
        "channels=<number of channels> "
        "channel_map=<channel map> "
        "connect=<connect ports?>");

#define DEFAULT_SOURCE_NAME "jack_in"

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_source *source;

    unsigned channels;

    jack_port_t* port[PA_CHANNELS_MAX];
    jack_client_t *client;

    pa_thread_mq thread_mq;
    pa_asyncmsgq *jack_msgq;
    pa_rtpoll *rtpoll;
    pa_rtpoll_item *rtpoll_item;

    pa_thread *thread;

    jack_nframes_t saved_frame_time;
    bool saved_frame_time_valid;
};

static const char* const valid_modargs[] = {
    "source_name",
    "source_properties",
    "server_name",
    "client_name",
    "channels",
    "channel_map",
    "connect",
    NULL
};

enum {
    SOURCE_MESSAGE_POST = PA_SOURCE_MESSAGE_MAX,
    SOURCE_MESSAGE_ON_SHUTDOWN
};

static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {

        case SOURCE_MESSAGE_POST:

            /* Handle the new block from the JACK thread */
            pa_assert(chunk);
            pa_assert(chunk->length > 0);

            if (u->source->thread_info.state == PA_SOURCE_RUNNING)
                pa_source_post(u->source, chunk);

            u->saved_frame_time = (jack_nframes_t) offset;
            u->saved_frame_time_valid = true;

            return 0;

        case SOURCE_MESSAGE_ON_SHUTDOWN:
            pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
            return 0;

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            jack_latency_range_t r;
            jack_nframes_t l, ft, d;
            size_t n;

            /* This is the "worst-case" latency */
            jack_port_get_latency_range(u->port[0], JackCaptureLatency, &r);
            l = r.max;

            if (u->saved_frame_time_valid) {
                /* Adjust the worst case latency by the time that
                 * passed since we last handed data to JACK */

                ft = jack_frame_time(u->client);
                d = ft > u->saved_frame_time ? ft - u->saved_frame_time : 0;
                l += d;
            }

            /* Convert it to usec */
            n = l * pa_frame_size(&u->source->sample_spec);
            *((pa_usec_t*) data) = pa_bytes_to_usec(n, &u->source->sample_spec);

            return 0;
        }
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

static int jack_process(jack_nframes_t nframes, void *arg) {
    unsigned c;
    struct userdata *u = arg;
    const void *buffer[PA_CHANNELS_MAX];
    void *p;
    jack_nframes_t frame_time;
    pa_memchunk chunk;

    pa_assert(u);

    for (c = 0; c < u->channels; c++)
        pa_assert_se(buffer[c] = jack_port_get_buffer(u->port[c], nframes));

    /* We interleave the data and pass it on to the other RT thread */

    pa_memchunk_reset(&chunk);
    chunk.length = nframes * pa_frame_size(&u->source->sample_spec);
    chunk.memblock = pa_memblock_new(u->core->mempool, chunk.length);
    p = pa_memblock_acquire(chunk.memblock);
    pa_interleave(buffer, u->channels, p, sizeof(float), nframes);
    pa_memblock_release(chunk.memblock);

    frame_time = jack_frame_time(u->client);

    pa_asyncmsgq_post(u->jack_msgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_POST, NULL, frame_time, &chunk, NULL);

    pa_memblock_unref(chunk.memblock);

    return 0;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    for (;;) {
        int ret;

        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (ret == 0)
            goto finish;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

static void jack_error_func(const char*t) {
    char *s;

    s = pa_xstrndup(t, strcspn(t, "\n\r"));
    pa_log_warn("JACK error >%s<", s);
    pa_xfree(s);
}

static void jack_init(void *arg) {
    struct userdata *u = arg;

    pa_log_info("JACK thread starting up.");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority+4);
}

static void jack_shutdown(void* arg) {
    struct userdata *u = arg;

    pa_log_info("JACK thread shutting down..");
    pa_asyncmsgq_post(u->jack_msgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_ON_SHUTDOWN, NULL, 0, NULL, NULL);
}

int pa__init(pa_module*m) {
    struct userdata *u = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    jack_status_t status;
    const char *server_name, *client_name;
    uint32_t channels = 0;
    bool do_connect = true;
    unsigned i;
    const char **ports = NULL, **p;
    pa_source_new_data data;
    jack_latency_range_t r;
    size_t n;

    pa_assert(m);

    jack_set_error_function(jack_error_func);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "connect", &do_connect) < 0) {
        pa_log("Failed to parse connect= argument.");
        goto fail;
    }

    server_name = pa_modargs_get_value(ma, "server_name", NULL);
    client_name = pa_modargs_get_value(ma, "client_name", "PulseAudio JACK Source");

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->saved_frame_time_valid = false;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    u->jack_msgq = pa_asyncmsgq_new(0);
    u->rtpoll_item = pa_rtpoll_item_new_asyncmsgq_read(u->rtpoll, PA_RTPOLL_EARLY-1, u->jack_msgq);

    if (!(u->client = jack_client_open(client_name, server_name ? JackServerName : JackNullOption, &status, server_name))) {
        pa_log("jack_client_open() failed.");
        goto fail;
    }

    ports = jack_get_ports(u->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical|JackPortIsOutput);

    channels = 0;
    if (ports)
        for (p = ports; *p; p++)
            channels++;

    if (!channels)
        channels = m->core->default_sample_spec.channels;

    if (pa_modargs_get_value_u32(ma, "channels", &channels) < 0 ||
        !pa_channels_valid(channels)) {
        pa_log("failed to parse channels= argument.");
        goto fail;
    }

    if (channels == m->core->default_channel_map.channels)
        map = m->core->default_channel_map;
    else
        pa_channel_map_init_extend(&map, channels, PA_CHANNEL_MAP_ALSA);

    if (pa_modargs_get_channel_map(ma, NULL, &map) < 0 || map.channels != channels) {
        pa_log("failed to parse channel_map= argument.");
        goto fail;
    }

    pa_log_info("Successfully connected as '%s'", jack_get_client_name(u->client));

    u->channels = ss.channels = (uint8_t) channels;
    ss.rate = jack_get_sample_rate(u->client);
    ss.format = PA_SAMPLE_FLOAT32NE;

    pa_assert(pa_sample_spec_valid(&ss));

    for (i = 0; i < ss.channels; i++) {
        if (!(u->port[i] = jack_port_register(u->client, pa_channel_position_to_string(map.map[i]), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput|JackPortIsTerminal, 0))) {
            pa_log("jack_port_register() failed.");
            goto fail;
        }
    }

    pa_source_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_source_new_data_set_name(&data, pa_modargs_get_value(ma, "source_name", DEFAULT_SOURCE_NAME));
    pa_source_new_data_set_sample_spec(&data, &ss);
    pa_source_new_data_set_channel_map(&data, &map);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_API, "jack");
    if (server_name)
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, server_name);
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Jack source (%s)", jack_get_client_name(u->client));
    pa_proplist_sets(data.proplist, "jack.client_name", jack_get_client_name(u->client));

    if (pa_modargs_get_proplist(ma, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&data);
        goto fail;
    }

    u->source = pa_source_new(m->core, &data, PA_SOURCE_LATENCY);
    pa_source_new_data_done(&data);

    if (!u->source) {
        pa_log("Failed to create source.");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg;
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    jack_set_process_callback(u->client, jack_process, u);
    jack_on_shutdown(u->client, jack_shutdown, u);
    jack_set_thread_init_callback(u->client, jack_init, u);

    if (!(u->thread = pa_thread_new("jack-source", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    if (jack_activate(u->client)) {
        pa_log("jack_activate() failed");
        goto fail;
    }

    if (do_connect) {
        for (i = 0, p = ports; i < ss.channels; i++, p++) {

            if (!p || !*p) {
                pa_log("Not enough physical output ports, leaving unconnected.");
                break;
            }

            pa_log_info("Connecting %s to %s", jack_port_name(u->port[i]), *p);

            if (jack_connect(u->client, *p, jack_port_name(u->port[i]))) {
                pa_log("Failed to connect %s to %s, leaving unconnected.", jack_port_name(u->port[i]), *p);
                break;
            }
        }

    }

    jack_port_get_latency_range(u->port[0], JackCaptureLatency, &r);
    n = r.max * pa_frame_size(&u->source->sample_spec);
    pa_source_set_fixed_latency(u->source, pa_bytes_to_usec(n, &u->source->sample_spec));
    pa_source_put(u->source);

    if (ports)
        jack_free(ports);
    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (ports)
        jack_free(ports);

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_source_linked_by(u->source);
}

void pa__done(pa_module*m) {
    struct userdata *u;
    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->source)
        pa_source_unlink(u->source);

    if (u->client)
        jack_client_close(u->client);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->source)
        pa_source_unref(u->source);

    if (u->rtpoll_item)
        pa_rtpoll_item_free(u->rtpoll_item);

    if (u->jack_msgq)
        pa_asyncmsgq_unref(u->jack_msgq);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    pa_xfree(u);
}
