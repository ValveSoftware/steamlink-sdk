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

#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/modargs.h>
#include <pulsecore/queue.h>

#include <modules/reserve-wrap.h>

#ifdef HAVE_UDEV
#include <modules/udev-util.h>
#endif

#include "alsa-util.h"
#include "alsa-ucm.h"
#include "alsa-sink.h"
#include "alsa-source.h"
#include "module-alsa-card-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("ALSA Card");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "name=<name for the card/sink/source, to be prefixed> "
        "card_name=<name for the card> "
        "card_properties=<properties for the card> "
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "source_name=<name for the source> "
        "source_properties=<properties for the source> "
        "namereg_fail=<when false attempt to synthesise new names if they are already taken> "
        "device_id=<ALSA card index> "
        "format=<sample format> "
        "rate=<sample rate> "
        "fragments=<number of fragments> "
        "fragment_size=<fragment size> "
        "mmap=<enable memory mapping?> "
        "tsched=<enable system timer based scheduling mode?> "
        "tsched_buffer_size=<buffer size when using timer based scheduling> "
        "tsched_buffer_watermark=<lower fill watermark> "
        "profile=<profile name> "
        "fixed_latency_range=<disable latency range changes on underrun?> "
        "ignore_dB=<ignore dB information from the device?> "
        "deferred_volume=<Synchronize software and hardware volume changes to avoid momentary jumps?> "
        "profile_set=<profile set configuration file> "
        "paths_dir=<directory containing the path configuration files> "
        "use_ucm=<load use case manager> "
);

static const char* const valid_modargs[] = {
    "name",
    "card_name",
    "card_properties",
    "sink_name",
    "sink_properties",
    "source_name",
    "source_properties",
    "namereg_fail",
    "device_id",
    "format",
    "rate",
    "fragments",
    "fragment_size",
    "mmap",
    "tsched",
    "tsched_buffer_size",
    "tsched_buffer_watermark",
    "fixed_latency_range",
    "profile",
    "ignore_dB",
    "deferred_volume",
    "profile_set",
    "paths_dir",
    "use_ucm",
    NULL
};

#define DEFAULT_DEVICE_ID "0"

struct userdata {
    pa_core *core;
    pa_module *module;

    char *device_id;
    int alsa_card_index;

    snd_mixer_t *mixer_handle;
    pa_hashmap *jacks;
    pa_alsa_fdlist *mixer_fdl;

    pa_card *card;

    pa_modargs *modargs;

    pa_alsa_profile_set *profile_set;

    /* ucm stuffs */
    bool use_ucm;
    pa_alsa_ucm_config ucm;

    /* hooks for modifier action */
    pa_hook_slot
        *sink_input_put_hook_slot,
        *source_output_put_hook_slot,
        *sink_input_unlink_hook_slot,
        *source_output_unlink_hook_slot;
};

struct profile_data {
    pa_alsa_profile *profile;
};

static void add_profiles(struct userdata *u, pa_hashmap *h, pa_hashmap *ports) {
    pa_alsa_profile *ap;
    void *state;

    pa_assert(u);
    pa_assert(h);

    PA_HASHMAP_FOREACH(ap, u->profile_set->profiles, state) {
        struct profile_data *d;
        pa_card_profile *cp;
        pa_alsa_mapping *m;
        uint32_t idx;

        cp = pa_card_profile_new(ap->name, ap->description, sizeof(struct profile_data));
        cp->priority = ap->priority;

        if (ap->output_mappings) {
            cp->n_sinks = pa_idxset_size(ap->output_mappings);

            PA_IDXSET_FOREACH(m, ap->output_mappings, idx) {
                if (u->use_ucm)
                    pa_alsa_ucm_add_ports_combination(NULL, &m->ucm_context, true, ports, cp, u->core);
                else
                    pa_alsa_path_set_add_ports(m->output_path_set, cp, ports, NULL, u->core);
                if (m->channel_map.channels > cp->max_sink_channels)
                    cp->max_sink_channels = m->channel_map.channels;
            }
        }

        if (ap->input_mappings) {
            cp->n_sources = pa_idxset_size(ap->input_mappings);

            PA_IDXSET_FOREACH(m, ap->input_mappings, idx) {
                if (u->use_ucm)
                    pa_alsa_ucm_add_ports_combination(NULL, &m->ucm_context, false, ports, cp, u->core);
                else
                    pa_alsa_path_set_add_ports(m->input_path_set, cp, ports, NULL, u->core);
                if (m->channel_map.channels > cp->max_source_channels)
                    cp->max_source_channels = m->channel_map.channels;
            }
        }

        d = PA_CARD_PROFILE_DATA(cp);
        d->profile = ap;

        pa_hashmap_put(h, cp->name, cp);
    }
}

