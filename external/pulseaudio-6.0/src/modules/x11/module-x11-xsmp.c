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
#include <string.h>

#include <X11/Xlib.h>
#include <X11/SM/SMlib.h>

#include <pulse/xmalloc.h>
#include <pulse/util.h>

#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/x11wrap.h>

#include "module-x11-xsmp-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("X11 session management");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE("session_manager=<session manager string> display=<X11 display>");

static bool ice_in_use = false;

static const char* const valid_modargs[] = {
    "session_manager",
    "display",
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_client *client;
    SmcConn connection;
    pa_x11_wrapper *x11;
};

static void die_cb(SmcConn connection, SmPointer client_data) {
    struct userdata *u = client_data;
    pa_assert(u);

    pa_log_debug("Got die message from XSMP.");

    pa_x11_wrapper_kill(u->x11);

    pa_x11_wrapper_unref(u->x11);
    u->x11 = NULL;

    pa_module_unload_request(u->module, true);
}

static void save_complete_cb(SmcConn connection, SmPointer client_data) {
}

static void shutdown_cancelled_cb(SmcConn connection, SmPointer client_data) {
    SmcSaveYourselfDone(connection, True);
}

static void save_yourself_cb(SmcConn connection, SmPointer client_data, int save_type, Bool _shutdown, int interact_style, Bool fast) {
    SmcSaveYourselfDone(connection, True);
}

static void ice_io_cb(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t flags, void *userdata) {
    IceConn connection = userdata;

    if (IceProcessMessages(connection, NULL, NULL) == IceProcessMessagesIOError) {
        IceSetShutdownNegotiation(connection, False);
        IceCloseConnection(connection);
    }
}

static void new_ice_connection(IceConn connection, IcePointer client_data, Bool opening, IcePointer *watch_data) {
    pa_core *c = client_data;

    if (opening)
        *watch_data = c->mainloop->io_new(
                c->mainloop,
                IceConnectionNumber(connection),
                PA_IO_EVENT_INPUT,
                ice_io_cb,
                connection);
    else
        c->mainloop->io_free(*watch_data);
}

int pa__init(pa_module*m) {

    pa_modargs *ma = NULL;
    char t[256], *vendor, *client_id;
    SmcCallbacks callbacks;
    SmProp prop_program, prop_user;
    SmProp *prop_list[2];
    SmPropValue val_program, val_user;
    struct userdata *u;
    const char *e;
    pa_client_new_data data;

    pa_assert(m);

    if (ice_in_use) {
        pa_log("module-x11-xsmp may not be loaded twice.");
        return -1;
    }

    IceAddConnectionWatch(new_ice_connection, m->core);
    ice_in_use = true;

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->client = NULL;
    u->connection = NULL;
    u->x11 = NULL;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (!(u->x11 = pa_x11_wrapper_get(m->core, pa_modargs_get_value(ma, "display", NULL))))
        goto fail;

    e = pa_modargs_get_value(ma, "session_manager", NULL);

    if (!e && !getenv("SESSION_MANAGER")) {
        pa_log("X11 session manager not running.");
        goto fail;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.die.callback = die_cb;
    callbacks.die.client_data = u;
    callbacks.save_yourself.callback = save_yourself_cb;
    callbacks.save_yourself.client_data = m->core;
    callbacks.save_complete.callback = save_complete_cb;
    callbacks.save_complete.client_data = m->core;
    callbacks.shutdown_cancelled.callback = shutdown_cancelled_cb;
    callbacks.shutdown_cancelled.client_data = m->core;

    if (!(u->connection = SmcOpenConnection(
                  (char*) e, m->core,
                  SmProtoMajor, SmProtoMinor,
                  SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
                  &callbacks, NULL, &client_id,
                  sizeof(t), t))) {

        pa_log("Failed to open connection to session manager: %s", t);
        goto fail;
    }

    prop_program.name = (char*) SmProgram;
    prop_program.type = (char*) SmARRAY8;
    val_program.value = (char*) PACKAGE_NAME;
    val_program.length = (int) strlen(val_program.value);
    prop_program.num_vals = 1;
    prop_program.vals = &val_program;
    prop_list[0] = &prop_program;

    prop_user.name = (char*) SmUserID;
    prop_user.type = (char*) SmARRAY8;
    pa_get_user_name(t, sizeof(t));
    val_user.value = t;
    val_user.length = (int) strlen(val_user.value);
    prop_user.num_vals = 1;
    prop_user.vals = &val_user;
    prop_list[1] = &prop_user;

    SmcSetProperties(u->connection, PA_ELEMENTSOF(prop_list), prop_list);

    pa_log_info("Connected to session manager '%s' as '%s'.", vendor = SmcVendor(u->connection), client_id);

    pa_client_new_data_init(&data);
    data.module = m;
    data.driver = __FILE__;
    pa_proplist_setf(data.proplist, PA_PROP_APPLICATION_NAME, "XSMP Session on %s as %s", vendor, client_id);
    pa_proplist_sets(data.proplist, "xsmp.vendor", vendor);
    pa_proplist_sets(data.proplist, "xsmp.client.id", client_id);
    u->client = pa_client_new(u->core, &data);
    pa_client_new_data_done(&data);

    free(vendor);
    free(client_id);

    if (!u->client)
        goto fail;

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

    if ((u = m->userdata)) {

        if (u->connection)
            SmcCloseConnection(u->connection, 0, NULL);

        if (u->client)
            pa_client_free(u->client);

        if (u->x11)
            pa_x11_wrapper_unref(u->x11);

        pa_xfree(u);
    }

    if (ice_in_use) {
        IceRemoveConnectionWatch(new_ice_connection, m->core);
        ice_in_use = false;
    }
}
