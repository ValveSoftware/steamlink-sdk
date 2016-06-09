/***
    This file is part of PulseAudio.

    Copyright 2008 Colin Guthrie

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
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/sink.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>

#include "module-always-sink-symdef.h"

PA_MODULE_AUTHOR("Colin Guthrie");
PA_MODULE_DESCRIPTION(_("Always keeps at least one sink loaded even if it's a null one"));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "sink_name=<name of sink>");

#define DEFAULT_SINK_NAME "auto_null"

static const char* const valid_modargs[] = {
    "sink_name",
    NULL,
};

struct userdata {
    uint32_t null_module;
    bool ignore;
    char *sink_name;
};

static void load_null_sink_if_needed(pa_core *c, pa_sink *sink, struct userdata* u) {
    pa_sink *target;
    uint32_t idx;
    char *t;
    pa_module *m;

    pa_assert(c);
    pa_assert(u);

    if (u->null_module != PA_INVALID_INDEX)
        return; /* We've already got a null-sink loaded */

    /* Loop through all sinks and check to see if we have *any*
     * sinks. Ignore the sink passed in (if it's not null), and
     * don't count filter sinks. */
    PA_IDXSET_FOREACH(target, c->sinks, idx)
        if (!sink || ((target != sink) && !pa_sink_is_filter(target)))
            break;

    if (target)
        return;

    pa_log_debug("Autoloading null-sink as no other sinks detected.");

    u->ignore = true;

    t = pa_sprintf_malloc("sink_name=%s sink_properties='device.description=\"%s\"'", u->sink_name,
                          _("Dummy Output"));
    m = pa_module_load(c, "module-null-sink", t);
    u->null_module = m ? m->index : PA_INVALID_INDEX;
    pa_xfree(t);

    u->ignore = false;

    if (!m)
        pa_log_warn("Unable to load module-null-sink");
}

static pa_hook_result_t put_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);

    /* This is us detecting ourselves on load... just ignore this. */
    if (u->ignore)
        return PA_HOOK_OK;

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    /* Auto-loaded null-sink not active, so ignoring newly detected sink. */
    if (u->null_module == PA_INVALID_INDEX)
        return PA_HOOK_OK;

    /* This is us detecting ourselves on load in a different way... just ignore this too. */
    if (sink->module && sink->module->index == u->null_module)
        return PA_HOOK_OK;

    /* We don't count filter sinks since they need a real sink */
    if (pa_sink_is_filter(sink))
        return PA_HOOK_OK;

    pa_log_info("A new sink has been discovered. Unloading null-sink.");

    pa_module_unload_request_by_index(c, u->null_module, true);
    u->null_module = PA_INVALID_INDEX;

    return PA_HOOK_OK;
}

static pa_hook_result_t unlink_hook_callback(pa_core *c, pa_sink *sink, void* userdata) {
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);

    /* First check to see if it's our own null-sink that's been removed... */
    if (u->null_module != PA_INVALID_INDEX && sink->module && sink->module->index == u->null_module) {
        pa_log_debug("Autoloaded null-sink removed");
        u->null_module = PA_INVALID_INDEX;
        return PA_HOOK_OK;
    }

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    load_null_sink_if_needed(c, sink, u);

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        return -1;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->sink_name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE, (pa_hook_cb_t) put_hook_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_EARLY, (pa_hook_cb_t) unlink_hook_callback, u);
    u->null_module = PA_INVALID_INDEX;
    u->ignore = false;

    pa_modargs_free(ma);

    load_null_sink_if_needed(m->core, NULL, u);

    return 0;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->null_module != PA_INVALID_INDEX && m->core->state != PA_CORE_SHUTDOWN)
        pa_module_unload_request_by_index(m->core, u->null_module, true);

    pa_xfree(u->sink_name);
    pa_xfree(u);
}
