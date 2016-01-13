/***
    This file is part of PulseAudio.

    Copyright 2008 Lennart Poettering

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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/log.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/idxset.h>
#include <pulsecore/modargs.h>
#include <pulsecore/dbus-shared.h>

#include "module-console-kit-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Create a client for each ConsoleKit session of this user");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    NULL
};

struct session {
    char *id;
    pa_client *client;
};

struct userdata {
    pa_module *module;
    pa_core *core;
    pa_dbus_connection *connection;
    pa_hashmap *sessions;
    bool filter_added;
};

static void add_session(struct userdata *u, const char *id) {
    DBusError error;
    DBusMessage *m = NULL, *reply = NULL;
    uint32_t uid;
    struct session *session;
    pa_client_new_data data;

    dbus_error_init(&error);

    if (pa_hashmap_get(u->sessions, id)) {
        pa_log_warn("Duplicate session %s, ignoring.", id);
        return;
    }

    if (!(m = dbus_message_new_method_call("org.freedesktop.ConsoleKit", id, "org.freedesktop.ConsoleKit.Session", "GetUnixUser"))) {
        pa_log("Failed to allocate GetUnixUser() method call.");
        goto fail;
    }

    if (!(reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->connection), m, -1, &error))) {
        pa_log("GetUnixUser() call failed: %s: %s", error.name, error.message);
        goto fail;
    }

    /* CK 0.3 this changed from int32 to uint32 */
    if (!dbus_message_get_args(reply, &error, DBUS_TYPE_UINT32, &uid, DBUS_TYPE_INVALID)) {
        dbus_error_free(&error);

        if (!dbus_message_get_args(reply, &error, DBUS_TYPE_INT32, &uid, DBUS_TYPE_INVALID)) {
            pa_log("Failed to parse GetUnixUser() result: %s: %s", error.name, error.message);
            goto fail;
        }
    }

    /* We only care about our own sessions */
    if ((uid_t) uid != getuid())
        goto fail;

    session = pa_xnew(struct session, 1);
    session->id = pa_xstrdup(id);

    pa_client_new_data_init(&data);
    data.module = u->module;
    data.driver = __FILE__;
    pa_proplist_setf(data.proplist, PA_PROP_APPLICATION_NAME, "ConsoleKit Session %s", id);
    pa_proplist_sets(data.proplist, "console-kit.session", id);
    session->client = pa_client_new(u->core, &data);
    pa_client_new_data_done(&data);

    if (!session->client) {
        pa_xfree(session->id);
        pa_xfree(session);
        goto fail;
    }

    pa_hashmap_put(u->sessions, session->id, session);

    pa_log_debug("Added new session %s", id);

fail:

    if (m)
        dbus_message_unref(m);

    if (reply)
        dbus_message_unref(reply);

    dbus_error_free(&error);
}

static void free_session(struct session *session) {
    pa_assert(session);

    pa_log_debug("Removing session %s", session->id);

    pa_client_free(session->client);
    pa_xfree(session->id);
    pa_xfree(session);
}

static void remove_session(struct userdata *u, const char *id) {
    pa_assert(u);
    pa_assert(id);

    pa_hashmap_remove_and_free(u->sessions, id);
}

static DBusHandlerResult filter_cb(DBusConnection *bus, DBusMessage *message, void *userdata) {
    struct userdata *u = userdata;
    DBusError error;
    const char *path;

    pa_assert(bus);
    pa_assert(message);
    pa_assert(u);

    dbus_error_init(&error);

    if (dbus_message_is_signal(message, "org.freedesktop.ConsoleKit.Seat", "SessionAdded")) {

        /* CK API changed to match spec in 0.3 */
        if (!dbus_message_get_args(message, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID)) {
            dbus_error_free(&error);

            if (!dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
                pa_log_error("Failed to parse SessionAdded message: %s: %s", error.name, error.message);
                goto finish;
            }
        }

        add_session(u, path);

    } else if (dbus_message_is_signal(message, "org.freedesktop.ConsoleKit.Seat", "SessionRemoved")) {

        /* CK API changed to match spec in 0.3 */
        if (!dbus_message_get_args(message, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID)) {
            dbus_error_free(&error);

            if (!dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &path, DBUS_TYPE_INVALID)) {
                pa_log_error("Failed to parse SessionRemoved message: %s: %s", error.name, error.message);
                goto finish;
            }
        }

        remove_session(u, path);
    }

