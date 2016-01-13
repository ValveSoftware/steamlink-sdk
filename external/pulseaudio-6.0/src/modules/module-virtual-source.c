/***
    This file is part of PulseAudio.

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

#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/macro.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/ltdl-helper.h>
#include <pulsecore/mix.h>

#include "module-virtual-source-symdef.h"

PA_MODULE_AUTHOR("Pierre-Louis Bossart");
PA_MODULE_DESCRIPTION("Virtual source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        _("source_name=<name for the source> "
          "source_properties=<properties for the source> "
          "master=<name of source to filter> "
          "uplink_sink=<name> (optional)"
          "format=<sample format> "
          "rate=<sample rate> "
          "channels=<number of channels> "
          "channel_map=<channel map> "
          "use_volume_sharing=<yes or no> "
          "force_flat_volume=<yes or no> "
        ));

#define MEMBLOCKQ_MAXLENGTH (16*1024*1024)
#define BLOCK_USEC 1000 /* FIXME */

struct userdata {
    pa_module *module;

    /* FIXME: Uncomment this and take "autoloaded" as a modarg if this is a filter */
    /* bool autoloaded; */

    pa_source *source;
    pa_source_output *source_output;

    pa_memblockq *memblockq;

    bool auto_desc;
    unsigned channels;

    /* optional fields for uplink sink */
    pa_sink *sink;
    pa_usec_t block_usec;
    pa_memblockq *sink_memblockq;

};

static const char* const valid_modargs[] = {
    "source_name",
    "source_properties",
    "master",
    "uplink_sink",
    "format",
    "rate",
    "channels",
    "channel_map",
    "use_volume_sharing",
    "force_flat_volume",
    NULL
};

/* Called from I/O thread context */
static int sink_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY:

            /* there's no real latency here */
            *((pa_usec_t*) data) = 0;

            return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int sink_set_state_cb(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state)) {
        return 0;
    }

    if (state == PA_SINK_RUNNING) {
        /* need to wake-up source if it was suspended */
        pa_log_debug("Resuming source %s, because its uplink sink became active.", u->source->name);
        pa_source_suspend(u->source, false, PA_SUSPEND_ALL);

        /* FIXME: if there's no client connected, the source will suspend
           and playback will be stuck. You'd want to prevent the source from
           sleeping when the uplink sink is active; even if the audio is
           discarded at least the app isn't stuck */

    } else {
        /* nothing to do, if the sink becomes idle or suspended let
           module-suspend-idle handle the sources later */
    }

    return 0;
}

static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    /* FIXME: there's no latency support */

}

/* Called from I/O thread context */
static void sink_request_rewind_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    /* Do nothing */
    pa_sink_process_rewind(u->sink, 0);

}

/* Called from I/O thread context */
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
                /* FIXME, no idea what I am doing here */
                pa_bytes_to_usec(pa_memblockq_get_length(u->source_output->thread_info.delay_memblockq), &u->source_output->source->sample_spec);

            return 0;
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int source_set_state_cb(pa_source *s, pa_source_state_t state) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(state) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return 0;

    pa_source_output_cork(u->source_output, state == PA_SOURCE_SUSPENDED);
    return 0;
}

/* Called from I/O thread context */
static void source_update_requested_latency_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(u->source->thread_info.state) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(u->source_output->thread_info.state))
        return;

    /* Just hand this one over to the master source */
    pa_source_output_set_requested_latency_within_thread(
            u->source_output,
            pa_source_get_requested_latency_within_thread(s));
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
static void source_set_mute_cb(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SOURCE_IS_LINKED(pa_source_get_state(s)) ||
        !PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output)))
        return;

    pa_source_output_set_mute(u->source_output, s->muted, s->save_muted);
}

