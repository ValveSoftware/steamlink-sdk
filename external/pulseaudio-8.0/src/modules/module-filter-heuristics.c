/***
  This file is part of PulseAudio.

  Copyright 2011 Colin Guthrie

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
#include <pulsecore/hook-list.h>
#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/modargs.h>

#include "module-filter-heuristics-symdef.h"

#define PA_PROP_FILTER_APPLY_MOVING "filter.apply.moving"
#define PA_PROP_FILTER_HEURISTICS "filter.heuristics"

PA_MODULE_AUTHOR("Colin Guthrie");
PA_MODULE_DESCRIPTION("Detect when various filters are desirable");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    NULL
};

struct userdata {
    pa_core *core;
};

static pa_hook_result_t process(struct userdata *u, pa_object *o, bool is_sink_input) {
    const char *want;
    pa_proplist *pl, *parent_pl;

    if (is_sink_input) {
        pl = PA_SINK_INPUT(o)->proplist;
        parent_pl = PA_SINK_INPUT(o)->sink->proplist;
    } else {
        pl = PA_SOURCE_OUTPUT(o)->proplist;
        parent_pl = PA_SOURCE_OUTPUT(o)->source->proplist;
    }

    /* If the stream already specifies what it must have, then let it be. */
    if (!pa_proplist_gets(pl, PA_PROP_FILTER_HEURISTICS) && pa_proplist_gets(pl, PA_PROP_FILTER_APPLY))
        return PA_HOOK_OK;

    /* On phone sinks, make sure we're not applying echo cancellation */
    if (pa_str_in_list_spaces(pa_proplist_gets(parent_pl, PA_PROP_DEVICE_INTENDED_ROLES), "phone")) {
        const char *apply = pa_proplist_gets(pl, PA_PROP_FILTER_APPLY);

        if (apply && pa_streq(apply, "echo-cancel")) {
            pa_proplist_unset(pl, PA_PROP_FILTER_APPLY);
            pa_proplist_unset(pl, PA_PROP_FILTER_HEURISTICS);
        }

        return PA_HOOK_OK;
    }

    want = pa_proplist_gets(pl, PA_PROP_FILTER_WANT);

    if (want) {
        /* There's a filter that we want, ask module-filter-apply to apply it, and remember that we're managing filter.apply */
        pa_proplist_sets(pl, PA_PROP_FILTER_APPLY, want);
        pa_proplist_sets(pl, PA_PROP_FILTER_HEURISTICS, "1");
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_put_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);
    pa_assert(u);

    return process(u, PA_OBJECT(i), true);
}

static pa_hook_result_t sink_input_move_finish_cb(pa_core *core, pa_sink_input *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_sink_input_assert_ref(i);
    pa_assert(u);

    /* module-filter-apply triggered this move, ignore */
    if (pa_proplist_gets(i->proplist, PA_PROP_FILTER_APPLY_MOVING))
        return PA_HOOK_OK;

    return process(u, PA_OBJECT(i), true);
}

static pa_hook_result_t source_output_put_cb(pa_core *core, pa_source_output *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_source_output_assert_ref(i);
    pa_assert(u);

    return process(u, PA_OBJECT(i), false);
}

static pa_hook_result_t source_output_move_finish_cb(pa_core *core, pa_source_output *i, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_source_output_assert_ref(i);
    pa_assert(u);

    /* module-filter-apply triggered this move, ignore */
    if (pa_proplist_gets(i->proplist, PA_PROP_FILTER_APPLY_MOVING))
        return PA_HOOK_OK;

    return process(u, PA_OBJECT(i), false);
}

int pa__init(pa_module *m) {
    pa_modargs *ma = NULL;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);

    u->core = m->core;

    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT], PA_HOOK_LATE-1, (pa_hook_cb_t) sink_input_put_cb, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FINISH], PA_HOOK_LATE-1, (pa_hook_cb_t) sink_input_move_finish_cb, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PUT], PA_HOOK_LATE-1, (pa_hook_cb_t) source_output_put_cb, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_MOVE_FINISH], PA_HOOK_LATE-1, (pa_hook_cb_t) source_output_move_finish_cb, u);

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

    pa_xfree(u);

}
