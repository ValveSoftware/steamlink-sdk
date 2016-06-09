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

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <pulse/xmalloc.h>
#include <pulse/volume.h>
#include <pulse/channelmap.h>

#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/sink-input.h>

#include "module-position-event-sounds-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Position event sounds between L and R depending on the position on screen of the widget triggering them.");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    NULL
};

struct userdata {
    pa_hook_slot *sink_input_fixate_hook_slot;
    const char *name;
};

static int parse_pos(const char *pos, double *f) {

    if (pa_atod(pos, f) < 0) {
        pa_log_warn("Failed to parse hpos/vpos property '%s'.", pos);
        return -1;
    }

    if (*f < 0.0 || *f > 1.0) {
        pa_log_debug("Property hpos/vpos out of range %0.2f", *f);

        *f = PA_CLAMP(*f, 0.0, 1.0);
    }

    return 0;
}

static pa_hook_result_t sink_input_fixate_hook_callback(pa_core *core, pa_sink_input_new_data *data, struct userdata *u) {
    const char *hpos, *vpos, *role, *id;
    double f;
    char t[PA_CVOLUME_SNPRINT_VERBOSE_MAX];
    pa_cvolume v;

    pa_assert(data);

    if (!(role = pa_proplist_gets(data->proplist, PA_PROP_MEDIA_ROLE)))
        return PA_HOOK_OK;

    if (!pa_streq(role, "event"))
        return PA_HOOK_OK;

    if ((id = pa_proplist_gets(data->proplist, PA_PROP_EVENT_ID))) {

        /* The test sounds should never be positioned in space, since
         * they might be triggered themselves to configure the speakers
         * in space, which we don't want to mess up. */

        if (pa_startswith(id, "audio-channel-"))
            return PA_HOOK_OK;

        if (pa_streq(id, "audio-volume-change"))
            return PA_HOOK_OK;

        if (pa_streq(id, "audio-test-signal"))
            return PA_HOOK_OK;
    }

    if (!(hpos = pa_proplist_gets(data->proplist, PA_PROP_EVENT_MOUSE_HPOS)))
        hpos = pa_proplist_gets(data->proplist, PA_PROP_WINDOW_HPOS);

    if (!(vpos = pa_proplist_gets(data->proplist, PA_PROP_EVENT_MOUSE_VPOS)))
        vpos = pa_proplist_gets(data->proplist, PA_PROP_WINDOW_VPOS);

    if (!hpos && !vpos)
        return PA_HOOK_OK;

    pa_cvolume_reset(&v, data->sink->sample_spec.channels);

    if (hpos) {
        if (parse_pos(hpos, &f) < 0)
            return PA_HOOK_OK;

        if (pa_channel_map_can_balance(&data->sink->channel_map)) {
            pa_log_debug("Positioning event sound '%s' horizontally at %0.2f.", pa_strnull(pa_proplist_gets(data->proplist, PA_PROP_EVENT_ID)), f);
            pa_cvolume_set_balance(&v, &data->sink->channel_map, f*2.0-1.0);
        }
    }

    if (vpos) {
        if (parse_pos(vpos, &f) < 0)
            return PA_HOOK_OK;

        if (pa_channel_map_can_fade(&data->sink->channel_map)) {
            pa_log_debug("Positioning event sound '%s' vertically at %0.2f.", pa_strnull(pa_proplist_gets(data->proplist, PA_PROP_EVENT_ID)), f);
            pa_cvolume_set_fade(&v, &data->sink->channel_map, f*2.0-1.0);
        }
    }

    pa_log_debug("Final volume factor %s.",
                 pa_cvolume_snprint_verbose(t,
                                            sizeof(t),
                                            &v,
                                            &data->sink->channel_map,
                                            data->sink->flags & PA_SINK_DECIBEL_VOLUME));
    pa_sink_input_new_data_add_volume_factor_sink(data, u->name, &v);

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->sink_input_fixate_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_FIXATE], PA_HOOK_EARLY, (pa_hook_cb_t) sink_input_fixate_hook_callback, u);

    pa_modargs_free(ma);
    u->name = m->name;

    return 0;

fail:
    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_input_fixate_hook_slot)
        pa_hook_slot_free(u->sink_input_fixate_hook_slot);

    pa_xfree(u);
}