/* Called from input thread context */
static void source_output_push_cb(pa_source_output *o, const pa_memchunk *chunk) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output))) {
        pa_log("push when no link?");
        return;
    }

    /* PUT YOUR CODE HERE TO DO SOMETHING WITH THE SOURCE DATA */

    /* if uplink sink exists, pull data from there; simplify by using
       same length as chunk provided by source */
    if (u->sink && (pa_sink_get_state(u->sink) == PA_SINK_RUNNING)) {
        pa_memchunk tchunk;
        size_t nbytes = chunk->length;
        pa_mix_info streams[2];
        pa_memchunk target_chunk;
        void *target;
        int ch;

        /* Hmm, process any rewind request that might be queued up */
        pa_sink_process_rewind(u->sink, 0);

        /* get data from the sink */
        while (pa_memblockq_peek(u->sink_memblockq, &tchunk) < 0) {
            pa_memchunk nchunk;

            /* make sure we get nbytes from the sink with render_full,
               otherwise we cannot mix with the uplink */
            pa_sink_render_full(u->sink, nbytes, &nchunk);
            pa_memblockq_push(u->sink_memblockq, &nchunk);
            pa_memblock_unref(nchunk.memblock);
        }
        pa_assert(tchunk.length == chunk->length);

        /* move the read pointer for sink memblockq */
        pa_memblockq_drop(u->sink_memblockq, tchunk.length);

        /* allocate target chunk */
        /* this could probably be done in-place, but having chunk as both
           the input and output creates issues with reference counts */
        target_chunk.index = 0;
        target_chunk.length = chunk->length;
        pa_assert(target_chunk.length == chunk->length);

        target_chunk.memblock = pa_memblock_new(o->source->core->mempool,
                                                target_chunk.length);
        pa_assert( target_chunk.memblock );

        /* get target pointer */
        target = pa_memblock_acquire_chunk(&target_chunk);

        /* set-up mixing structure
           volume was taken care of in sink and source already */
        streams[0].chunk = *chunk;
        for(ch=0;ch<o->sample_spec.channels;ch++)
            streams[0].volume.values[ch] = PA_VOLUME_NORM; /* FIXME */
        streams[0].volume.channels = o->sample_spec.channels;

        streams[1].chunk = tchunk;
        for(ch=0;ch<o->sample_spec.channels;ch++)
            streams[1].volume.values[ch] = PA_VOLUME_NORM; /* FIXME */
        streams[1].volume.channels = o->sample_spec.channels;

        /* do mixing */
        pa_mix(streams,                /* 2 streams to be mixed */
               2,
               target,                 /* put result in target chunk */
               chunk->length,          /* same length as input */
               (const pa_sample_spec *)&o->sample_spec, /* same sample spec for input and output */
               NULL,                   /* no volume information */
               false);                 /* no mute */

        pa_memblock_release(target_chunk.memblock);
        pa_memblock_unref(tchunk.memblock); /* clean-up */

        /* forward the data to the virtual source */
        pa_source_post(u->source, &target_chunk);

        pa_memblock_unref(target_chunk.memblock); /* clean-up */

    } else {
        /* forward the data to the virtual source */
        pa_source_post(u->source, chunk);
    }

}

/* Called from input thread context */
static void source_output_process_rewind_cb(pa_source_output *o, size_t nbytes) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    /* FIXME, no idea what I am doing here */
#if 0
    pa_asyncmsgq_post(u->asyncmsgq, PA_MSGOBJECT(u->sink_input), SINK_INPUT_MESSAGE_REWIND, NULL, (int64_t) nbytes, NULL, NULL);
    u->send_counter -= (int64_t) nbytes;
#endif
}

/* Called from output thread context */
static void source_output_attach_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_set_rtpoll(u->source, o->source->thread_info.rtpoll);
    pa_source_set_latency_range_within_thread(u->source, o->source->thread_info.min_latency, o->source->thread_info.max_latency);
    pa_source_set_fixed_latency_within_thread(u->source, o->source->thread_info.fixed_latency);
    pa_source_set_max_rewind_within_thread(u->source, pa_source_output_get_max_rewind(o));

    pa_source_attach_within_thread(u->source);
}

/* Called from output thread context */
static void source_output_detach_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_detach_within_thread(u->source);
    pa_source_set_rtpoll(u->source, NULL);
}

