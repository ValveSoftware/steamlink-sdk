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

#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>

#include "iface-device-port.h"

#define OBJECT_NAME "port"

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_description(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_priority(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_available(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

struct pa_dbusiface_device_port {
    uint32_t index;
    pa_device_port *port;
    char *path;

    pa_hook_slot *available_changed_slot;

    pa_dbus_protocol *dbus_protocol;
};

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_DESCRIPTION,
    PROPERTY_HANDLER_PRIORITY,
    PROPERTY_HANDLER_AVAILABLE,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]       = { .property_name = "Index",       .type = "u", .get_cb = handle_get_index,       .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]        = { .property_name = "Name",        .type = "s", .get_cb = handle_get_name,        .set_cb = NULL },
    [PROPERTY_HANDLER_DESCRIPTION] = { .property_name = "Description", .type = "s", .get_cb = handle_get_description, .set_cb = NULL },
    [PROPERTY_HANDLER_PRIORITY]    = { .property_name = "Priority",    .type = "u", .get_cb = handle_get_priority,    .set_cb = NULL },
    [PROPERTY_HANDLER_AVAILABLE]   = { .property_name = "Available",   .type = "u", .get_cb = handle_get_available,   .set_cb = NULL }
};

enum signal_index {
    SIGNAL_AVAILABLE_CHANGED,
    SIGNAL_MAX
};

static pa_dbus_arg_info available_changed_args[] = { { "available", "u", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_AVAILABLE_CHANGED] = { .name = "AvailableChanged", .arguments = available_changed_args, .n_arguments = 1 }
};

static pa_dbus_interface_info port_interface_info = {
    .name = PA_DBUSIFACE_DEVICE_PORT_INTERFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &p->index);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &p->port->name);
}

static void handle_get_description(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &p->port->description);
}

static void handle_get_priority(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;
    dbus_uint32_t priority = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    priority = p->port->priority;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &priority);
}

static void handle_get_available(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;
    dbus_uint32_t available = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    available = p->port->available;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &available);
}


static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_device_port *p = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t priority = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    priority = p->port->priority;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &p->index);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &p->port->name);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DESCRIPTION].property_name, DBUS_TYPE_STRING, &p->port->description);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PRIORITY].property_name, DBUS_TYPE_UINT32, &priority);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_AVAILABLE].property_name, DBUS_TYPE_UINT32, &p->port->available);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

static pa_hook_result_t available_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_device_port *p = slot_data;
    pa_device_port *port = call_data;
    DBusMessage *signal_msg;
    uint32_t available;

    pa_assert(p);
    pa_assert(port);

    if(p->port != port)
        return PA_HOOK_OK;

    available = port->available;

    pa_assert_se(signal_msg = dbus_message_new_signal(p->path,
                                                      PA_DBUSIFACE_DEVICE_PORT_INTERFACE,
                                                      signals[SIGNAL_AVAILABLE_CHANGED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_UINT32, &available, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(p->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}


pa_dbusiface_device_port *pa_dbusiface_device_port_new(
        pa_dbusiface_device *device,
        pa_core *core,
        pa_device_port *port,
        uint32_t idx) {
    pa_dbusiface_device_port *p = NULL;

    pa_assert(device);
    pa_assert(core);
    pa_assert(port);

    p = pa_xnew(pa_dbusiface_device_port, 1);
    p->index = idx;
    p->port = port;
    p->path = pa_sprintf_malloc("%s/%s%u", pa_dbusiface_device_get_path(device), OBJECT_NAME, idx);
    p->dbus_protocol = pa_dbus_protocol_get(core);
    p->available_changed_slot = pa_hook_connect(&port->core->hooks[PA_CORE_HOOK_PORT_AVAILABLE_CHANGED],
                                                PA_HOOK_NORMAL, available_changed_cb, p);

    pa_assert_se(pa_dbus_protocol_add_interface(p->dbus_protocol, p->path, &port_interface_info, p) >= 0);

    return p;
}

void pa_dbusiface_device_port_free(pa_dbusiface_device_port *p) {
    pa_assert(p);

    pa_assert_se(pa_dbus_protocol_remove_interface(p->dbus_protocol, p->path, port_interface_info.name) >= 0);

    pa_hook_slot_free(p->available_changed_slot);
    pa_dbus_protocol_unref(p->dbus_protocol);

    pa_xfree(p->path);
    pa_xfree(p);
}

const char *pa_dbusiface_device_port_get_path(pa_dbusiface_device_port *p) {
    pa_assert(p);

    return p->path;
}

const char *pa_dbusiface_device_port_get_name(pa_dbusiface_device_port *p) {
    pa_assert(p);

    return p->port->name;
}
