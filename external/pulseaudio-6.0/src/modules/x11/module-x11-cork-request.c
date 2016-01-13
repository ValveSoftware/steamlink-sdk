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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>

#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/x11wrap.h>
#include <pulsecore/core-util.h>

#include "module-x11-cork-request-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Synthesize X11 media key events when cork/uncork is requested");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE("display=<X11 display>");

static const char* const valid_modargs[] = {
    "display",
    NULL
};

struct userdata {
    pa_module *module;

    pa_x11_wrapper *x11_wrapper;
    pa_x11_client *x11_client;

    pa_hook_slot *hook_slot;
};

static void x11_kill_cb(pa_x11_wrapper *w, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(w);
    pa_assert(u);
    pa_assert(u->x11_wrapper == w);

    if (u->x11_client) {
        pa_x11_client_free(u->x11_client);
        u->x11_client = NULL;
    }

    if (u->x11_wrapper) {
        pa_x11_wrapper_unref(u->x11_wrapper);
        u->x11_wrapper = NULL;
    }

    pa_module_unload_request(u->module, true);
}

static pa_hook_result_t sink_input_send_event_hook_cb(
        pa_core *c,
        pa_sink_input_send_event_hook_data *data,
        struct userdata *u) {

    KeySym sym;
    KeyCode code;
    Display *display;

    pa_assert(c);
    pa_assert(data);
    pa_assert(u);

    if (pa_streq(data->event, PA_STREAM_EVENT_REQUEST_CORK))
        sym = XF86XK_AudioPause;
    else if (pa_streq(data->event, PA_STREAM_EVENT_REQUEST_UNCORK))
        sym = XF86XK_AudioPlay;
    else
        return PA_HOOK_OK;

    pa_log_debug("Triggering X11 keysym: %s", XKeysymToString(sym));

    display = pa_x11_wrapper_get_display(u->x11_wrapper);
    code = XKeysymToKeycode(display, sym);

    XTestFakeKeyEvent(display, code, True, CurrentTime);
    XSync(display, False);

    XTestFakeKeyEvent(display, code, False, CurrentTime);
    XSync(display, False);

    return PA_HOOK_OK;
}

int pa__init(pa_module *m) {
    struct userdata *u;
    pa_modargs *ma;
    int xtest_event_base, xtest_error_base;
    int major_version, minor_version;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;

    if (!(u->x11_wrapper = pa_x11_wrapper_get(m->core, pa_modargs_get_value(ma, "display", NULL))))
        goto fail;

    if (!XTestQueryExtension(
                pa_x11_wrapper_get_display(u->x11_wrapper),
                &xtest_event_base, &xtest_error_base,
                &major_version, &minor_version)) {

        pa_log("XTest extension not supported.");
        goto fail;
    }

    pa_log_debug("XTest %i.%i supported.", major_version, minor_version);

    u->x11_client = pa_x11_client_new(u->x11_wrapper, NULL, x11_kill_cb, u);

    u->hook_slot = pa_hook_connect(
            &m->core->hooks[PA_CORE_HOOK_SINK_INPUT_SEND_EVENT],
            PA_HOOK_NORMAL,
            (pa_hook_cb_t) sink_input_send_event_hook_cb, u);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata*u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->x11_client)
        pa_x11_client_free(u->x11_client);

    if (u->x11_wrapper)
        pa_x11_wrapper_unref(u->x11_wrapper);

    if (u->hook_slot)
        pa_hook_slot_free(u->hook_slot);

    pa_xfree(u);
}
