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

#include "iface-card-profile.h"

#define OBJECT_NAME "profile"

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_description(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sinks(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sources(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_priority(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_available(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

struct pa_dbusiface_card_profile {
    uint32_t index;
    pa_card_profile *profile;
    char *path;
    pa_dbus_protocol *dbus_protocol;
};

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_DESCRIPTION,
    PROPERTY_HANDLER_SINKS,
    PROPERTY_HANDLER_SOURCES,
    PROPERTY_HANDLER_PRIORITY,
    PROPERTY_HANDLER_AVAILABLE,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]       = { .property_name = "Index",       .type = "u", .get_cb = handle_get_index,       .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]        = { .property_name = "Name",        .type = "s", .get_cb = handle_get_name,        .set_cb = NULL },
    [PROPERTY_HANDLER_DESCRIPTION] = { .property_name = "Description", .type = "s", .get_cb = handle_get_description, .set_cb = NULL },
    [PROPERTY_HANDLER_SINKS]       = { .property_name = "Sinks",       .type = "u", .get_cb = handle_get_sinks,       .set_cb = NULL },
    [PROPERTY_HANDLER_SOURCES]     = { .property_name = "Sources",     .type = "u", .get_cb = handle_get_sources,     .set_cb = NULL },
    [PROPERTY_HANDLER_PRIORITY]    = { .property_name = "Priority",    .type = "u", .get_cb = handle_get_priority,    .set_cb = NULL },
    [PROPERTY_HANDLER_AVAILABLE]   = { .property_name = "Available",   .type = "b", .get_cb = handle_get_available,   .set_cb = NULL },
};

static pa_dbus_interface_info profile_interface_info = {
    .name = PA_DBUSIFACE_CARD_PROFILE_INTERFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = NULL,
    .n_signals = 0
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &p->index);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &p->profile->name);
}

static void handle_get_description(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &p->profile->description);
}

static void handle_get_sinks(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;
    dbus_uint32_t sinks = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    sinks = p->profile->n_sinks;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sinks);
}

static void handle_get_sources(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;
    dbus_uint32_t sources = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    sources = p->profile->n_sources;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sources);
}

static void handle_get_priority(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;
    dbus_uint32_t priority = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    priority = p->profile->priority;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &priority);
}

static void handle_get_available(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;
    dbus_bool_t available;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    available = p->profile->available != PA_AVAILABLE_NO;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &available);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_card_profile *p = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t sinks = 0;
    dbus_uint32_t sources = 0;
    dbus_uint32_t priority = 0;
    dbus_bool_t available;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(p);

    sinks = p->profile->n_sinks;
    sources = p->profile->n_sources;
    priority = p->profile->priority;
    available = p->profile->available != PA_AVAILABLE_NO;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &p->index);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &p->profile->name);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DESCRIPTION].property_name, DBUS_TYPE_STRING, &p->profile->description);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SINKS].property_name, DBUS_TYPE_UINT32, &sinks);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SOURCES].property_name, DBUS_TYPE_UINT32, &sources);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PRIORITY].property_name, DBUS_TYPE_UINT32, &priority);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_AVAILABLE].property_name, DBUS_TYPE_BOOLEAN, &available);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

pa_dbusiface_card_profile *pa_dbusiface_card_profile_new(
        pa_dbusiface_card *card,
        pa_core *core,
        pa_card_profile *profile,
        uint32_t idx) {
    pa_dbusiface_card_profile *p = NULL;

    pa_assert(card);
    pa_assert(core);
    pa_assert(profile);

    p = pa_xnew(pa_dbusiface_card_profile, 1);
    p->index = idx;
    p->profile = profile;
    p->path = pa_sprintf_malloc("%s/%s%u", pa_dbusiface_card_get_path(card), OBJECT_NAME, idx);
    p->dbus_protocol = pa_dbus_protocol_get(core);

    pa_assert_se(pa_dbus_protocol_add_interface(p->dbus_protocol, p->path, &profile_interface_info, p) >= 0);

    return p;
}

void pa_dbusiface_card_profile_free(pa_dbusiface_card_profile *p) {
    pa_assert(p);

    pa_assert_se(pa_dbus_protocol_remove_interface(p->dbus_protocol, p->path, profile_interface_info.name) >= 0);

    pa_dbus_protocol_unref(p->dbus_protocol);

    pa_xfree(p->path);
    pa_xfree(p);
}

const char *pa_dbusiface_card_profile_get_path(pa_dbusiface_card_profile *p) {
    pa_assert(p);

    return p->path;
}

const char *pa_dbusiface_card_profile_get_name(pa_dbusiface_card_profile *p) {
    pa_assert(p);

    return p->profile->name;
}

pa_card_profile *pa_dbusiface_card_profile_get_profile(pa_dbusiface_card_profile *p) {
    pa_assert(p);

    return p->profile;
}
