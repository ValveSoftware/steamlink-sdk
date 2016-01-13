/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB
  Copyright 2011 David Henningsson, Canonical Ltd.

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

#include "device-port.h"
#include <pulsecore/card.h>

PA_DEFINE_PUBLIC_CLASS(pa_device_port, pa_object);

pa_device_port_new_data *pa_device_port_new_data_init(pa_device_port_new_data *data) {
    pa_assert(data);

    pa_zero(*data);
    data->available = PA_AVAILABLE_UNKNOWN;
    return data;
}

void pa_device_port_new_data_set_name(pa_device_port_new_data *data, const char *name) {
    pa_assert(data);

    pa_xfree(data->name);
    data->name = pa_xstrdup(name);
}

void pa_device_port_new_data_set_description(pa_device_port_new_data *data, const char *description) {
    pa_assert(data);

    pa_xfree(data->description);
    data->description = pa_xstrdup(description);
}

void pa_device_port_new_data_set_available(pa_device_port_new_data *data, pa_available_t available) {
    pa_assert(data);

    data->available = available;
}

void pa_device_port_new_data_set_direction(pa_device_port_new_data *data, pa_direction_t direction) {
    pa_assert(data);

    data->direction = direction;
}

void pa_device_port_new_data_done(pa_device_port_new_data *data) {
    pa_assert(data);

    pa_xfree(data->name);
    pa_xfree(data->description);
}

void pa_device_port_set_available(pa_device_port *p, pa_available_t status) {
    pa_core *core;

    pa_assert(p);

    if (p->available == status)
        return;

/*    pa_assert(status != PA_AVAILABLE_UNKNOWN); */

    p->available = status;
    pa_log_debug("Setting port %s to status %s", p->name, status == PA_AVAILABLE_YES ? "yes" :
       status == PA_AVAILABLE_NO ? "no" : "unknown");

    /* Post subscriptions to the card which owns us */
    pa_assert_se(core = p->core);
    pa_subscription_post(core, PA_SUBSCRIPTION_EVENT_CARD|PA_SUBSCRIPTION_EVENT_CHANGE, p->card->index);

    pa_hook_fire(&core->hooks[PA_CORE_HOOK_PORT_AVAILABLE_CHANGED], p);
}

static void device_port_free(pa_object *o) {
    pa_device_port *p = PA_DEVICE_PORT(o);

    pa_assert(p);
    pa_assert(pa_device_port_refcnt(p) == 0);

    if (p->proplist)
        pa_proplist_free(p->proplist);

    if (p->profiles)
        pa_hashmap_free(p->profiles);

    pa_xfree(p->name);
    pa_xfree(p->description);
    pa_xfree(p);
}

pa_device_port *pa_device_port_new(pa_core *c, pa_device_port_new_data *data, size_t extra) {
    pa_device_port *p;

    pa_assert(data);
    pa_assert(data->name);
    pa_assert(data->description);
    pa_assert(data->direction == PA_DIRECTION_OUTPUT || data->direction == PA_DIRECTION_INPUT);

    p = PA_DEVICE_PORT(pa_object_new_internal(PA_ALIGN(sizeof(pa_device_port)) + extra, pa_device_port_type_id, pa_device_port_check_type));
    p->parent.free = device_port_free;

    p->name = data->name;
    data->name = NULL;
    p->description = data->description;
    data->description = NULL;
    p->core = c;
    p->card = NULL;
    p->priority = 0;
    p->available = data->available;
    p->profiles = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    p->direction = data->direction;

    p->latency_offset = 0;
    p->proplist = pa_proplist_new();

    return p;
}

void pa_device_port_set_latency_offset(pa_device_port *p, int64_t offset) {
    uint32_t state;
    pa_core *core;

    pa_assert(p);

    if (offset == p->latency_offset)
        return;

    p->latency_offset = offset;

    switch (p->direction) {
        case PA_DIRECTION_OUTPUT: {
            pa_sink *sink;

            PA_IDXSET_FOREACH(sink, p->core->sinks, state) {
                if (sink->active_port == p) {
                    pa_sink_set_latency_offset(sink, p->latency_offset);
                    break;
                }
            }

            break;
        }

        case PA_DIRECTION_INPUT: {
            pa_source *source;

            PA_IDXSET_FOREACH(source, p->core->sources, state) {
                if (source->active_port == p) {
                    pa_source_set_latency_offset(source, p->latency_offset);
                    break;
                }
            }

            break;
        }
    }

    pa_assert_se(core = p->core);
    pa_subscription_post(core, PA_SUBSCRIPTION_EVENT_CARD|PA_SUBSCRIPTION_EVENT_CHANGE, p->card->index);
    pa_hook_fire(&core->hooks[PA_CORE_HOOK_PORT_LATENCY_OFFSET_CHANGED], p);
}

pa_device_port *pa_device_port_find_best(pa_hashmap *ports)
{
    void *state;
    pa_device_port *p, *best = NULL;

    if (!ports)
        return NULL;

    /* First run: skip unavailable ports */
    PA_HASHMAP_FOREACH(p, ports, state) {
        if (p->available == PA_AVAILABLE_NO)
            continue;

        if (!best || p->priority > best->priority)
            best = p;
    }

    /* Second run: if only unavailable ports exist, still suggest a port */
    if (!best) {
        PA_HASHMAP_FOREACH(p, ports, state)
            if (!best || p->priority > best->priority)
                best = p;
    }

    return best;
}
