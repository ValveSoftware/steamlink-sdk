/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2009 Canonical Ltd

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
#include <pulsecore/source.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-util.h>

#include "module-switch-on-connect-symdef.h"

PA_MODULE_AUTHOR("Michael Terry");
PA_MODULE_DESCRIPTION("When a sink/source is added, switch to it or conditionally switch to it");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "only_from_unavailable=<boolean, only switch from unavailable ports> "
);

static const char* const valid_modargs[] = {
    "only_from_unavailable",
    NULL,
};

struct userdata {
    bool only_from_unavailable;
};

static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    pa_sink_input *i;
    uint32_t idx;
    pa_sink *def;
    const char *s;
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(userdata);

    /* Don't want to run during startup or shutdown */
    if (c->state != PA_CORE_RUNNING)
        return PA_HOOK_OK;

    /* Don't switch to any internal devices */
    if ((s = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_BUS))) {
        if (pa_streq(s, "pci"))
            return PA_HOOK_OK;
        else if (pa_streq(s, "isa"))
            return PA_HOOK_OK;
    }

    def = pa_namereg_get_default_sink(c);
    if (def == sink)
        return PA_HOOK_OK;

    if (u->only_from_unavailable)
        if (!def->active_port || def->active_port->available != PA_AVAILABLE_NO)
            return PA_HOOK_OK;

    /* Actually do the switch to the new sink */
    pa_namereg_set_default_sink(c, sink);

    /* Now move all old inputs over */
    if (pa_idxset_size(def->inputs) <= 0) {
        pa_log_debug("No sink inputs to move away.");
        return PA_HOOK_OK;
    }

    PA_IDXSET_FOREACH(i, def->inputs, idx) {
        if (i->save_sink || !PA_SINK_INPUT_IS_LINKED(i->state))
            continue;

        if (pa_sink_input_move_to(i, sink, false) < 0)
            pa_log_info("Failed to move sink input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), sink->name);
        else
            pa_log_info("Successfully moved sink input %u \"%s\" to %s.", i->index,
                        pa_strnull(pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME)), sink->name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_put_hook_callback(pa_core *c, pa_source *source, void* userdata) {
    pa_source_output *o;
    uint32_t idx;
    pa_source *def;
    const char *s;
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(source);
    pa_assert(userdata);

    /* Don't want to run during startup or shutdown */
    if (c->state != PA_CORE_RUNNING)
        return PA_HOOK_OK;

    /* Don't switch to a monitoring source */
    if (source->monitor_of)
        return PA_HOOK_OK;

    /* Don't switch to any internal devices */
    if ((s = pa_proplist_gets(source->proplist, PA_PROP_DEVICE_BUS))) {
        if (pa_streq(s, "pci"))
            return PA_HOOK_OK;
        else if (pa_streq(s, "isa"))
            return PA_HOOK_OK;
    }

    def = pa_namereg_get_default_source(c);
    if (def == source)
        return PA_HOOK_OK;

    if (u->only_from_unavailable)
        if (!def->active_port || def->active_port->available != PA_AVAILABLE_NO)
            return PA_HOOK_OK;

    /* Actually do the switch to the new source */
    pa_namereg_set_default_source(c, source);

    /* Now move all old outputs over */
    if (pa_idxset_size(def->outputs) <= 0) {
        pa_log_debug("No source outputs to move away.");
        return PA_HOOK_OK;
    }

    PA_IDXSET_FOREACH(o, def->outputs, idx) {
        if (o->save_source || !PA_SOURCE_OUTPUT_IS_LINKED(o->state))
            continue;

        if (pa_source_output_move_to(o, source, false) < 0)
            pa_log_info("Failed to move source output %u \"%s\" to %s.", o->index,
                        pa_strnull(pa_proplist_gets(o->proplist, PA_PROP_APPLICATION_NAME)), source->name);
        else
            pa_log_info("Successfully moved source output %u \"%s\" to %s.", o->index,
                        pa_strnull(pa_proplist_gets(o->proplist, PA_PROP_APPLICATION_NAME)), source->name);
    }

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        return -1;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);

    /* A little bit later than module-rescue-streams... */
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE+30, (pa_hook_cb_t) sink_put_hook_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE+20, (pa_hook_cb_t) source_put_hook_callback, u);

    if (pa_modargs_get_value_boolean(ma, "only_from_unavailable", &u->only_from_unavailable) < 0) {
	pa_log("Failed to get a boolean value for only_from_unavailable.");
	goto fail;
    }

    pa_modargs_free(ma);
    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    pa_xfree(u);
}