static void add_disabled_profile(pa_hashmap *profiles) {
    pa_card_profile *p;
    struct profile_data *d;

    p = pa_card_profile_new("off", _("Off"), sizeof(struct profile_data));

    d = PA_CARD_PROFILE_DATA(p);
    d->profile = NULL;

    pa_hashmap_put(profiles, p->name, p);
}

static int card_set_profile(pa_card *c, pa_card_profile *new_profile) {
    struct userdata *u;
    struct profile_data *nd, *od;
    uint32_t idx;
    pa_alsa_mapping *am;
    pa_queue *sink_inputs = NULL, *source_outputs = NULL;
    int ret = 0;

    pa_assert(c);
    pa_assert(new_profile);
    pa_assert_se(u = c->userdata);

    nd = PA_CARD_PROFILE_DATA(new_profile);
    od = PA_CARD_PROFILE_DATA(c->active_profile);

    if (od->profile && od->profile->output_mappings)
        PA_IDXSET_FOREACH(am, od->profile->output_mappings, idx) {
            if (!am->sink)
                continue;

            if (nd->profile &&
                nd->profile->output_mappings &&
                pa_idxset_get_by_data(nd->profile->output_mappings, am, NULL))
                continue;

            sink_inputs = pa_sink_move_all_start(am->sink, sink_inputs);
            pa_alsa_sink_free(am->sink);
            am->sink = NULL;
        }

    if (od->profile && od->profile->input_mappings)
        PA_IDXSET_FOREACH(am, od->profile->input_mappings, idx) {
            if (!am->source)
                continue;

            if (nd->profile &&
                nd->profile->input_mappings &&
                pa_idxset_get_by_data(nd->profile->input_mappings, am, NULL))
                continue;

            source_outputs = pa_source_move_all_start(am->source, source_outputs);
            pa_alsa_source_free(am->source);
            am->source = NULL;
        }

    /* if UCM is available for this card then update the verb */
    if (u->use_ucm) {
        if (pa_alsa_ucm_set_profile(&u->ucm, nd->profile ? nd->profile->name : NULL,
                    od->profile ? od->profile->name : NULL) < 0) {
            ret = -1;
            goto finish;
        }
    }

    if (nd->profile && nd->profile->output_mappings)
        PA_IDXSET_FOREACH(am, nd->profile->output_mappings, idx) {

            if (!am->sink)
                am->sink = pa_alsa_sink_new(c->module, u->modargs, __FILE__, c, am);

            if (sink_inputs && am->sink) {
                pa_sink_move_all_finish(am->sink, sink_inputs, false);
                sink_inputs = NULL;
            }
        }

    if (nd->profile && nd->profile->input_mappings)
        PA_IDXSET_FOREACH(am, nd->profile->input_mappings, idx) {

            if (!am->source)
                am->source = pa_alsa_source_new(c->module, u->modargs, __FILE__, c, am);

            if (source_outputs && am->source) {
                pa_source_move_all_finish(am->source, source_outputs, false);
                source_outputs = NULL;
            }
        }

finish:
    if (sink_inputs)
        pa_sink_move_all_fail(sink_inputs);

    if (source_outputs)
        pa_source_move_all_fail(source_outputs);

    return ret;
}

