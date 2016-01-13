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

#include <pulse/xmalloc.h>

#include <pulsecore/core.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-util.h>

#include "module-rescue-streams-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("When a sink/source is removed, try to move its streams to the default sink/source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    NULL,
};

struct userdata {
    pa_hook_slot
        *sink_unlink_slot,
        *source_unlink_slot,
        *sink_input_move_fail_slot,
        *source_output_move_fail_slot;
};

static pa_source* find_source_from_port(pa_core *c, pa_device_port *port) {
    pa_source *target;
    uint32_t idx;
    void *state;
    pa_device_port *p;

    if (!port)
        return NULL;

    PA_IDXSET_FOREACH(target, c->sources, idx)
        PA_HASHMAP_FOREACH(p, target->ports, state)
            if (port == p)
                return target;

    return NULL;
}

static pa_sink* find_sink_from_port(pa_core *c, pa_device_port *port) {
    pa_sink *target;
    uint32_t idx;
    void *state;
    pa_device_port *p;

    if (!port)
        return NULL;

    PA_IDXSET_FOREACH(target, c->sinks, idx)
        PA_HASHMAP_FOREACH(p, target->ports, state)
            if (port == p)
                return target;

    return NULL;
}

static void build_group_ports(pa_hashmap *g_ports, pa_hashmap *s_ports) {
    void *state;
    pa_device_port *p;

    if (!g_ports || !s_ports)
        return;

    PA_HASHMAP_FOREACH(p, s_ports, state)
        pa_hashmap_put(g_ports, p, p);
}

static pa_sink* find_evacuation_sink(pa_core *c, pa_sink_input *i, pa_sink *skip) {
    pa_sink *target, *def, *fb_sink = NULL;
    uint32_t idx;
    pa_hashmap *all_ports;
    pa_device_port *best_port;

    pa_assert(c);
    pa_assert(i);

    def = pa_namereg_get_default_sink(c);

    if (def && def != skip && pa_sink_input_may_move_to(i, def))
        return def;

    all_ports = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    PA_IDXSET_FOREACH(target, c->sinks, idx) {
        if (target == def)
            continue;

        if (target == skip)
            continue;

        if (!PA_SINK_IS_LINKED(pa_sink_get_state(target)))
            continue;

        if (!pa_sink_input_may_move_to(i, target))
            continue;

        if (!fb_sink)
            fb_sink = target;

        build_group_ports(all_ports, target->ports);
    }

    best_port = pa_device_port_find_best(all_ports);

    pa_hashmap_free(all_ports);

    if (best_port)
        target = find_sink_from_port(c, best_port);
    else
        target = fb_sink;

    if (!target)
	pa_log_debug("No evacuation sink found.");

    return target;
}

