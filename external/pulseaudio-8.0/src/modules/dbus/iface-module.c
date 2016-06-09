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

#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-module.h"

#define OBJECT_NAME "module"

struct pa_dbusiface_module {
    pa_module *module;
    char *path;
    pa_proplist *proplist;

    pa_dbus_protocol *dbus_protocol;
    pa_hook_slot *module_proplist_changed_slot;
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_arguments(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_usage_counter(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_unload(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_ARGUMENTS,
    PROPERTY_HANDLER_USAGE_COUNTER,
    PROPERTY_HANDLER_PROPERTY_LIST,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]         = { .property_name = "Index",        .type = "u",      .get_cb = handle_get_index,         .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]          = { .property_name = "Name",         .type = "s",      .get_cb = handle_get_name,          .set_cb = NULL },
    [PROPERTY_HANDLER_ARGUMENTS]     = { .property_name = "Arguments",    .type = "a{ss}",  .get_cb = handle_get_arguments,     .set_cb = NULL },
    [PROPERTY_HANDLER_USAGE_COUNTER] = { .property_name = "UsageCounter", .type = "u",      .get_cb = handle_get_usage_counter, .set_cb = NULL },
    [PROPERTY_HANDLER_PROPERTY_LIST] = { .property_name = "PropertyList", .type = "a{say}", .get_cb = handle_get_property_list, .set_cb = NULL }
};

enum method_handler_index {
    METHOD_HANDLER_UNLOAD,
    METHOD_HANDLER_MAX
};

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_UNLOAD] = {
        .method_name = "Unload",
        .arguments = NULL,
        .n_arguments = 0,
        .receive_cb = handle_unload }
};

enum signal_index {
    SIGNAL_PROPERTY_LIST_UPDATED,
    SIGNAL_MAX
};

static pa_dbus_arg_info property_list_updated_args[] =  { { "property_list", "a{say}", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_PROPERTY_LIST_UPDATED] = { .name = "PropertyListUpdated", .arguments = property_list_updated_args, .n_arguments = 1 }
};

static pa_dbus_interface_info module_interface_info = {
    .name = PA_DBUSIFACE_MODULE_INTERFACE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;
    dbus_uint32_t idx = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    idx = m->module->index;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &idx);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &m->module->name);
}

static void append_modargs_variant(DBusMessageIter *iter, pa_dbusiface_module *m) {
    pa_modargs *ma = NULL;
    DBusMessageIter variant_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    void *state = NULL;
    const char *key = NULL;
    const char *value = NULL;

    pa_assert(iter);
    pa_assert(m);

    pa_assert_se(ma = pa_modargs_new(m->module->argument, NULL));

    pa_assert_se(dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "a{ss}", &variant_iter));
    pa_assert_se(dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "{ss}", &dict_iter));

    for (state = NULL, key = pa_modargs_iterate(ma, &state); key; key = pa_modargs_iterate(ma, &state)) {
        pa_assert_se(value = pa_modargs_get_value(ma, key, NULL));

        pa_assert_se(dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter));

        pa_assert_se(dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &key));
        pa_assert_se(dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &value));

        pa_assert_se(dbus_message_iter_close_container(&dict_iter, &dict_entry_iter));
    }

    pa_assert_se(dbus_message_iter_close_container(&variant_iter, &dict_iter));
    pa_assert_se(dbus_message_iter_close_container(iter, &variant_iter));

    pa_modargs_free(ma);
}

static void handle_get_arguments(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    pa_assert_se(reply = dbus_message_new_method_return(msg));
    dbus_message_iter_init_append(reply, &msg_iter);
    append_modargs_variant(&msg_iter, m);
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

static void handle_get_usage_counter(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;
    int real_counter_value = -1;
    dbus_uint32_t usage_counter = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    if (!m->module->get_n_used || (real_counter_value = m->module->get_n_used(m->module)) < 0) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Module %u (%s) doesn't have a usage counter.", m->module->index, m->module->name);
        return;
    }

    usage_counter = real_counter_value;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &usage_counter);
}

static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    pa_dbus_send_proplist_variant_reply(conn, msg, m->proplist);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    dbus_uint32_t idx = 0;
    int real_counter_value = -1;
    dbus_uint32_t usage_counter = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    idx = m->module->index;
    if (m->module->get_n_used && (real_counter_value = m->module->get_n_used(m->module)) >= 0)
        usage_counter = real_counter_value;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &idx);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &m->module->name);

    pa_assert_se(dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter));
    pa_assert_se(dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &property_handlers[PROPERTY_HANDLER_ARGUMENTS].property_name));
    append_modargs_variant(&dict_entry_iter, m);
    pa_assert_se(dbus_message_iter_close_container(&dict_iter, &dict_entry_iter));

    if (real_counter_value >= 0)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ARGUMENTS].property_name, DBUS_TYPE_UINT32, &usage_counter);

    pa_dbus_append_proplist_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PROPERTY_LIST].property_name, m->proplist);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);
}

static void handle_unload(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_module *m = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    if (m->module->core->disallow_module_loading) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_ACCESS_DENIED, "The server is configured to disallow module unloading.");
        return;
    }

    pa_module_unload_request(m->module, false);

    pa_dbus_send_empty_reply(conn, msg);
}

static pa_hook_result_t module_proplist_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_module *module_iface = slot_data;
    pa_module * module = call_data;
    DBusMessage *signal_msg;

    pa_assert(module_iface);
    pa_assert(module);

    if (module_iface->module != module)
        return PA_HOOK_OK;

    if (!pa_proplist_equal(module_iface->proplist, module->proplist)) {
        DBusMessageIter msg_iter;

        pa_proplist_update(module_iface->proplist, PA_UPDATE_SET, module->proplist);

        pa_assert_se(signal_msg = dbus_message_new_signal(module_iface->path,
                                                          PA_DBUSIFACE_MODULE_INTERFACE,
                                                          signals[SIGNAL_PROPERTY_LIST_UPDATED].name));
        dbus_message_iter_init_append(signal_msg, &msg_iter);
        pa_dbus_append_proplist(&msg_iter, module_iface->proplist);

        pa_dbus_protocol_send_signal(module_iface->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

pa_dbusiface_module *pa_dbusiface_module_new(pa_module *module) {
    pa_dbusiface_module *m;

    pa_assert(module);

    m = pa_xnew0(pa_dbusiface_module, 1);
    m->module = module;
    m->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, OBJECT_NAME, module->index);
    m->proplist = pa_proplist_copy(module->proplist);
    m->dbus_protocol = pa_dbus_protocol_get(module->core);
    m->module_proplist_changed_slot = pa_hook_connect(&module->core->hooks[PA_CORE_HOOK_MODULE_PROPLIST_CHANGED],
                                                      PA_HOOK_NORMAL, module_proplist_changed_cb, m);

    pa_assert_se(pa_dbus_protocol_add_interface(m->dbus_protocol, m->path, &module_interface_info, m) >= 0);

    return m;
}

void pa_dbusiface_module_free(pa_dbusiface_module *m) {
    pa_assert(m);

    pa_assert_se(pa_dbus_protocol_remove_interface(m->dbus_protocol, m->path, module_interface_info.name) >= 0);

    pa_proplist_free(m->proplist);
    pa_dbus_protocol_unref(m->dbus_protocol);
    pa_hook_slot_free(m->module_proplist_changed_slot);

    pa_xfree(m->path);
    pa_xfree(m);
}

const char *pa_dbusiface_module_get_path(pa_dbusiface_module *m) {
    pa_assert(m);

    return m->path;
}