static void init_profile(struct userdata *u) {
    uint32_t idx;
    pa_alsa_mapping *am;
    struct profile_data *d;
    pa_alsa_ucm_config *ucm = &u->ucm;

    pa_assert(u);

    d = PA_CARD_PROFILE_DATA(u->card->active_profile);

    if (d->profile && u->use_ucm) {
        /* Set initial verb */
        if (pa_alsa_ucm_set_profile(ucm, d->profile->name, NULL) < 0) {
            pa_log("Failed to set ucm profile %s", d->profile->name);
            return;
        }
    }

    if (d->profile && d->profile->output_mappings)
        PA_IDXSET_FOREACH(am, d->profile->output_mappings, idx)
            am->sink = pa_alsa_sink_new(u->module, u->modargs, __FILE__, u->card, am);

    if (d->profile && d->profile->input_mappings)
        PA_IDXSET_FOREACH(am, d->profile->input_mappings, idx)
            am->source = pa_alsa_source_new(u->module, u->modargs, __FILE__, u->card, am);
}

static void report_port_state(pa_device_port *p, struct userdata *u) {
    void *state;
    pa_alsa_jack *jack;
    pa_available_t pa = PA_AVAILABLE_UNKNOWN;
    pa_device_port *port;

    PA_HASHMAP_FOREACH(jack, u->jacks, state) {
        pa_available_t cpa;

        if (u->use_ucm)
            port = pa_hashmap_get(u->card->ports, jack->name);
        else {
            if (jack->path)
                port = jack->path->port;
            else
                continue;
        }

        if (p != port)
            continue;

        cpa = jack->plugged_in ? jack->state_plugged : jack->state_unplugged;

        if (cpa == PA_AVAILABLE_NO) {
          /* If a plugged-in jack causes the availability to go to NO, it
           * should override all other availability information (like a
           * blacklist) so set and bail */
          if (jack->plugged_in) {
            pa = cpa;
            break;
          }

          /* If the current availablility is unknown go the more precise no,
           * but otherwise don't change state */
          if (pa == PA_AVAILABLE_UNKNOWN)
            pa = cpa;
        } else if (cpa == PA_AVAILABLE_YES) {
          /* Output is available through at least one jack, so go to that
           * level of availability. We still need to continue iterating through
           * the jacks in case a jack is plugged in that forces the state to no
           */
          pa = cpa;
        }
    }

    pa_device_port_set_available(p, pa);
}

static int report_jack_state(snd_mixer_elem_t *melem, unsigned int mask) {
    struct userdata *u = snd_mixer_elem_get_callback_private(melem);
    snd_hctl_elem_t *elem = snd_mixer_elem_get_private(melem);
    snd_ctl_elem_value_t *elem_value;
    bool plugged_in;
    void *state;
    pa_alsa_jack *jack;
    pa_device_port *port;

    pa_assert(u);

    if (mask == SND_CTL_EVENT_MASK_REMOVE)
        return 0;

    snd_ctl_elem_value_alloca(&elem_value);
    if (snd_hctl_elem_read(elem, elem_value) < 0) {
        pa_log_warn("Failed to read jack detection from '%s'", pa_strnull(snd_hctl_elem_get_name(elem)));
        return 0;
    }

    plugged_in = !!snd_ctl_elem_value_get_boolean(elem_value, 0);

    pa_log_debug("Jack '%s' is now %s", pa_strnull(snd_hctl_elem_get_name(elem)), plugged_in ? "plugged in" : "unplugged");

    PA_HASHMAP_FOREACH(jack, u->jacks, state)
        if (jack->melem == melem) {
            jack->plugged_in = plugged_in;
            if (u->use_ucm) {
                pa_assert(u->card->ports);
                port = pa_hashmap_get(u->card->ports, jack->name);
                pa_assert(port);
            }
            else {
                pa_assert(jack->path);
                pa_assert_se(port = jack->path->port);
            }
            report_port_state(port, u);
        }
    return 0;
}

