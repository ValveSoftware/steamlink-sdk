/***
    This file is part of PulseAudio.

    Copyright 2013 bct electronic GmbH
    Contributor: Stefan Huber <s.huber@bct-electronic.com>

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
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/ltdl-helper.h>
#include <pulsecore/mix.h>

#include "module-remap-source-symdef.h"

PA_MODULE_AUTHOR("Stefan Huber");
PA_MODULE_DESCRIPTION("Virtual channel remapping source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "source_name=<name for the source> "
        "source_properties=<properties for the source> "
        "master=<name of source to filter> "
        "master_channel_map=<channel map> "
        "format=<sample format> "
        "rate=<sample rate> "
        "channels=<number of channels> "
        "channel_map=<channel map> "
        "resample_method=<resampler> "
        "remix=<remix channels?>");

struct userdata {
    pa_module *module;

    pa_source *source;
    pa_source_output *source_output;

    bool auto_desc;
};

static const char* const valid_modargs[] = {
    "source_name",
    "source_properties",
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

    pa_log_debug("Source update requested latency.");

    /* Just hand this one over to the master source */
    pa_source_output_set_requested_latency_within_thread(
            u->source_output,
            pa_source_get_requested_latency_within_thread(s));
}

/* Called from output thread context */
static void source_output_push_cb(pa_source_output *o, const pa_memchunk *chunk) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(u->source_output))) {
        pa_log("push when no link?");
        return;
    }

    pa_source_post(u->source, chunk);
}

/* Called from output thread context */
static void source_output_process_rewind_cb(pa_source_output *o, size_t nbytes) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_source_process_rewind(u->source, nbytes);
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

/* Called from output thread context */
static void source_output_state_change_cb(pa_source_output *o, pa_source_output_state_t state) {
    struct userdata *u;

    pa_source_output_assert_ref(o);
    pa_source_output_assert_io_context(o);
    pa_assert_se(u = o->userdata);

    pa_log_debug("Source output %d state %d.", o->index, state);
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
        const char *k;
        pa_proplist *pl;

        pl = pa_proplist_new();
        k = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : dest->name);

        pa_source_update_proplist(u->source, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss;
    pa_resample_method_t resample_method = PA_RESAMPLER_INVALID;
    pa_channel_map source_map, stream_map;
    pa_modargs *ma;
    pa_source *master;
    pa_source_output_new_data source_output_data;
    pa_source_new_data source_data;
    bool remix = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SOURCE))) {
        pa_log("Master source not found.");
        goto fail;
    }

    ss = master->sample_spec;
    source_map = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &source_map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map.");
        goto fail;
    }

    stream_map = source_map;
    if (pa_modargs_get_channel_map(ma, "master_channel_map", &stream_map) < 0) {
        pa_log("Invalid master channel map.");
        goto fail;
    }

    if (stream_map.channels != ss.channels) {
        pa_log("Number of channels doesn't match.");
        goto fail;
    }

    if (pa_channel_map_equal(&stream_map, &master->channel_map))
        pa_log_warn("No remapping configured, proceeding nonetheless!");

    if (pa_modargs_get_value_boolean(ma, "remix", &remix) < 0) {
        pa_log("Invalid boolean remix parameter.");
        goto fail;
    }

    if (pa_modargs_get_resample_method(ma, &resample_method) < 0) {
        pa_log("Invalid resampling method");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;

    /* Create source */
    pa_source_new_data_init(&source_data);
    source_data.driver = __FILE__;
    source_data.module = m;
    if (!(source_data.name = pa_xstrdup(pa_modargs_get_value(ma, "source_name", NULL))))
        source_data.name = pa_sprintf_malloc("%s.remapped", master->name);
    pa_source_new_data_set_sample_spec(&source_data, &ss);
    pa_source_new_data_set_channel_map(&source_data, &source_map);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_CLASS, "filter");

    if (pa_modargs_get_proplist(ma, "source_properties", source_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties.");
        pa_source_new_data_done(&source_data);
        goto fail;
    }

    if ((u->auto_desc = !pa_proplist_contains(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *k;

        k = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(source_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : master->name);
    }

    u->source = pa_source_new(m->core, &source_data, master->flags & (PA_SOURCE_LATENCY|PA_SOURCE_DYNAMIC_LATENCY));
    pa_source_new_data_done(&source_data);

    if (!u->source) {
        pa_log("Failed to create source.");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg_cb;
    u->source->set_state = source_set_state_cb;
    u->source->update_requested_latency = source_update_requested_latency_cb;

    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, master->asyncmsgq);

    /* Create source output */
    pa_source_output_new_data_init(&source_output_data);
    source_output_data.driver = __FILE__;
    source_output_data.module = m;
    pa_source_output_new_data_set_source(&source_output_data, master, false);
    source_output_data.destination_source = u->source;

    pa_proplist_sets(source_output_data.proplist, PA_PROP_MEDIA_NAME, "Remapped Stream");
    pa_proplist_sets(source_output_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_source_output_new_data_set_sample_spec(&source_output_data, &ss);
    pa_source_output_new_data_set_channel_map(&source_output_data, &stream_map);
    source_output_data.flags = remix ? 0 : PA_SOURCE_OUTPUT_NO_REMIX;
    source_output_data.resample_method = resample_method;

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

    pa_xfree(u);
}