finish:
    dbus_error_free(&error);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int get_session_list(struct userdata *u) {
    DBusError error;
    DBusMessage *m = NULL, *reply = NULL;
    uint32_t uid;
    DBusMessageIter iter, sub;
    int ret = -1;

    pa_assert(u);

    dbus_error_init(&error);

    if (!(m = dbus_message_new_method_call("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager", "org.freedesktop.ConsoleKit.Manager", "GetSessionsForUnixUser"))) {
        pa_log("Failed to allocate GetSessionsForUnixUser() method call.");
        goto fail;
    }

    uid = (uint32_t) getuid();
    if (!(dbus_message_append_args(m, DBUS_TYPE_UINT32, &uid, DBUS_TYPE_INVALID))) {
        pa_log("Failed to append arguments to GetSessionsForUnixUser() method call.");
        goto fail;
    }

    if (!(reply = dbus_connection_send_with_reply_and_block(pa_dbus_connection_get(u->connection), m, -1, &error))) {
        pa_log("GetSessionsForUnixUser() call failed: %s: %s", error.name, error.message);
        goto fail;
    }

    dbus_message_iter_init(reply, &iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY ||
        dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
        pa_log("Failed to parse GetSessionsForUnixUser() result.");
        goto fail;
    }

    dbus_message_iter_recurse(&iter, &sub);

    for (;;) {
        int at;
        const char *id;

        if ((at = dbus_message_iter_get_arg_type(&sub)) == DBUS_TYPE_INVALID)
            break;

        pa_assert(at == DBUS_TYPE_OBJECT_PATH);
        dbus_message_iter_get_basic(&sub, &id);

        add_session(u, id);

        dbus_message_iter_next(&sub);
    }

    ret = 0;

fail:

    if (m)
        dbus_message_unref(m);

    if (reply)
        dbus_message_unref(reply);

    dbus_error_free(&error);

    return ret;
}

int pa__init(pa_module*m) {
    DBusError error;
    pa_dbus_connection *connection;
    struct userdata *u = NULL;
    pa_modargs *ma;

    pa_assert(m);

    dbus_error_init(&error);

    /* If systemd's logind service is running, we shouldn't watch ConsoleKit
     * but login */
    if (access("/run/systemd/seats/", F_OK) >= 0)
        return 0;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (!(connection = pa_dbus_bus_get(m->core, DBUS_BUS_SYSTEM, &error)) || dbus_error_is_set(&error)) {

        if (connection)
            pa_dbus_connection_unref(connection);

        pa_log_error("Unable to contact D-Bus system bus: %s: %s", error.name, error.message);
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->connection = connection;
    u->sessions = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) free_session);

    if (!dbus_connection_add_filter(pa_dbus_connection_get(connection), filter_cb, u, NULL)) {
        pa_log_error("Failed to add filter function");
        goto fail;
    }

    u->filter_added = true;

    if (pa_dbus_add_matches(
                pa_dbus_connection_get(connection), &error,
                "type='signal',sender='org.freedesktop.ConsoleKit',interface='org.freedesktop.ConsoleKit.Seat',member='SessionAdded'",
                "type='signal',sender='org.freedesktop.ConsoleKit',interface='org.freedesktop.ConsoleKit.Seat',member='SessionRemoved'", NULL) < 0) {
        pa_log_error("Unable to subscribe to ConsoleKit signals: %s: %s", error.name, error.message);
        goto fail;
    }

    if (get_session_list(u) < 0)
        goto fail;

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    dbus_error_free(&error);
    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sessions)
        pa_hashmap_free(u->sessions);

    if (u->connection) {
        pa_dbus_remove_matches(
                pa_dbus_connection_get(u->connection),
                "type='signal',sender='org.freedesktop.ConsoleKit',interface='org.freedesktop.ConsoleKit.Seat',member='SessionAdded'",
                "type='signal',sender='org.freedesktop.ConsoleKit',interface='org.freedesktop.ConsoleKit.Seat',member='SessionRemoved'", NULL);

        if (u->filter_added)
            dbus_connection_remove_filter(pa_dbus_connection_get(u->connection), filter_cb, u);

        pa_dbus_connection_unref(u->connection);
    }

    pa_xfree(u);
}
