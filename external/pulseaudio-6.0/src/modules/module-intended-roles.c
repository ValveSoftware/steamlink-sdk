/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/namereg.h>

#include "module-intended-roles-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Automatically set device of streams based on intended roles of devices");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "on_hotplug=<When new device becomes available, recheck streams?> "
        "on_rescue=<When device becomes unavailable, recheck streams?>");

static const char* const valid_modargs[] = {
    "on_hotplug",
    "on_rescue",
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_hook_slot
        *sink_input_new_hook_slot,
        *source_output_new_hook_slot,
        *sink_put_hook_slot,
        *source_put_hook_slot,
        *sink_unlink_hook_slot,
        *source_unlink_hook_slot;

    bool on_hotplug:1;
    bool on_rescue:1;
};

static bool role_match(pa_proplist *proplist, const char *role) {
    return pa_str_in_list_spaces(pa_proplist_gets(proplist, PA_PROP_DEVICE_INTENDED_ROLES), role);
}

static pa_hook_result_t sink_input_new_hook_callback(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u) {
    const char *role;
    pa_sink *s, *def;
    uint32_t idx;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!new_data->proplist) {
        pa_log_debug("New stream lacks property data.");
        return PA_HOOK_OK;
    }

    if (new_data->sink) {
        pa_log_debug("Not setting device for stream %s, because already set.", pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    if (!(role = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_ROLE))) {
        pa_log_debug("Not setting device for stream %s, because it lacks role.", pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    /* Prefer the default sink over any other sink, just in case... */
    if ((def = pa_namereg_get_default_sink(c)))
        if (role_match(def->proplist, role) && pa_sink_input_new_data_set_sink(new_data, def, false))
            return PA_HOOK_OK;

    /* @todo: favour the highest priority device, not the first one we find? */
    PA_IDXSET_FOREACH(s, c->sinks, idx) {
        if (s == def)
            continue;

        if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)))
            continue;

        if (role_match(s->proplist, role) && pa_sink_input_new_data_set_sink(new_data, s, false))
            return PA_HOOK_OK;
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_new_hook_callback(pa_core *c, pa_source_output_new_data *new_data, struct userdata *u) {
    const char *role;
    pa_source *s, *def;
    uint32_t idx;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!new_data->proplist) {
        pa_log_debug("New stream lacks property data.");
        return PA_HOOK_OK;
    }

    if (new_data->source) {
        pa_log_debug("Not setting device for stream %s, because already set.", pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    if (!(role = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_ROLE))) {
        pa_log_debug("Not setting device for stream %s, because it lacks role.", pa_strnull(pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_NAME)));
        return PA_HOOK_OK;
    }

    /* Prefer the default source over any other source, just in case... */
    if ((def = pa_namereg_get_default_source(c)))
        if (role_match(def->proplist, role)) {
            pa_source_output_new_data_set_source(new_data, def, false);
            return PA_HOOK_OK;
        }

    PA_IDXSET_FOREACH(s, c->sources, idx) {
        if (s->monitor_of)
            continue;

        if (s == def)
            continue;

        if (!PA_SOURCE_IS_LINKED(pa_source_get_state(s)))
            continue;

        /* @todo: favour the highest priority device, not the first one we find? */
        if (role_match(s->proplist, role)) {
            pa_source_output_new_data_set_source(new_data, s, false);
            return PA_HOOK_OK;
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->on_hotplug);

    PA_IDXSET_FOREACH(si, c->sink_inputs, idx) {
        const char *role;

        if (si->sink == sink)
            continue;

        if (si->save_sink)
            continue;

        /* Skip this if it is already in the process of being moved
         * anyway */
        if (!si->sink)
            continue;

        /* It might happen that a stream and a sink are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (!PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(si)))
            continue;

        if (!(role = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        if (role_match(si->sink->proplist, role))
            continue;

        if (!role_match(sink->proplist, role))
            continue;

        pa_sink_input_move_to(si, sink, false);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_put_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    pa_source_output *so;
    uint32_t idx;

    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->on_hotplug);

    if (source->monitor_of)
        return PA_HOOK_OK;

    PA_IDXSET_FOREACH(so, c->source_outputs, idx) {
        const char *role;

        if (so->source == source)
            continue;

        if (so->save_source)
            continue;

        if (so->direct_on_input)
            continue;

        /* Skip this if it is already in the process of being moved
         * anyway */
        if (!so->source)
            continue;

        /* It might happen that a stream and a source are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(so)))
            continue;

        if (!(role = pa_proplist_gets(so->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        if (role_match(so->source->proplist, role))
            continue;

        if (!role_match(source->proplist, role))
            continue;

        pa_source_output_move_to(so, source, false);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_unlink_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    pa_sink_input *si;
    uint32_t idx;
    pa_sink *def;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->on_rescue);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    /* If there not default sink, then there is no sink at all */
    if (!(def = pa_namereg_get_default_sink(c)))
        return PA_HOOK_OK;

    PA_IDXSET_FOREACH(si, sink->inputs, idx) {
        const char *role;
        uint32_t jdx;
        pa_sink *d;

        if (!si->sink)
            continue;

        if (!(role = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        /* Would the default sink fit? If so, let's use it */
        if (def != sink && role_match(def->proplist, role))
            if (pa_sink_input_move_to(si, def, false) >= 0)
                continue;

        /* Try to find some other fitting sink */
        /* @todo: favour the highest priority device, not the first one we find? */
        PA_IDXSET_FOREACH(d, c->sinks, jdx) {
            if (d == def || d == sink)
                continue;

            if (!PA_SINK_IS_LINKED(pa_sink_get_state(d)))
                continue;

            if (role_match(d->proplist, role))
                if (pa_sink_input_move_to(si, d, false) >= 0)
                    break;
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_unlink_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    pa_source_output *so;
    uint32_t idx;
    pa_source *def;

    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->on_rescue);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    /* If there not default source, then there is no source at all */
    if (!(def = pa_namereg_get_default_source(c)))
        return PA_HOOK_OK;

    PA_IDXSET_FOREACH(so, source->outputs, idx) {
        const char *role;
        uint32_t jdx;
        pa_source *d;

        if (so->direct_on_input)
            continue;

        if (!so->source)
            continue;

        if (!(role = pa_proplist_gets(so->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        /* Would the default source fit? If so, let's use it */
        if (def != source && role_match(def->proplist, role) && !source->monitor_of == !def->monitor_of) {
            pa_source_output_move_to(so, def, false);
            continue;
        }

        /* Try to find some other fitting source */
        /* @todo: favour the highest priority device, not the first one we find? */
        PA_IDXSET_FOREACH(d, c->sources, jdx) {
            if (d == def || d == source)
                continue;

            if (!PA_SOURCE_IS_LINKED(pa_source_get_state(d)))
                continue;

            /* If moving from a monitor, move to another monitor */
            if (!source->monitor_of == !d->monitor_of && role_match(d->proplist, role)) {
                pa_source_output_move_to(so, d, false);
                break;
            }
        }
    }

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    bool on_hotplug = true, on_rescue = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "on_hotplug", &on_hotplug) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_rescue", &on_rescue) < 0) {
        pa_log("on_hotplug= and on_rescue= expect boolean arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->on_hotplug = on_hotplug;
    u->on_rescue = on_rescue;

    /* A little bit later than module-stream-restore */
    u->sink_input_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_NEW], PA_HOOK_EARLY+10, (pa_hook_cb_t) sink_input_new_hook_callback, u);
    u->source_output_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_NEW], PA_HOOK_EARLY+10, (pa_hook_cb_t) source_output_new_hook_callback, u);

    if (on_hotplug) {
        /* A little bit later than module-stream-restore */
        u->sink_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE+10, (pa_hook_cb_t) sink_put_hook_callback, u);
        u->source_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE+10, (pa_hook_cb_t) source_put_hook_callback, u);
    }

    if (on_rescue) {
        /* A little bit later than module-stream-restore, a little bit earlier than module-rescue-streams, ... */
        u->sink_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE+10, (pa_hook_cb_t) sink_unlink_hook_callback, u);
        u->source_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE+10, (pa_hook_cb_t) source_unlink_hook_callback, u);
    }

    pa_modargs_free(ma);
    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_input_new_hook_slot)
        pa_hook_slot_free(u->sink_input_new_hook_slot);
    if (u->source_output_new_hook_slot)
        pa_hook_slot_free(u->source_output_new_hook_slot);

    if (u->sink_put_hook_slot)
        pa_hook_slot_free(u->sink_put_hook_slot);
    if (u->source_put_hook_slot)
        pa_hook_slot_free(u->source_put_hook_slot);

    if (u->sink_unlink_hook_slot)
        pa_hook_slot_free(u->sink_unlink_hook_slot);
    if (u->source_unlink_hook_slot)
        pa_hook_slot_free(u->source_unlink_hook_slot);

    pa_xfree(u);
}