/* Called from output thread context */
static void source_output_state_change_cb(pa_source_output *o, pa_source_output_state_t state) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    /* FIXME */
#if 0
    if (PA_SOURCE_OUTPUT_IS_LINKED(state) && o->thread_info.state == PA_SOURCE_OUTPUT_INIT) {

        u->skip = pa_usec_to_bytes(PA_CLIP_SUB(pa_source_get_latency_within_thread(o->source),
                                               u->latency),
                                   &o->sample_spec);

        pa_log_info("Skipping %lu bytes", (unsigned long) u->skip);
    }
#endif
}

/* Called from main thread */
static void source_output_kill_cb(pa_source_output *o) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_assert_ctl_context();
    pa_assert_se(u = o->userdata);

    /* The order here matters! We first kill the source output, followed
     * by the source. That means the source callbacks must be protected
     * against an unconnected source output! */
    pa_source_output_unlink(u->source_output);
    pa_source_unlink(u->source);

    pa_source_output_unref(u->source_output);
    u->source_output = NULL;

    pa_source_unref(u->source);
    u->source = NULL;

    pa_module_unload_request(u->module, true);
}

/* Called from main thread */
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

    if (u->auto_desc && dest) {
        const char *z;
        pa_proplist *pl;

        pl = pa_proplist_new();
        z = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "Virtual Source %s on %s",
                         pa_proplist_gets(u->source->proplist, "device.vsource.name"), z ? z : dest->name);

        pa_source_update_proplist(u->source, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma;
    pa_source *master=NULL;
    pa_source_output_new_data source_output_data;
    pa_source_new_data source_data;
    bool use_volume_sharing = true;
    bool force_flat_volume = false;

    /* optional for uplink_sink */
    pa_sink_new_data sink_data;
    size_t nbytes;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SOURCE))) {
        pa_log("Master source not found");
        goto fail;
    }

    pa_assert(master);

    ss = master->sample_spec;
    ss.format = PA_SAMPLE_FLOAT32;
    map = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

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

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;
    u->memblockq = pa_memblockq_new("module-virtual-source memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0, &ss, 1, 1, 0, NULL);
    if (!u->memblockq) {
        pa_log("Failed to create source memblockq.");
        goto fail;
    }
    u->channels = ss.channels;

    /* Create source */
    pa_source_new_data_init(&source_data);
    source_data.driver = __FILE__;
    source_data.module = m;
    if (!(source_data.name = pa_xstrdup(pa_modargs_get_value(ma, "source_name", NULL))))
        source_data.name = pa_sprintf_malloc("%s.vsource", master->name);
    pa_source_new_data_set_sample_spec(&source_data, &ss);
    pa_source_new_data_set_channel_map(&source_data, &map);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_CLASS, "filter");
    pa_proplist_sets(source_data.proplist, "device.vsource.name", source_data.name);

    if (pa_modargs_get_proplist(ma, "source_properties", source_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&source_data);
        goto fail;
    }

    if ((u->auto_desc = !pa_proplist_contains(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *z;

        z = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Virtual Source %s on %s", source_data.name, z ? z : master->name);
    }

    u->source = pa_source_new(m->core, &source_data, (master->flags & (PA_SOURCE_LATENCY|PA_SOURCE_DYNAMIC_LATENCY))
                                                     | (use_volume_sharing ? PA_SOURCE_SHARE_VOLUME_WITH_MASTER : 0));

    pa_source_new_data_done(&source_data);

    if (!u->source) {
        pa_log("Failed to create source.");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg_cb;
    u->source->set_state = source_set_state_cb;
    u->source->update_requested_latency = source_update_requested_latency_cb;
    pa_source_set_set_mute_callback(u->source, source_set_mute_cb);
    if (!use_volume_sharing) {
        pa_source_set_set_volume_callback(u->source, source_set_volume_cb);
        pa_source_enable_decibel_volume(u->source, true);
    }
    /* Normally this flag would be enabled automatically be we can force it. */
    if (force_flat_volume)
        u->source->flags |= PA_SOURCE_FLAT_VOLUME;
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, master->asyncmsgq);

    /* Create source output */
    pa_source_output_new_data_init(&source_output_data);
    source_output_data.driver = __FILE__;
    source_output_data.module = m;
    pa_source_output_new_data_set_source(&source_output_data, master, false);
    source_output_data.destination_source = u->source;

    pa_proplist_setf(source_output_data.proplist, PA_PROP_MEDIA_NAME, "Virtual Source Stream of %s", pa_proplist_gets(u->source->proplist, PA_PROP_DEVICE_DESCRIPTION));
    pa_proplist_sets(source_output_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_source_output_new_data_set_sample_spec(&source_output_data, &ss);
    pa_source_output_new_data_set_channel_map(&source_output_data, &map);

    pa_source_output_new(&u->source_output, m->core, &source_output_data);
    pa_source_output_new_data_done(&source_output_data);

    if (!u->source_output)
        goto fail;

    u->source_output->push = source_output_push_cb;
    u->source_output->process_rewind = source_output_process_rewind_cb;
    u->source_output->kill = source_output_kill_cb;
    u->source_output->attach = source_output_attach_cb;
    u->source_output->detach = source_output_detach_cb;
    u->source_output->state_change = source_output_state_change_cb;
    u->source_output->moving = source_output_moving_cb;
    u->source_output->userdata = u;

    u->source->output_from_master = u->source_output;

    pa_source_put(u->source);
    pa_source_output_put(u->source_output);

    /* Create optional uplink sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if ((sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "uplink_sink", NULL)))) {
        pa_sink_new_data_set_sample_spec(&sink_data, &ss);
        pa_sink_new_data_set_channel_map(&sink_data, &map);
        pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
        pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "uplink sink");
        pa_proplist_sets(sink_data.proplist, "device.uplink_sink.name", sink_data.name);

        if ((u->auto_desc = !pa_proplist_contains(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
            const char *z;

            z = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
            pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Uplink Sink %s on %s", sink_data.name, z ? z : master->name);
        }

        u->sink_memblockq = pa_memblockq_new("module-virtual-source sink_memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0, &ss, 1, 1, 0, NULL);
        if (!u->sink_memblockq) {
            pa_sink_new_data_done(&sink_data);
            pa_log("Failed to create sink memblockq.");
            goto fail;
        }

        u->sink = pa_sink_new(m->core, &sink_data, 0);  /* FIXME, sink has no capabilities */
        pa_sink_new_data_done(&sink_data);

        if (!u->sink) {
            pa_log("Failed to create sink.");
            goto fail;
        }

        u->sink->parent.process_msg = sink_process_msg_cb;
        u->sink->update_requested_latency = sink_update_requested_latency_cb;
        u->sink->request_rewind = sink_request_rewind_cb;
        u->sink->set_state = sink_set_state_cb;
        u->sink->userdata = u;

        pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);

        /* FIXME: no idea what I am doing here */
        u->block_usec = BLOCK_USEC;
        nbytes = pa_usec_to_bytes(u->block_usec, &u->sink->sample_spec);
        pa_sink_set_max_rewind(u->sink, nbytes);
        pa_sink_set_max_request(u->sink, nbytes);

        pa_sink_put(u->sink);
    } else {
        pa_sink_new_data_done(&sink_data);
        /* optional uplink sink not enabled */
        u->sink = NULL;
    }

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

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

    /* See comments in source_output_kill_cb() above regarding
     * destruction order! */

    if (u->source_output)
        pa_source_output_unlink(u->source_output);

    if (u->source)
        pa_source_unlink(u->source);

    if (u->source_output)
        pa_source_output_unref(u->source_output);

    if (u->source)
        pa_source_unref(u->source);

    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->memblockq)
        pa_memblockq_free(u->memblockq);

    if (u->sink_memblockq)
        pa_memblockq_free(u->sink_memblockq);

    pa_xfree(u);
}
