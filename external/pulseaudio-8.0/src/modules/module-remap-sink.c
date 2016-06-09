/***
  This file is part of PulseAudio.

  Copyright 2004-2009 Lennart Poettering

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

#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>

#include "module-remap-sink-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Virtual channel remapping sink");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "master=<name of sink to remap> "
        "master_channel_map=<channel map> "
        "format=<sample format> "
        "rate=<sample rate> "
        "channels=<number of channels> "
        "channel_map=<channel map> "
        "resample_method=<resampler> "
        "remix=<remix channels?>");

struct userdata {
    pa_module *module;

    pa_sink *sink;
    pa_sink_input *sink_input;

    bool auto_desc;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "master",
    "master_channel_map",
    "format",
    "rate",
    "channels",
    "channel_map",
    "resample_method",
    "remix",
    NULL
};

/* Called from I/O thread context */
static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY:

            /* The sink is _put() before the sink input is, so let's
             * make sure we don't access it yet */
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
static int sink_set_state(pa_sink *s, pa_sink_state_t state) {
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
static void sink_request_rewind(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    pa_sink_input_request_rewind(u->sink_input, s->thread_info.rewind_nbytes, true, false, false);
}

/* Called from I/O thread context */
static void sink_update_requested_latency(pa_sink *s) {
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

/* Called from I/O thread context */
static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert(chunk);
    pa_assert_se(u = i->userdata);

    /* Hmm, process any rewind request that might be queued up */
    pa_sink_process_rewind(u->sink, 0);

    pa_sink_render(u->sink, nbytes, chunk);
    return 0;
}

/* Called from I/O thread context */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    size_t amount = 0;
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->sink->thread_info.rewind_nbytes > 0) {
        amount = PA_MIN(u->sink->thread_info.rewind_nbytes, nbytes);
        u->sink->thread_info.rewind_nbytes = 0;
    }

    pa_sink_process_rewind(u->sink, amount);
}

/* Called from I/O thread context */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_max_request_within_thread(u->sink, nbytes);
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
    pa_sink_set_max_request_within_thread(u->sink, pa_sink_input_get_max_request(i));

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i));

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
        const char *k;
        pa_proplist *pl;

        pl = pa_proplist_new();
        k = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : dest->name);

        pa_sink_update_proplist(u->sink, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss;
    pa_resample_method_t resample_method = PA_RESAMPLER_INVALID;
    pa_channel_map sink_map, stream_map;
    pa_modargs *ma;
    pa_sink *master;
    pa_sink_input_new_data sink_input_data;
    pa_sink_new_data sink_data;
    bool remix = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }

    ss = master->sample_spec;
    sink_map = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &sink_map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    stream_map = sink_map;
    if (pa_modargs_get_channel_map(ma, "master_channel_map", &stream_map) < 0) {
        pa_log("Invalid master channel map");
        goto fail;
    }

    if (stream_map.channels != ss.channels) {
        pa_log("Number of channels doesn't match");
        goto fail;
    }

    if (pa_channel_map_equal(&stream_map, &master->channel_map))
        pa_log_warn("No remapping configured, proceeding nonetheless!");

    if (pa_modargs_get_value_boolean(ma, "remix", &remix) < 0) {
        pa_log("Invalid boolean remix parameter");
        goto fail;
    }

    if (pa_modargs_get_resample_method(ma, &resample_method) < 0) {
        pa_log("Invalid resampling method");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if (!(sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sink_data.name = pa_sprintf_malloc("%s.remapped", master->name);
    pa_sink_new_data_set_sample_spec(&sink_data, &ss);
    pa_sink_new_data_set_channel_map(&sink_data, &sink_map);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "filter");

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    if ((u->auto_desc = !pa_proplist_contains(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *k;

        k = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : master->name);
    }

    u->sink = pa_sink_new(m->core, &sink_data, master->flags & (PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY));
    pa_sink_new_data_done(&sink_data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg;
    u->sink->set_state = sink_set_state;
    u->sink->update_requested_latency = sink_update_requested_latency;
    u->sink->request_rewind = sink_request_rewind;
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);

    /* Create sink input */
    pa_sink_input_new_data_init(&sink_input_data);
    sink_input_data.driver = __FILE__;
    sink_input_data.module = m;
    pa_sink_input_new_data_set_sink(&sink_input_data, master, false);
    sink_input_data.origin_sink = u->sink;
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_NAME, "Remapped Stream");
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sink_input_data, &ss);
    pa_sink_input_new_data_set_channel_map(&sink_input_data, &stream_map);
    sink_input_data.flags = (remix ? 0 : PA_SINK_INPUT_NO_REMIX);
    sink_input_data.resample_method = resample_method;

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
    u->sink_input->attach = sink_input_attach_cb;
    u->sink_input->detach = sink_input_detach_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->moving = sink_input_moving_cb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;

    pa_sink_put(u->sink);
    pa_sink_input_put(u->sink_input);

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

    pa_xfree(u);
}
