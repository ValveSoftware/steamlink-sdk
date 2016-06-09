/***
  This file is part of PulseAudio.

  Copyright 2009 Tanu Kaskinen

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

#include <dbus/dbus.h>

#include <pulse/client-conf.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dbus-shared.h>
#include <pulsecore/macro.h>
#include <pulsecore/protocol-dbus.h>

#include "server-lookup.h"

#define OBJECT_PATH "/org/pulseaudio/server_lookup1"
#define INTERFACE "org.PulseAudio.ServerLookup1"

struct pa_dbusobj_server_lookup {
    pa_core *core;
    pa_dbus_connection *conn;
    bool path_registered;
};

static const char introspection[] =
    DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE
    "<node>"
    " <!-- If you are looking for documentation make sure to check out\n"
    "      http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/ -->\n"
    " <interface name=\"" INTERFACE "\">\n"
    "  <property name=\"Address\" type=\"s\" access=\"read\"/>\n"
    " </interface>\n"
    " <interface name=\"" DBUS_INTERFACE_INTROSPECTABLE "\">\n"
    "  <method name=\"Introspect\">\n"
    "   <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
    "  </method>\n"
    " </interface>\n"
    " <interface name=\"" DBUS_INTERFACE_PROPERTIES "\">\n"
    "  <method name=\"Get\">\n"
    "   <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
    "   <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
    "   <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
    "  </method>\n"
    "  <method name=\"Set\">\n"
    "   <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
    "   <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
    "   <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
    "  </method>\n"
    "  <method name=\"GetAll\">\n"
    "   <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
    "   <arg name=\"props\" type=\"a{sv}\" direction=\"out\"/>\n"
    "  </method>\n"
    " </interface>\n"
    "</node>\n";

static void unregister_cb(DBusConnection *conn, void *user_data) {
    pa_dbusobj_server_lookup *sl = user_data;

    pa_assert(sl);
    pa_assert(sl->path_registered);

    sl->path_registered = false;
}

static DBusHandlerResult handle_introspect(DBusConnection *conn, DBusMessage *msg, pa_dbusobj_server_lookup *sl) {
    DBusHandlerResult r = DBUS_HANDLER_RESULT_HANDLED;
    const char *i = introspection;
    DBusMessage *reply = NULL;

    pa_assert(conn);
    pa_assert(msg);

    if (!(reply = dbus_message_new_method_return(msg))) {
        r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        goto finish;
    }
    if (!dbus_message_append_args(reply, DBUS_TYPE_STRING, &i, DBUS_TYPE_INVALID)) {
        r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        goto finish;
    }
    if (!dbus_connection_send(conn, reply, NULL)) {
        r = DBUS_HANDLER_RESULT_NEED_MEMORY;
        goto finish;
    }

finish:
    if (reply)
        dbus_message_unref(reply);

    return r;
}

enum get_address_result_t {
    SUCCESS,
    SERVER_FROM_TYPE_FAILED
};

/* Caller frees the returned address. */
static enum get_address_result_t get_address(pa_server_type_t server_type, char **address) {
    enum get_address_result_t r = SUCCESS;
    pa_client_conf *conf = pa_client_conf_new();

    *address = NULL;

    pa_client_conf_load(conf, false, false);

    if (conf->default_dbus_server)
        *address = pa_xstrdup(conf->default_dbus_server);
    else if (!(*address = pa_get_dbus_address_from_server_type(server_type))) {
        r = SERVER_FROM_TYPE_FAILED;
        goto finish;
    }

finish:
    pa_client_conf_free(conf);
    return r;
}