static pa_device_port* find_port_with_eld_device(pa_hashmap *ports, int device) {
    void *state;
    pa_device_port *p;

    PA_HASHMAP_FOREACH(p, ports, state) {
        pa_alsa_port_data *data = PA_DEVICE_PORT_DATA(p);
        pa_assert(data->path);
        if (device == data->path->eld_device)
            return p;
    }
    return NULL;
}

static int hdmi_eld_changed(snd_mixer_elem_t *melem, unsigned int mask) {
    struct userdata *u = snd_mixer_elem_get_callback_private(melem);
    snd_hctl_elem_t *elem = snd_mixer_elem_get_private(melem);
    int device = snd_hctl_elem_get_device(elem);
    const char *old_monitor_name;
    pa_device_port *p;
    pa_hdmi_eld eld;
    bool changed = false;

    if (mask == SND_CTL_EVENT_MASK_REMOVE)
        return 0;

    p = find_port_with_eld_device(u->card->ports, device);
    if (p == NULL) {
        pa_log_error("Invalid device changed in ALSA: %d", device);
        return 0;
    }

    if (pa_alsa_get_hdmi_eld(elem, &eld) < 0)
        memset(&eld, 0, sizeof(eld));

    old_monitor_name = pa_proplist_gets(p->proplist, PA_PROP_DEVICE_PRODUCT_NAME);
    if (eld.monitor_name[0] == '\0') {
        changed |= old_monitor_name != NULL;
        pa_proplist_unset(p->proplist, PA_PROP_DEVICE_PRODUCT_NAME);
    } else {
        changed |= (old_monitor_name == NULL) || (strcmp(old_monitor_name, eld.monitor_name) != 0);
        pa_proplist_sets(p->proplist, PA_PROP_DEVICE_PRODUCT_NAME, eld.monitor_name);
    }

    if (changed && mask != 0)
        pa_subscription_post(u->core, PA_SUBSCRIPTION_EVENT_CARD|PA_SUBSCRIPTION_EVENT_CHANGE, u->card->index);

    return 0;
}

static void init_eld_ctls(struct userdata *u) {
    void *state;
    pa_device_port *port;

    if (!u->mixer_handle)
        return;

    /* The code in this function expects ports to have a pa_alsa_port_data
     * struct as their data, but in UCM mode ports don't have any data. Hence,
     * the ELD controls can't currently be used in UCM mode. */
    if (u->use_ucm)
        return;

    PA_HASHMAP_FOREACH(port, u->card->ports, state) {
        pa_alsa_port_data *data = PA_DEVICE_PORT_DATA(port);
        snd_mixer_elem_t* melem;
        int device;

        pa_assert(data->path);
        device = data->path->eld_device;
        if (device < 0)
            continue;

        melem = pa_alsa_mixer_find(u->mixer_handle, "ELD", device);
        if (melem) {
            snd_mixer_elem_set_callback(melem, hdmi_eld_changed);
            snd_mixer_elem_set_callback_private(melem, u);
            hdmi_eld_changed(melem, 0);
        }
        else
            pa_log_debug("No ELD device found for port %s.", port->name);
    }
}