static pa_hook_result_t sink_unlink_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    pa_sink_input *i;
    uint32_t idx;

    pa_assert(c);
    pa_assert(sink);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    if (pa_idxset_size(sink->inputs) <= 0) {
        pa_log_debug("No sink inputs to move away.");
        return PA_HOOK_OK;
    }

    PA_IDXSET_FOREACH(i, sink->inputs, idx) {
        pa_sink *target;

        if (!(target = find_evacuation_sink(c, i, sink)))
            continue;

        if (pa_sink_input_move_to(i, target, false) < 0)
            pa_log_info("Failed to move sink input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        else
            pa_log_info("Successfully moved sink input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_move_fail_hook_callback(pa_core *c, pa_sink_input *i, void *userdata) {
    pa_sink *target;

    pa_assert(c);
    pa_assert(i);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    if (!(target = find_evacuation_sink(c, i, NULL)))
        return PA_HOOK_OK;

    if (pa_sink_input_finish_move(i, target, false) < 0) {
        pa_log_info("Failed to move sink input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        return PA_HOOK_OK;

    } else {
        pa_log_info("Successfully moved sink input %u \"%s\" to %s.", i->index,
                    pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        return PA_HOOK_STOP;
    }
}

static pa_source* find_evacuation_source(pa_core *c, pa_source_output *o, pa_source *skip) {
    pa_source *target, *def, *fb_source = NULL;
    uint32_t idx;
    pa_hashmap *all_ports;
    pa_device_port *best_port;

    pa_assert(c);
    pa_assert(o);

    def = pa_namereg_get_default_source(c);

    if (def && def != skip && pa_source_output_may_move_to(o, def))
        return def;

    all_ports = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    PA_IDXSET_FOREACH(target, c->sources, idx) {
        if (target == def)
            continue;

        if (target == skip)
            continue;

        if (skip && !target->monitor_of != !skip->monitor_of)
            continue;

        if (!PA_SOURCE_IS_LINKED(pa_source_get_state(target)))
            continue;

        if (!pa_source_output_may_move_to(o, target))
            continue;

        if (!fb_source)
            fb_source = target;

        build_group_ports(all_ports, target->ports);
    }

    best_port = pa_device_port_find_best(all_ports);

    pa_hashmap_free(all_ports);

    if (best_port)
        target = find_source_from_port(c, best_port);
    else
        target = fb_source;

    if (!target)
        pa_log_debug("No evacuation source found.");

    return target;
}

static pa_hook_result_t source_unlink_hook_callback(pa_core *c, pa_source *source, void* userdata) {
    pa_source_output *o;
    uint32_t idx;

    pa_assert(c);
    pa_assert(source);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    if (pa_idxset_size(source->outputs) <= 0) {
        pa_log_debug("No source outputs to move away.");
        return PA_HOOK_OK;
    }

    PA_IDXSET_FOREACH(o, source->outputs, idx) {
        pa_source *target;

        if (!(target = find_evacuation_source(c, o, source)))
            continue;

        if (pa_source_output_move_to(o, target, false) < 0)
            pa_log_info("Failed to move source output %u \"%s\" to %s.", o->index,
                        pa_strnull(pa_proplist_gets(o->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        else
            pa_log_info("Successfully moved source output %u \"%s\" to %s.", o->index,
                        pa_strnull(pa_proplist_gets(o->proplist, PA_PROP_APPLICATION_NAME)), target->name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_move_fail_hook_callback(pa_core *c, pa_source_output *i, void *userdata) {
    pa_source *target;

    pa_assert(c);
    pa_assert(i);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    if (!(target = find_evacuation_source(c, i, NULL)))
        return PA_HOOK_OK;

    if (pa_source_output_finish_move(i, target, false) < 0) {
        pa_log_info("Failed to move source input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        return PA_HOOK_OK;

    } else {
        pa_log_info("Successfully moved source input %u \"%s\" to %s.", i->index,
                    pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), target->name);
        return PA_HOOK_STOP;
    }
}

int pa__init(pa_module*m) {
    pa_modargs *ma;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        return -1;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);

    /* A little bit later than module-stream-restore, module-intended-roles... */
    u->sink_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE+20, (pa_hook_cb_t) sink_unlink_hook_callback, u);
    u->source_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE+20, (pa_hook_cb_t) source_unlink_hook_callback, u);

    u->sink_input_move_fail_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FAIL], PA_HOOK_LATE+20, (pa_hook_cb_t) sink_input_move_fail_hook_callback, u);
    u->source_output_move_fail_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_MOVE_FAIL], PA_HOOK_LATE+20, (pa_hook_cb_t) source_output_move_fail_hook_callback, u);

    pa_modargs_free(ma);
    return 0;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_unlink_slot)
        pa_hook_slot_free(u->sink_unlink_slot);
    if (u->source_unlink_slot)
        pa_hook_slot_free(u->source_unlink_slot);

    if (u->sink_input_move_fail_slot)
        pa_hook_slot_free(u->sink_input_move_fail_slot);
    if (u->source_output_move_fail_slot)
        pa_hook_slot_free(u->source_output_move_fail_slot);

    pa_xfree(u);
}
