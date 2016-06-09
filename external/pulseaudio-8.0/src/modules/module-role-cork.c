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

#include <pulsecore/macro.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/modargs.h>

#include "module-role-cork-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Mute & cork streams with certain roles while others exist");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "trigger_roles=<Comma separated list of roles which will trigger a cork> "
        "cork_roles=<Comma separated list of roles which will be corked> "
        "global=<Should we operate globally or only inside the same device?>");

static const char* const valid_modargs[] = {
    "trigger_roles",
    "cork_roles",
    "global",
    NULL
};

struct userdata {
    pa_core *core;
    pa_hashmap *cork_state;
    pa_idxset *trigger_roles;
    pa_idxset *cork_roles;
    bool global:1;
    pa_hook_slot
        *sink_input_put_slot,
        *sink_input_unlink_slot,
        *sink_input_move_start_slot,
        *sink_input_move_finish_slot;
};

static bool shall_cork(struct userdata *u, pa_sink *s, pa_sink_input *ignore) {
    pa_sink_input *j;
    uint32_t idx, role_idx;
    const char *trigger_role;

    pa_assert(u);
    pa_sink_assert_ref(s);

    for (j = PA_SINK_INPUT(pa_idxset_first(s->inputs, &idx)); j; j = PA_SINK_INPUT(pa_idxset_next(s->inputs, &idx))) {
        const char *role;

        if (j == ignore)
            continue;

        if (!(role = pa_proplist_gets(j->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        PA_IDXSET_FOREACH(trigger_role, u->trigger_roles, role_idx) {
            if (pa_streq(role, trigger_role)) {
                pa_log_debug("Found a '%s' stream that will trigger the auto-cork.", trigger_role);
                return true;
            }
        }
    }

    return false;
}

static inline void apply_cork_to_sink(struct userdata *u, pa_sink *s, pa_sink_input *ignore, bool cork) {
    pa_sink_input *j;
    uint32_t idx, role_idx;
    const char *cork_role;
    bool trigger = false;

    pa_assert(u);
    pa_sink_assert_ref(s);

    for (j = PA_SINK_INPUT(pa_idxset_first(s->inputs, &idx)); j; j = PA_SINK_INPUT(pa_idxset_next(s->inputs, &idx))) {
        bool corked, corked_here;
        const char *role;

        if (j == ignore)
            continue;

        if (!(role = pa_proplist_gets(j->proplist, PA_PROP_MEDIA_ROLE)))
            continue;

        PA_IDXSET_FOREACH(cork_role, u->cork_roles, role_idx) {
            if ((trigger = pa_streq(role, cork_role)))
                break;
        }
        if (!trigger)
            continue;

        corked = (pa_sink_input_get_state(j) == PA_SINK_INPUT_CORKED);
        corked_here = !!pa_hashmap_get(u->cork_state, j);

        if (cork && !corked && !j->muted) {
            pa_log_debug("Found a '%s' stream that should be corked/muted.", cork_role);
            if (!corked_here)
                pa_hashmap_put(u->cork_state, j, PA_INT_TO_PTR(1));
            pa_sink_input_set_mute(j, true, false);
            pa_sink_input_send_event(j, PA_STREAM_EVENT_REQUEST_CORK, NULL);
        } else if (!cork) {
            pa_hashmap_remove(u->cork_state, j);

            if (corked_here && (corked || j->muted)) {
                pa_log_debug("Found a '%s' stream that should be uncorked/unmuted.", cork_role);
                if (j->muted)
                    pa_sink_input_set_mute(j, false, false);
                if (corked)
                    pa_sink_input_send_event(j, PA_STREAM_EVENT_REQUEST_UNCORK, NULL);
            }
        }
    }
}

static void apply_cork(struct userdata *u, pa_sink *s, pa_sink_input *ignore, bool cork) {
    pa_assert(u);

    if (u->global) {
        uint32_t idx;
        PA_IDXSET_FOREACH(s, u->core->sinks, idx)
            apply_cork_to_sink(u, s, ignore, cork);
    } else
        apply_cork_to_sink(u, s, ignore, cork);
}

static pa_hook_result_t process(struct userdata *u, pa_sink_input *i, bool create) {
    bool cork = false;
    const char *role;

    pa_assert(u);
    pa_sink_input_assert_ref(i);

    if (!create)
        pa_hashmap_remove(u->cork_state, i);

    if (!(role = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_ROLE)))
        return PA_HOOK_OK;

    if (!i->sink)
        return PA_HOOK_OK;

    cork = shall_cork(u, i->sink, create ? NULL : i);
    apply_cork(u, i->sink, create ? NULL : i, cork);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_put_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);

    return process(u, i, true);
}

static pa_hook_result_t sink_input_unlink_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_sink_input_assert_ref(i);

    return process(u, i, false);
}

