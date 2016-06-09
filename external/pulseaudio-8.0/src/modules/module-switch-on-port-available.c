/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2011 Canonical Ltd

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

#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/device-port.h>
#include <pulsecore/hashmap.h>

#include "module-switch-on-port-available-symdef.h"

static bool profile_good_for_output(pa_card_profile *profile) {
    pa_sink *sink;
    uint32_t idx;

    pa_assert(profile);

    if (!pa_safe_streq(profile->card->active_profile->input_name, profile->input_name))
        return false;

    if (profile->card->active_profile->n_sources != profile->n_sources)
        return false;

    if (profile->card->active_profile->max_source_channels != profile->max_source_channels)
        return false;

    /* Try not to switch to HDMI sinks from analog when HDMI is becoming available */
    PA_IDXSET_FOREACH(sink, profile->card->sinks, idx) {
        if (!sink->active_port)
            continue;

        if (sink->active_port->available != PA_AVAILABLE_NO)
            return false;
    }

    return true;
}

static bool profile_good_for_input(pa_card_profile *profile) {
    pa_assert(profile);

    if (!pa_safe_streq(profile->card->active_profile->output_name, profile->output_name))
        return false;

    if (profile->card->active_profile->n_sinks != profile->n_sinks)
        return false;

    if (profile->card->active_profile->max_sink_channels != profile->max_sink_channels)
        return false;

    return true;
}

static int try_to_switch_profile(pa_device_port *port) {
    pa_card_profile *best_profile = NULL, *profile;
    void *state;
    unsigned best_prio = 0;

    pa_log_debug("Finding best profile for port %s, preferred = %s",
                 port->name, pa_strnull(port->preferred_profile));

    PA_HASHMAP_FOREACH(profile, port->profiles, state) {
        bool good = false;
        const char *name;
        unsigned prio = profile->priority;

        /* We make a best effort to keep other direction unchanged */
        switch (port->direction) {
            case PA_DIRECTION_OUTPUT:
                name = profile->output_name;
                good = profile_good_for_output(profile);
                break;

            case PA_DIRECTION_INPUT:
                name = profile->input_name;
                good = profile_good_for_input(profile);
                break;
        }

        if (!good)
            continue;

        /* Give a high bonus in case this is the preferred profile */
        if (port->preferred_profile && pa_streq(name ? name : profile->name, port->preferred_profile))
            prio += 1000000;

        if (best_profile && best_prio >= prio)
            continue;

        best_profile = profile;
        best_prio = prio;
    }

    if (!best_profile) {
        pa_log_debug("No suitable profile found");
        return -1;
    }

    if (pa_card_set_profile(port->card, best_profile, false) != 0) {
        pa_log_debug("Could not set profile %s", best_profile->name);
        return -1;
    }

    return 0;
}

struct port_pointers {
    pa_device_port *port;
    pa_sink *sink;
    pa_source *source;
    bool is_possible_profile_active;
    bool is_preferred_profile_active;
    bool is_port_active;
};

static const char* profile_name_for_dir(pa_card_profile *cp, pa_direction_t dir) {
    if (dir == PA_DIRECTION_OUTPUT && cp->output_name)
        return cp->output_name;
    if (dir == PA_DIRECTION_INPUT && cp->input_name)
        return cp->input_name;
    return cp->name;
}

static struct port_pointers find_port_pointers(pa_device_port *port) {
    struct port_pointers pp = { .port = port };
    uint32_t state;
    pa_card *card;

    pa_assert(port);
    pa_assert_se(card = port->card);

    switch (port->direction) {
        case PA_DIRECTION_OUTPUT:
            PA_IDXSET_FOREACH(pp.sink, card->sinks, state)
                if (port == pa_hashmap_get(pp.sink->ports, port->name))
                    break;
            break;

        case PA_DIRECTION_INPUT:
            PA_IDXSET_FOREACH(pp.source, card->sources, state)
                if (port == pa_hashmap_get(pp.source->ports, port->name))
                    break;
            break;
    }

    pp.is_possible_profile_active =
        card->active_profile == pa_hashmap_get(port->profiles, card->active_profile->name);
    pp.is_preferred_profile_active = pp.is_possible_profile_active && (!port->preferred_profile ||
        pa_safe_streq(port->preferred_profile, profile_name_for_dir(card->active_profile, port->direction)));
    pp.is_port_active = (pp.sink && pp.sink->active_port == port) || (pp.source && pp.source->active_port == port);

    return pp;
}

