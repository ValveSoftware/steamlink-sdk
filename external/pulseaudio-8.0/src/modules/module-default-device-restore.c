/***
  This file is part of PulseAudio.

  Copyright 2006-2008 Lennart Poettering

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

#include <errno.h>
#include <stdio.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core-util.h>
#include <pulsecore/module.h>
#include <pulsecore/log.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-error.h>

#include "module-default-device-restore-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Automatically restore the default sink and source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define SAVE_INTERVAL (5 * PA_USEC_PER_SEC)

struct userdata {
    pa_core *core;
    pa_subscription *subscription;
    pa_time_event *time_event;
    char *sink_filename, *source_filename;
    bool modified;
};

static void load(struct userdata *u) {
    FILE *f;

    /* We never overwrite manually configured settings */

    if (u->core->default_sink)
        pa_log_info("Manually configured default sink, not overwriting.");
    else if ((f = pa_fopen_cloexec(u->sink_filename, "r"))) {
        char ln[256] = "";
        pa_sink *s;

        if (fgets(ln, sizeof(ln)-1, f))
            pa_strip_nl(ln);
        fclose(f);

        if (!ln[0])
            pa_log_info("No previous default sink setting, ignoring.");
        else if ((s = pa_namereg_get(u->core, ln, PA_NAMEREG_SINK))) {
            pa_namereg_set_default_sink(u->core, s);
            pa_log_info("Restored default sink '%s'.", ln);
        } else
            pa_log_info("Saved default sink '%s' not existent, not restoring default sink setting.", ln);

    } else if (errno != ENOENT)
        pa_log("Failed to load default sink: %s", pa_cstrerror(errno));

    if (u->core->default_source)
        pa_log_info("Manually configured default source, not overwriting.");
    else if ((f = pa_fopen_cloexec(u->source_filename, "r"))) {
        char ln[256] = "";
        pa_source *s;

        if (fgets(ln, sizeof(ln)-1, f))
            pa_strip_nl(ln);
        fclose(f);

        if (!ln[0])
            pa_log_info("No previous default source setting, ignoring.");
        else if ((s = pa_namereg_get(u->core, ln, PA_NAMEREG_SOURCE))) {
            pa_namereg_set_default_source(u->core, s);
            pa_log_info("Restored default source '%s'.", ln);
        } else
            pa_log_info("Saved default source '%s' not existent, not restoring default source setting.", ln);

    } else if (errno != ENOENT)
            pa_log("Failed to load default sink: %s", pa_cstrerror(errno));
}

static void save(struct userdata *u) {
    FILE *f;

    if (!u->modified)
        return;

    if (u->sink_filename) {
        if ((f = pa_fopen_cloexec(u->sink_filename, "w"))) {
            pa_sink *s = pa_namereg_get_default_sink(u->core);
            fprintf(f, "%s\n", s ? s->name : "");
            fclose(f);
        } else
            pa_log("Failed to save default sink: %s", pa_cstrerror(errno));
    }

    if (u->source_filename) {
        if ((f = pa_fopen_cloexec(u->source_filename, "w"))) {
            pa_source *s = pa_namereg_get_default_source(u->core);
            fprintf(f, "%s\n", s ? s->name : "");
            fclose(f);
        } else
            pa_log("Failed to save default source: %s", pa_cstrerror(errno));
    }

    u->modified = false;
}

static void time_cb(pa_mainloop_api *a, pa_time_event *e, const struct timeval *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);
    save(u);

    if (u->time_event) {
        u->core->mainloop->time_free(u->time_event);
        u->time_event = NULL;
    }
}

static void subscribe_cb(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    u->modified = true;

    if (!u->time_event)
        u->time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + SAVE_INTERVAL, time_cb, u);
}

int pa__init(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;

    if (!(u->sink_filename = pa_state_path("default-sink", true)))
        goto fail;

    if (!(u->source_filename = pa_state_path("default-source", true)))
        goto fail;

    load(u);

    u->subscription = pa_subscription_new(u->core, PA_SUBSCRIPTION_MASK_SERVER, subscribe_cb, u);

    return 0;

fail:
    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    save(u);

    if (u->subscription)
        pa_subscription_free(u->subscription);

    if (u->time_event)
        m->core->mainloop->time_free(u->time_event);

    pa_xfree(u->sink_filename);
    pa_xfree(u->source_filename);
    pa_xfree(u);
}