static pa_hook_result_t sink_input_move_start_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);

    return process(u, i, false);
}

static pa_hook_result_t sink_input_move_finish_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);

    return process(u, i, true);
}

int pa__init(pa_module *m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    const char *roles;
    bool global = false;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);

    u->core = m->core;
    u->cork_state = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->trigger_roles = pa_idxset_new(NULL, NULL);
    roles = pa_modargs_get_value(ma, "trigger_roles", NULL);
    if (roles) {
        const char *split_state = NULL;
        char *n = NULL;
        while ((n = pa_split(roles, ",", &split_state))) {
            if (n[0] != '\0')
                pa_idxset_put(u->trigger_roles, n, NULL);
            else
                pa_xfree(n);
        }
    }
    if (pa_idxset_isempty(u->trigger_roles)) {
        pa_log_debug("Using role 'phone' as trigger role.");
        pa_idxset_put(u->trigger_roles, pa_xstrdup("phone"), NULL);
    }

    u->cork_roles = pa_idxset_new(NULL, NULL);
    roles = pa_modargs_get_value(ma, "cork_roles", NULL);
    if (roles) {
        const char *split_state = NULL;
        char *n = NULL;
        while ((n = pa_split(roles, ",", &split_state))) {
            if (n[0] != '\0')
                pa_idxset_put(u->cork_roles, n, NULL);
            else
                pa_xfree(n);
        }
    }
    if (pa_idxset_isempty(u->cork_roles)) {
        pa_log_debug("Using roles 'music' and 'video' as cork roles.");
        pa_idxset_put(u->cork_roles, pa_xstrdup("music"), NULL);
        pa_idxset_put(u->cork_roles, pa_xstrdup("video"), NULL);
    }

    if (pa_modargs_get_value_boolean(ma, "global", &global) < 0) {
        pa_log("Invalid boolean parameter: global");
        goto fail;
    }
    u->global = global;

    u->sink_input_put_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_put_cb, u);
    u->sink_input_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_unlink_cb, u);
    u->sink_input_move_start_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_START], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_move_start_cb, u);
    u->sink_input_move_finish_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FINISH], PA_HOOK_LATE, (pa_hook_cb_t) sink_input_move_finish_cb, u);

    pa_modargs_free(ma);

    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;

}

void pa__done(pa_module *m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->trigger_roles)
        pa_idxset_free(u->trigger_roles, pa_xfree);

    if (u->cork_roles)
        pa_idxset_free(u->cork_roles, pa_xfree);

    if (u->sink_input_put_slot)
        pa_hook_slot_free(u->sink_input_put_slot);
    if (u->sink_input_unlink_slot)
        pa_hook_slot_free(u->sink_input_unlink_slot);
    if (u->sink_input_move_start_slot)
        pa_hook_slot_free(u->sink_input_move_start_slot);
    if (u->sink_input_move_finish_slot)
        pa_hook_slot_free(u->sink_input_move_finish_slot);

    if (u->cork_state)
        pa_hashmap_free(u->cork_state);

    pa_xfree(u);

}
