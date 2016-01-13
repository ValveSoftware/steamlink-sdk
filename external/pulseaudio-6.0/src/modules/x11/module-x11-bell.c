/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

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

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <pulse/xmalloc.h>

#include <pulsecore/core-scache.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/x11wrap.h>

#include "module-x11-bell-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("X11 bell interceptor");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE("sink=<sink to connect to> sample=<sample name> display=<X11 display>");

static const char* const valid_modargs[] = {
    "sink",
    "sample",
    "display",
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;

    int xkb_event_base;

    char *sink_name;
    char *scache_item;

    pa_x11_wrapper *x11_wrapper;
    pa_x11_client *x11_client;
};

static int x11_event_cb(pa_x11_wrapper *w, XEvent *e, void *userdata) {
    XkbBellNotifyEvent *bne;
    struct userdata *u = userdata;

    pa_assert(w);
    pa_assert(e);
    pa_assert(u);
    pa_assert(u->x11_wrapper == w);

    if (((XkbEvent*) e)->any.xkb_type != XkbBellNotify)
        return 0;

    bne = (XkbBellNotifyEvent*) e;

    if (pa_scache_play_item_by_name(u->core, u->scache_item, u->sink_name, ((pa_volume_t) bne->percent*PA_VOLUME_NORM)/100U, NULL, NULL) < 0) {
        pa_log_info("Ringing bell failed, reverting to X11 device bell.");
        XkbForceDeviceBell(pa_x11_wrapper_get_display(w), bne->device, bne->bell_class, bne->bell_id, bne->percent);
    }

    return 1;
}

static void x11_kill_cb(pa_x11_wrapper *w, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(w);
    pa_assert(u);
    pa_assert(u->x11_wrapper == w);

    if (u->x11_client)
        pa_x11_client_free(u->x11_client);

    if (u->x11_wrapper)
        pa_x11_wrapper_unref(u->x11_wrapper);

    u->x11_client = NULL;
    u->x11_wrapper = NULL;

    pa_module_unload_request(u->module, true);
}

int pa__init(pa_module*m) {

    struct userdata *u = NULL;
    pa_modargs *ma = NULL;
    int major, minor;
    unsigned int auto_ctrls, auto_values;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->scache_item = pa_xstrdup(pa_modargs_get_value(ma, "sample", "bell-window-system"));
    u->sink_name = pa_xstrdup(pa_modargs_get_value(ma, "sink", NULL));
    u->x11_client = NULL;

    if (!(u->x11_wrapper = pa_x11_wrapper_get(m->core, pa_modargs_get_value(ma, "display", NULL))))
        goto fail;

    major = XkbMajorVersion;
    minor = XkbMinorVersion;

    if (!XkbLibraryVersion(&major, &minor)) {
        pa_log("XkbLibraryVersion() failed");
        goto fail;
    }

    major = XkbMajorVersion;
    minor = XkbMinorVersion;

    if (!XkbQueryExtension(pa_x11_wrapper_get_display(u->x11_wrapper), NULL, &u->xkb_event_base, NULL, &major, &minor)) {
        pa_log("XkbQueryExtension() failed");
        goto fail;
    }

    XkbSelectEvents(pa_x11_wrapper_get_display(u->x11_wrapper), XkbUseCoreKbd, XkbBellNotifyMask, XkbBellNotifyMask);
    auto_ctrls = auto_values = XkbAudibleBellMask;
    XkbSetAutoResetControls(pa_x11_wrapper_get_display(u->x11_wrapper), XkbAudibleBellMask, &auto_ctrls, &auto_values);
    XkbChangeEnabledControls(pa_x11_wrapper_get_display(u->x11_wrapper), XkbUseCoreKbd, XkbAudibleBellMask, 0);

    u->x11_client = pa_x11_client_new(u->x11_wrapper, x11_event_cb, x11_kill_cb, u);

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

    pa_xfree(u->scache_item);
    pa_xfree(u->sink_name);

    if (u->x11_client)
        pa_x11_client_free(u->x11_client);

    if (u->x11_wrapper)
        pa_x11_wrapper_unref(u->x11_wrapper);

    pa_xfree(u);
}
