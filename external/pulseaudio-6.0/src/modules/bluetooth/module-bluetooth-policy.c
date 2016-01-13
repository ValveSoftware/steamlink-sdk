/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2009 Canonical Ltd
  Copyright (C) 2012 Intel Corporation

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
#include <pulsecore/modargs.h>
#include <pulsecore/source-output.h>
#include <pulsecore/source.h>
#include <pulsecore/core-util.h>

#include "module-bluetooth-policy-symdef.h"

PA_MODULE_AUTHOR("Frédéric Dalleau");
PA_MODULE_DESCRIPTION("When a bluetooth sink or source is added, load module-loopback");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "a2dp_source=<Handle a2dp_source card profile (sink role)?> "
        "ag=<Handle headset_audio_gateway card profile (headset role)?> "
        "hfgw=<Handle hfgw card profile (headset role)?> DEPRECATED");

static const char* const valid_modargs[] = {
    "a2dp_source",
    "ag",
    "hfgw",
    NULL
};

struct userdata {
    bool enable_a2dp_source;
    bool enable_ag;
    pa_hook_slot *source_put_slot;
    pa_hook_slot *sink_put_slot;
    pa_hook_slot *profile_available_changed_slot;
};

/* When a source is created, loopback it to default sink */
static pa_hook_result_t source_put_hook_callback(pa_core *c, pa_source *source, void *userdata) {
    struct userdata *u = userdata;
    const char *s;
    const char *role;
    char *args;

    pa_assert(c);
    pa_assert(source);

    /* Only consider bluetooth sinks and sources */
    s = pa_proplist_gets(source->proplist, PA_PROP_DEVICE_BUS);
    if (!s)
        return PA_HOOK_OK;

    if (!pa_streq(s, "bluetooth"))
        return PA_HOOK_OK;

    s = pa_proplist_gets(source->proplist, "bluetooth.protocol");
    if (!s)
        return PA_HOOK_OK;

    if (u->enable_a2dp_source && pa_streq(s, "a2dp_source"))
        role = "music";
    /* TODO: remove hfgw when we remove BlueZ 4 support */
    else if (u->enable_ag && (pa_streq(s, "hfgw") || pa_streq(s, "headset_audio_gateway")))
        role = "phone";
    else {
        pa_log_debug("Profile %s cannot be selected for loopback", s);
        return PA_HOOK_OK;
    }

    /* Load module-loopback */
    args = pa_sprintf_malloc("source=\"%s\" source_dont_move=\"true\" sink_input_properties=\"media.role=%s\"", source->name,
                             role);
    (void) pa_module_load(c, "module-loopback", args);
    pa_xfree(args);

    return PA_HOOK_OK;
}

/* When a sink is created, loopback it to default source */
static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, void *userdata) {
    struct userdata *u = userdata;
    const char *s;
    const char *role;
    char *args;

    pa_assert(c);
    pa_assert(sink);

    /* Only consider bluetooth sinks and sources */
    s = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_BUS);
    if (!s)
        return PA_HOOK_OK;

    if (!pa_streq(s, "bluetooth"))
        return PA_HOOK_OK;

    s = pa_proplist_gets(sink->proplist, "bluetooth.protocol");
    if (!s)
        return PA_HOOK_OK;

    /* TODO: remove hfgw when we remove BlueZ 4 support */
    if (u->enable_ag && (pa_streq(s, "hfgw") || pa_streq(s, "headset_audio_gateway")))
        role = "phone";
    else {
        pa_log_debug("Profile %s cannot be selected for loopback", s);
        return PA_HOOK_OK;
    }

    /* Load module-loopback */
    args = pa_sprintf_malloc("sink=\"%s\" sink_dont_move=\"true\" source_output_properties=\"media.role=%s\"", sink->name,
                             role);
    (void) pa_module_load(c, "module-loopback", args);
    pa_xfree(args);

    return PA_HOOK_OK;
}