static DBusHandlerResult handle_get_address(DBusConnection *conn, DBusMessage *msg, pa_dbusobj_server_lookup *sl) {
    DBusHandlerResult r = DBUS_HANDLER_RESULT_HANDLED;
    DBusMessage *reply = NULL;
    char *address = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter variant_iter;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(sl);

    switch (get_address(sl->core->server_type, &address)) {
        case SUCCESS:
            if (!(reply = dbus_message_new_method_return(msg))) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            dbus_message_iter_init_append(reply, &msg_iter);
            if (!dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_VARIANT, "s", &variant_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &address)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_close_container(&msg_iter, &variant_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_connection_send(conn, reply, NULL)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            r = DBUS_HANDLER_RESULT_HANDLED;
            goto finish;

        case SERVER_FROM_TYPE_FAILED:
            if (!(reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "PulseAudio internal error: get_dbus_server_from_type() failed."))) {
                r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                goto finish;
            }
            if (!dbus_connection_send(conn, reply, NULL)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            r = DBUS_HANDLER_RESULT_HANDLED;
            goto finish;

        default:
            pa_assert_not_reached();
    }

finish:
    pa_xfree(address);
    if (reply)
        dbus_message_unref(reply);

    return r;
}

static DBusHandlerResult handle_get(DBusConnection *conn, DBusMessage *msg, pa_dbusobj_server_lookup *sl) {
    DBusHandlerResult r = DBUS_HANDLER_RESULT_HANDLED;
    const char* interface;
    const char* property;
    DBusMessage *reply = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(sl);

    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) {
        if (!(reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Invalid arguments"))) {
            r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            goto finish;
        }
        if (!dbus_connection_send(conn, reply, NULL)) {
            r = DBUS_HANDLER_RESULT_NEED_MEMORY;
            goto finish;
        }
        r = DBUS_HANDLER_RESULT_HANDLED;
        goto finish;
    }

    if (*interface && !pa_streq(interface, INTERFACE)) {
        r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        goto finish;
    }

    if (!pa_streq(property, "Address")) {
        if (!(reply = dbus_message_new_error_printf(msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s: No such property", property))) {
            r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            goto finish;
        }
        if (!dbus_connection_send(conn, reply, NULL)) {
            r = DBUS_HANDLER_RESULT_NEED_MEMORY;
            goto finish;
        }
        r = DBUS_HANDLER_RESULT_HANDLED;
        goto finish;
    }

    r = handle_get_address(conn, msg, sl);

finish:
    if (reply)
        dbus_message_unref(reply);

    return r;
}

static DBusHandlerResult handle_set(DBusConnection *conn, DBusMessage *msg, pa_dbusobj_server_lookup *sl) {
    DBusHandlerResult r = DBUS_HANDLER_RESULT_HANDLED;
    const char* interface;
    const char* property;
    DBusMessage *reply = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(sl);

    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) {
        if (!(reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Invalid arguments"))) {
            r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            goto finish;
        }
        if (!dbus_connection_send(conn, reply, NULL)) {
            r = DBUS_HANDLER_RESULT_NEED_MEMORY;
            goto finish;
        }
        r = DBUS_HANDLER_RESULT_HANDLED;
        goto finish;
    }

    if (*interface && !pa_streq(interface, INTERFACE)) {
        r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        goto finish;
    }

    if (!pa_streq(property, "Address")) {
        if (!(reply = dbus_message_new_error_printf(msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s: No such property", property))) {
            r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            goto finish;
        }
        if (!dbus_connection_send(conn, reply, NULL)) {
            r = DBUS_HANDLER_RESULT_NEED_MEMORY;
            goto finish;
        }
        r = DBUS_HANDLER_RESULT_HANDLED;
        goto finish;
    }

    if (!(reply = dbus_message_new_error_printf(msg, DBUS_ERROR_ACCESS_DENIED, "%s: Property not settable", property))) {
        r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        goto finish;
    }
    if (!dbus_connection_send(conn, reply, NULL)) {
        r = DBUS_HANDLER_RESULT_NEED_MEMORY;
        goto finish;
    }
    r = DBUS_HANDLER_RESULT_HANDLED;

finish:
    if (reply)
        dbus_message_unref(reply);

    return r;
}

static DBusHandlerResult handle_get_all(DBusConnection *conn, DBusMessage *msg, pa_dbusobj_server_lookup *sl) {
    DBusHandlerResult r = DBUS_HANDLER_RESULT_HANDLED;
    DBusMessage *reply = NULL;
    const char *property = "Address";
    char *interface = NULL;
    char *address = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    DBusMessageIter variant_iter;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(sl);

    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface, DBUS_TYPE_INVALID)) {
        if (!(reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Invalid arguments"))) {
            r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
            goto finish;
        }
        if (!dbus_connection_send(conn, reply, NULL)) {
            r = DBUS_HANDLER_RESULT_NEED_MEMORY;
            goto finish;
        }
        r = DBUS_HANDLER_RESULT_HANDLED;
        goto finish;
    }

    switch (get_address(sl->core->server_type, &address)) {
        case SUCCESS:
            if (!(reply = dbus_message_new_method_return(msg))) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            dbus_message_iter_init_append(reply, &msg_iter);
            if (!dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &property)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_open_container(&dict_entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &address)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_close_container(&dict_entry_iter, &variant_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_close_container(&dict_iter, &dict_entry_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_message_iter_close_container(&msg_iter, &dict_iter)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            if (!dbus_connection_send(conn, reply, NULL)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            r = DBUS_HANDLER_RESULT_HANDLED;
            goto finish;

        case SERVER_FROM_TYPE_FAILED:
            if (!(reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "PulseAudio internal error: get_dbus_server_from_type() failed."))) {
                r = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                goto finish;
            }
            if (!dbus_connection_send(conn, reply, NULL)) {
                r = DBUS_HANDLER_RESULT_NEED_MEMORY;
                goto finish;
            }
            r = DBUS_HANDLER_RESULT_HANDLED;
            goto finish;

        default:
            pa_assert_not_reached();
    }

finish:
    pa_xfree(address);
    if (reply)
        dbus_message_unref(reply);

    return r;
}

static DBusHandlerResult message_cb(DBusConnection *conn, DBusMessage *msg, void *user_data) {
    pa_dbusobj_server_lookup *sl = user_data;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(sl);

    /* pa_log("Got message! type = %s   path = %s   iface = %s   member = %s   dest = %s", dbus_message_type_to_string(dbus_message_get_type(msg)), dbus_message_get_path(msg), dbus_message_get_interface(msg), dbus_message_get_member(msg), dbus_message_get_destination(msg)); */

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_method_call(msg, DBUS_INTERFACE_INTROSPECTABLE, "Introspect") ||
        (!dbus_message_get_interface(msg) && dbus_message_has_member(msg, "Introspect")))
        return handle_introspect(conn, msg, sl);

    if (dbus_message_is_method_call(msg, DBUS_INTERFACE_PROPERTIES, "Get") ||
        (!dbus_message_get_interface(msg) && dbus_message_has_member(msg, "Get")))
        return handle_get(conn, msg, sl);

    if (dbus_message_is_method_call(msg, DBUS_INTERFACE_PROPERTIES, "Set") ||
        (!dbus_message_get_interface(msg) && dbus_message_has_member(msg, "Set")))
        return handle_set(conn, msg, sl);

    if (dbus_message_is_method_call(msg, DBUS_INTERFACE_PROPERTIES, "GetAll") ||
        (!dbus_message_get_interface(msg) && dbus_message_has_member(msg, "GetAll")))
        return handle_get_all(conn, msg, sl);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable vtable = {
    .unregister_function = unregister_cb,
    .message_function = message_cb,
    .dbus_internal_pad1 = NULL,
    .dbus_internal_pad2 = NULL,
    .dbus_internal_pad3 = NULL,
    .dbus_internal_pad4 = NULL
};

pa_dbusobj_server_lookup *pa_dbusobj_server_lookup_new(pa_core *c) {
    pa_dbusobj_server_lookup *sl;
    DBusError error;

    dbus_error_init(&error);

    sl = pa_xnew(pa_dbusobj_server_lookup, 1);
    sl->core = c;
    sl->path_registered = false;

    if (!(sl->conn = pa_dbus_bus_get(c, DBUS_BUS_SESSION, &error)) || dbus_error_is_set(&error)) {
        pa_log_warn("Unable to contact D-Bus: %s: %s", error.name, error.message);
        goto fail;
    }

    if (!dbus_connection_register_object_path(pa_dbus_connection_get(sl->conn), OBJECT_PATH, &vtable, sl)) {
        pa_log("dbus_connection_register_object_path() failed for " OBJECT_PATH ".");
        goto fail;
    }

    sl->path_registered = true;

    return sl;

fail:
    dbus_error_free(&error);

    pa_dbusobj_server_lookup_free(sl);

    return NULL;
}

void pa_dbusobj_server_lookup_free(pa_dbusobj_server_lookup *sl) {
    pa_assert(sl);

    if (sl->path_registered) {
        pa_assert(sl->conn);
        if (!dbus_connection_unregister_object_path(pa_dbus_connection_get(sl->conn), OBJECT_PATH))
            pa_log_debug("dbus_connection_unregister_object_path() failed for " OBJECT_PATH ".");
    }

    if (sl->conn)
        pa_dbus_connection_unref(sl->conn);

    pa_xfree(sl);
}