/* Switches to a port, switching profiles if necessary or preferred */
static bool switch_to_port(pa_device_port *port) {
    struct port_pointers pp = find_port_pointers(port);

    if (pp.is_port_active)
        return true; /* Already selected */

    pa_log_debug("Trying to switch to port %s", port->name);
    if (!pp.is_preferred_profile_active) {
        if (try_to_switch_profile(port) < 0) {
            if (pp.is_possible_profile_active)
                return false;
        }
        else
            /* Now that profile has changed, our sink and source pointers must be updated */
            pp = find_port_pointers(port);
    }

    if (pp.source)
        pa_source_set_port(pp.source, port->name, false);
    if (pp.sink)
        pa_sink_set_port(pp.sink, port->name, false);
    return true;
}

/* Switches away from a port, switching profiles if necessary or preferred */
static bool switch_from_port(pa_device_port *port) {
    struct port_pointers pp = find_port_pointers(port);
    pa_device_port *p, *best_port = NULL;
    void *state;

    if (!pp.is_port_active)
        return true; /* Already deselected */

    /* Try to find a good enough port to switch to */
    PA_HASHMAP_FOREACH(p, port->card->ports, state)
        if (p->direction == port->direction && p != port && p->available != PA_AVAILABLE_NO &&
           (!best_port || best_port->priority < p->priority))
           best_port = p;

    pa_log_debug("Trying to switch away from port %s, found %s", port->name, best_port ? best_port->name : "no better option");

    if (best_port)
        return switch_to_port(best_port);

    return false;
}


static pa_hook_result_t port_available_hook_callback(pa_core *c, pa_device_port *port, void* userdata) {
    pa_assert(port);

    if (!port->card) {
        pa_log_warn("Port %s does not have a card", port->name);
        return PA_HOOK_OK;
    }

    if (pa_idxset_size(port->card->sinks) == 0 && pa_idxset_size(port->card->sources) == 0)
        /* This card is not initialized yet. We'll handle it in
           sink_new / source_new callbacks later. */
        return PA_HOOK_OK;

    switch (port->available) {
    case PA_AVAILABLE_YES:
        switch_to_port(port);
        break;
    case PA_AVAILABLE_NO:
        switch_from_port(port);
        break;
    default:
        break;
    }

    return PA_HOOK_OK;
}

static void handle_all_unavailable(pa_core *core) {
    pa_card *card;
    uint32_t state;

    PA_IDXSET_FOREACH(card, core->cards, state) {
        pa_device_port *port;
        void *state2;

        PA_HASHMAP_FOREACH(port, card->ports, state2) {
            if (port->available == PA_AVAILABLE_NO)
                port_available_hook_callback(core, port, NULL);
        }
    }
}

static pa_device_port *new_sink_source(pa_hashmap *ports, const char *name) {

    void *state;
    pa_device_port *i, *p = NULL;

    if (!ports)
        return NULL;
    if (name)
        p = pa_hashmap_get(ports, name);
    if (!p)
        PA_HASHMAP_FOREACH(i, ports, state)
            if (!p || i->priority > p->priority)
                p = i;
    if (!p)
        return NULL;
    if (p->available != PA_AVAILABLE_NO)
        return NULL;

    pa_assert_se(p = pa_device_port_find_best(ports));
    return p;
}

static pa_hook_result_t sink_new_hook_callback(pa_core *c, pa_sink_new_data *new_data, void *u) {

    pa_device_port *p = new_sink_source(new_data->ports, new_data->active_port);

    if (p) {
        pa_log_debug("Switching initial port for sink '%s' to '%s'", new_data->name, p->name);
        pa_sink_new_data_set_port(new_data, p->name);
    }
    return PA_HOOK_OK;
}

static pa_hook_result_t source_new_hook_callback(pa_core *c, pa_source_new_data *new_data, void *u) {

    pa_device_port *p = new_sink_source(new_data->ports, new_data->active_port);

    if (p) {
        pa_log_debug("Switching initial port for source '%s' to '%s'", new_data->name, p->name);
        pa_source_new_data_set_port(new_data, p->name);
    }
    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_assert(m);

    /* Make sure we are after module-device-restore, so we can overwrite that suggestion if necessary */
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_NEW],
                           PA_HOOK_NORMAL, (pa_hook_cb_t) sink_new_hook_callback, NULL);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_NEW],
                           PA_HOOK_NORMAL, (pa_hook_cb_t) source_new_hook_callback, NULL);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_PORT_AVAILABLE_CHANGED],
                           PA_HOOK_LATE, (pa_hook_cb_t) port_available_hook_callback, NULL);

    handle_all_unavailable(m->core);

    return 0;
}