static void init_jacks(struct userdata *u) {
    void *state;
    pa_alsa_path* path;
    pa_alsa_jack* jack;

    u->jacks = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    if (u->use_ucm) {
        PA_LLIST_FOREACH(jack, u->ucm.jacks)
            if (jack->has_control)
                pa_hashmap_put(u->jacks, jack, jack);
    } else {
        /* See if we have any jacks */
        if (u->profile_set->output_paths)
            PA_HASHMAP_FOREACH(path, u->profile_set->output_paths, state)
                PA_LLIST_FOREACH(jack, path->jacks)
                    if (jack->has_control)
                        pa_hashmap_put(u->jacks, jack, jack);

        if (u->profile_set->input_paths)
            PA_HASHMAP_FOREACH(path, u->profile_set->input_paths, state)
                PA_LLIST_FOREACH(jack, path->jacks)
                    if (jack->has_control)
                        pa_hashmap_put(u->jacks, jack, jack);
    }

    pa_log_debug("Found %d jacks.", pa_hashmap_size(u->jacks));

    if (pa_hashmap_size(u->jacks) == 0)
        return;

    u->mixer_fdl = pa_alsa_fdlist_new();

    u->mixer_handle = pa_alsa_open_mixer(u->alsa_card_index, NULL);
    if (u->mixer_handle && pa_alsa_fdlist_set_handle(u->mixer_fdl, u->mixer_handle, NULL, u->core->mainloop) >= 0) {
        PA_HASHMAP_FOREACH(jack, u->jacks, state) {
            jack->melem = pa_alsa_mixer_find(u->mixer_handle, jack->alsa_name, 0);
            if (!jack->melem) {
                pa_log_warn("Jack '%s' seems to have disappeared.", jack->alsa_name);
                jack->has_control = false;
                continue;
            }
            snd_mixer_elem_set_callback(jack->melem, report_jack_state);
            snd_mixer_elem_set_callback_private(jack->melem, u);
            report_jack_state(jack->melem, 0);
        }

    } else
        pa_log("Failed to open mixer for jack detection");

}

static void set_card_name(pa_card_new_data *data, pa_modargs *ma, const char *device_id) {
    char *t;
    const char *n;

    pa_assert(data);
    pa_assert(ma);
    pa_assert(device_id);

    if ((n = pa_modargs_get_value(ma, "card_name", NULL))) {
        pa_card_new_data_set_name(data, n);
        data->namereg_fail = true;
        return;
    }

    if ((n = pa_modargs_get_value(ma, "name", NULL)))
        data->namereg_fail = true;
    else {
        n = device_id;
        data->namereg_fail = false;
    }

    t = pa_sprintf_malloc("alsa_card.%s", n);
    pa_card_new_data_set_name(data, t);
    pa_xfree(t);
}