static pa_card_profile *find_best_profile(pa_card *card) {
    void *state;
    pa_card_profile *profile;
    pa_card_profile *result = card->active_profile;

    PA_HASHMAP_FOREACH(profile, card->profiles, state) {
        if (profile->available == PA_AVAILABLE_NO)
            continue;

        if (result == NULL ||
            (profile->available == PA_AVAILABLE_YES && result->available == PA_AVAILABLE_UNKNOWN) ||
            (profile->available == result->available && profile->priority > result->priority))
            result = profile;
    }

    return result;
}

static pa_hook_result_t profile_available_hook_callback(pa_core *c, pa_card_profile *profile, void *userdata) {
    pa_card *card;
    const char *s;
    bool is_active_profile;
    pa_card_profile *selected_profile;

    pa_assert(c);
    pa_assert(profile);
    pa_assert_se((card = profile->card));

    /* Only consider bluetooth cards */
    s = pa_proplist_gets(card->proplist, PA_PROP_DEVICE_BUS);
    if (!s || !pa_streq(s, "bluetooth"))
        return PA_HOOK_OK;

    /* Do not automatically switch profiles for headsets, just in case */
    /* TODO: remove a2dp and hsp when we remove BlueZ 4 support */
    if (pa_streq(profile->name, "hsp") || pa_streq(profile->name, "a2dp") || pa_streq(profile->name, "a2dp_sink") ||
        pa_streq(profile->name, "headset_head_unit"))
        return PA_HOOK_OK;

    is_active_profile = card->active_profile == profile;

    if (profile->available == PA_AVAILABLE_YES) {
        if (is_active_profile)
            return PA_HOOK_OK;

        if (card->active_profile->available == PA_AVAILABLE_YES && card->active_profile->priority >= profile->priority)
            return PA_HOOK_OK;

        selected_profile = profile;
    } else {
        if (!is_active_profile)
            return PA_HOOK_OK;

        pa_assert_se((selected_profile = find_best_profile(card)));

        if (selected_profile == card->active_profile)
            return PA_HOOK_OK;
    }

    pa_log_debug("Setting card '%s' to profile '%s'", card->name, selected_profile->name);

    if (pa_card_set_profile(card, selected_profile, false) != 0)
        pa_log_warn("Could not set profile '%s'", selected_profile->name);

    return PA_HOOK_OK;
}

static void handle_all_profiles(pa_core *core) {
    pa_card *card;
    uint32_t state;

    PA_IDXSET_FOREACH(card, core->cards, state) {
        pa_card_profile *profile;
        void *state2;

        PA_HASHMAP_FOREACH(profile, card->profiles, state2)
            profile_available_hook_callback(core, profile, NULL);
    }
}

int pa__init(pa_module *m) {
    pa_modargs *ma;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log_error("Failed to parse module arguments");
        return -1;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);

    u->enable_a2dp_source = true;
    if (pa_modargs_get_value_boolean(ma, "a2dp_source", &u->enable_a2dp_source) < 0) {
        pa_log("Failed to parse a2dp_source argument.");
        goto fail;
    }

    u->enable_ag = true;
    if (pa_modargs_get_value_boolean(ma, "hfgw", &u->enable_ag) < 0) {
        pa_log("Failed to parse hfgw argument.");
        goto fail;
    }
    if (pa_modargs_get_value_boolean(ma, "ag", &u->enable_ag) < 0) {
        pa_log("Failed to parse ag argument.");
        goto fail;
    }

    u->source_put_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_NORMAL,
                                         (pa_hook_cb_t) source_put_hook_callback, u);

    u->sink_put_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_NORMAL,
                                       (pa_hook_cb_t) sink_put_hook_callback, u);

    u->profile_available_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_AVAILABLE_CHANGED],
                                                        PA_HOOK_NORMAL, (pa_hook_cb_t) profile_available_hook_callback, u);

    handle_all_profiles(m->core);

    pa_modargs_free(ma);
    return 0;

fail:
    pa_modargs_free(ma);
    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->source_put_slot)
        pa_hook_slot_free(u->source_put_slot);

    if (u->sink_put_slot)
        pa_hook_slot_free(u->sink_put_slot);

    if (u->profile_available_changed_slot)
        pa_hook_slot_free(u->profile_available_changed_slot);

    pa_xfree(u);
}