static pa_hook_result_t sink_input_put_hook_callback(pa_core *c, pa_sink_input *sink_input, struct userdata *u) {
    const char *role;
    pa_sink *sink = sink_input->sink;

    pa_assert(sink);

    role = pa_proplist_gets(sink_input->proplist, PA_PROP_MEDIA_ROLE);

    /* new sink input linked to sink of this card */
    if (role && sink->card == u->card)
        pa_alsa_ucm_roled_stream_begin(&u->ucm, role, PA_DIRECTION_OUTPUT);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_put_hook_callback(pa_core *c, pa_source_output *source_output, struct userdata *u) {
    const char *role;
    pa_source *source = source_output->source;

    pa_assert(source);

    role = pa_proplist_gets(source_output->proplist, PA_PROP_MEDIA_ROLE);

    /* new source output linked to source of this card */
    if (role && source->card == u->card)
        pa_alsa_ucm_roled_stream_begin(&u->ucm, role, PA_DIRECTION_INPUT);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_unlink_hook_callback(pa_core *c, pa_sink_input *sink_input, struct userdata *u) {
    const char *role;
    pa_sink *sink = sink_input->sink;

    pa_assert(sink);

    role = pa_proplist_gets(sink_input->proplist, PA_PROP_MEDIA_ROLE);

    /* new sink input unlinked from sink of this card */
    if (role && sink->card == u->card)
        pa_alsa_ucm_roled_stream_end(&u->ucm, role, PA_DIRECTION_OUTPUT);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_unlink_hook_callback(pa_core *c, pa_source_output *source_output, struct userdata *u) {
    const char *role;
    pa_source *source = source_output->source;

    pa_assert(source);

    role = pa_proplist_gets(source_output->proplist, PA_PROP_MEDIA_ROLE);

    /* new source output unlinked from source of this card */
    if (role && source->card == u->card)
        pa_alsa_ucm_roled_stream_end(&u->ucm, role, PA_DIRECTION_INPUT);

    return PA_HOOK_OK;
}

int pa__init(pa_module *m) {
    pa_card_new_data data;
    bool ignore_dB = false;
    struct userdata *u;
    pa_reserve_wrapper *reserve = NULL;
    const char *description;
    const char *profile = NULL;
    char *fn = NULL;
    bool namereg_fail = false;

    pa_alsa_refcnt_inc();

    pa_assert(m);

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->use_ucm = true;
    u->ucm.core = m->core;

    if (!(u->modargs = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    u->device_id = pa_xstrdup(pa_modargs_get_value(u->modargs, "device_id", DEFAULT_DEVICE_ID));

    if ((u->alsa_card_index = snd_card_get_index(u->device_id)) < 0) {
        pa_log("Card '%s' doesn't exist: %s", u->device_id, pa_alsa_strerror(u->alsa_card_index));
        goto fail;
    }

    if (pa_modargs_get_value_boolean(u->modargs, "ignore_dB", &ignore_dB) < 0) {
        pa_log("Failed to parse ignore_dB argument.");
        goto fail;
    }

    if (!pa_in_system_mode()) {
        char *rname;

        if ((rname = pa_alsa_get_reserve_name(u->device_id))) {
            reserve = pa_reserve_wrapper_get(m->core, rname);
            pa_xfree(rname);

            if (!reserve)
                goto fail;
        }
    }

    pa_modargs_get_value_boolean(u->modargs, "use_ucm", &u->use_ucm);
    if (u->use_ucm && !pa_alsa_ucm_query_profiles(&u->ucm, u->alsa_card_index)) {
        pa_log_info("Found UCM profiles");

        u->profile_set = pa_alsa_ucm_add_profile_set(&u->ucm, &u->core->default_channel_map);

        /* hook start of sink input/source output to enable modifiers */
        /* A little bit later than module-role-cork */
        u->sink_input_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT], PA_HOOK_LATE+10,
                (pa_hook_cb_t) sink_input_put_hook_callback, u);
        u->source_output_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PUT], PA_HOOK_LATE+10,
                (pa_hook_cb_t) source_output_put_hook_callback, u);

        /* hook end of sink input/source output to disable modifiers */
        /* A little bit later than module-role-cork */
        u->sink_input_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_UNLINK], PA_HOOK_LATE+10,
                (pa_hook_cb_t) sink_input_unlink_hook_callback, u);
        u->source_output_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_UNLINK], PA_HOOK_LATE+10,
                (pa_hook_cb_t) source_output_unlink_hook_callback, u);
    }
    else {
        u->use_ucm = false;
#ifdef HAVE_UDEV
        fn = pa_udev_get_property(u->alsa_card_index, "PULSE_PROFILE_SET");
#endif

        if (pa_modargs_get_value(u->modargs, "profile_set", NULL)) {
            pa_xfree(fn);
            fn = pa_xstrdup(pa_modargs_get_value(u->modargs, "profile_set", NULL));
        }

        u->profile_set = pa_alsa_profile_set_new(fn, &u->core->default_channel_map);
        pa_xfree(fn);
    }

    if (!u->profile_set)
        goto fail;

    u->profile_set->ignore_dB = ignore_dB;

    pa_alsa_profile_set_probe(u->profile_set, u->device_id, &m->core->default_sample_spec, m->core->default_n_fragments, m->core->default_fragment_size_msec);
    pa_alsa_profile_set_dump(u->profile_set);

    pa_card_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;

    pa_alsa_init_proplist_card(m->core, data.proplist, u->alsa_card_index);

    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, u->device_id);
    pa_alsa_init_description(data.proplist, NULL);
    set_card_name(&data, u->modargs, u->device_id);

    /* We need to give pa_modargs_get_value_boolean() a pointer to a local
     * variable instead of using &data.namereg_fail directly, because
     * data.namereg_fail is a bitfield and taking the address of a bitfield
     * variable is impossible. */
    namereg_fail = data.namereg_fail;
    if (pa_modargs_get_value_boolean(u->modargs, "namereg_fail", &namereg_fail) < 0) {
        pa_log("Failed to parse namereg_fail argument.");
        pa_card_new_data_done(&data);
        goto fail;
    }
    data.namereg_fail = namereg_fail;

    if (reserve)
        if ((description = pa_proplist_gets(data.proplist, PA_PROP_DEVICE_DESCRIPTION)))
            pa_reserve_wrapper_set_application_device_name(reserve, description);

    add_profiles(u, data.profiles, data.ports);

    if (pa_hashmap_isempty(data.profiles)) {
        pa_log("Failed to find a working profile.");
        pa_card_new_data_done(&data);
        goto fail;
    }

    add_disabled_profile(data.profiles);

    if (pa_modargs_get_proplist(u->modargs, "card_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_card_new_data_done(&data);
        goto fail;
    }

    if ((profile = pa_modargs_get_value(u->modargs, "profile", NULL)))
        pa_card_new_data_set_profile(&data, profile);

    u->card = pa_card_new(m->core, &data);
    pa_card_new_data_done(&data);

    if (!u->card)
        goto fail;

    u->card->userdata = u;
    u->card->set_profile = card_set_profile;

    init_jacks(u);
    init_profile(u);
    init_eld_ctls(u);

    if (reserve)
        pa_reserve_wrapper_unref(reserve);

    if (!pa_hashmap_isempty(u->profile_set->decibel_fixes))
        pa_log_warn("Card %s uses decibel fixes (i.e. overrides the decibel information for some alsa volume elements). "
                    "Please note that this feature is meant just as a help for figuring out the correct decibel values. "
                    "PulseAudio is not the correct place to maintain the decibel mappings! The fixed decibel values "
                    "should be sent to ALSA developers so that they can fix the driver. If it turns out that this feature "
                    "is abused (i.e. fixes are not pushed to ALSA), the decibel fix feature may be removed in some future "
                    "PulseAudio version.", u->card->name);

    return 0;

fail:
    if (reserve)
        pa_reserve_wrapper_unref(reserve);

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;
    int n = 0;
    uint32_t idx;
    pa_sink *sink;
    pa_source *source;

    pa_assert(m);
    pa_assert_se(u = m->userdata);
    pa_assert(u->card);

    PA_IDXSET_FOREACH(sink, u->card->sinks, idx)
        n += pa_sink_linked_by(sink);

    PA_IDXSET_FOREACH(source, u->card->sources, idx)
        n += pa_source_linked_by(source);

    return n;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        goto finish;

    if (u->sink_input_put_hook_slot)
        pa_hook_slot_free(u->sink_input_put_hook_slot);

    if (u->sink_input_unlink_hook_slot)
        pa_hook_slot_free(u->sink_input_unlink_hook_slot);

    if (u->source_output_put_hook_slot)
        pa_hook_slot_free(u->source_output_put_hook_slot);

    if (u->source_output_unlink_hook_slot)
        pa_hook_slot_free(u->source_output_unlink_hook_slot);

    if (u->mixer_fdl)
        pa_alsa_fdlist_free(u->mixer_fdl);
    if (u->mixer_handle)
        snd_mixer_close(u->mixer_handle);
    if (u->jacks)
        pa_hashmap_free(u->jacks);

    if (u->card && u->card->sinks)
        pa_idxset_remove_all(u->card->sinks, (pa_free_cb_t) pa_alsa_sink_free);

    if (u->card && u->card->sources)
        pa_idxset_remove_all(u->card->sources, (pa_free_cb_t) pa_alsa_source_free);

    if (u->card)
        pa_card_free(u->card);

    if (u->modargs)
        pa_modargs_free(u->modargs);

    if (u->profile_set)
        pa_alsa_profile_set_free(u->profile_set);

    pa_alsa_ucm_free(&u->ucm);

    pa_xfree(u->device_id);
    pa_xfree(u);

finish:
    pa_alsa_refcnt_dec();
}
