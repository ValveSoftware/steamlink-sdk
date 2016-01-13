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

#include <pulsecore/core-scache.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-memstats.h"

#define OBJECT_NAME "memstats"

static void handle_get_current_memblocks(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_current_memblocks_size(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_accumulated_memblocks(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_accumulated_memblocks_size(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_cache_size(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

struct pa_dbusiface_memstats {
    pa_core *core;
    char *path;
    pa_dbus_protocol *dbus_protocol;
};

enum property_handler_index {
    PROPERTY_HANDLER_CURRENT_MEMBLOCKS,
    PROPERTY_HANDLER_CURRENT_MEMBLOCKS_SIZE,
    PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS,
    PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS_SIZE,
    PROPERTY_HANDLER_SAMPLE_CACHE_SIZE,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_CURRENT_MEMBLOCKS]          = { .property_name = "CurrentMemblocks",         .type = "u", .get_cb = handle_get_current_memblocks,          .set_cb = NULL },
    [PROPERTY_HANDLER_CURRENT_MEMBLOCKS_SIZE]     = { .property_name = "CurrentMemblocksSize",     .type = "u", .get_cb = handle_get_current_memblocks_size,     .set_cb = NULL },
    [PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS]      = { .property_name = "AccumulatedMemblocks",     .type = "u", .get_cb = handle_get_accumulated_memblocks,      .set_cb = NULL },
    [PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS_SIZE] = { .property_name = "AccumulatedMemblocksSize", .type = "u", .get_cb = handle_get_accumulated_memblocks_size, .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_CACHE_SIZE]          = { .property_name = "SampleCacheSize",          .type = "u", .get_cb = handle_get_sample_cache_size,          .set_cb = NULL }
};

static pa_dbus_interface_info memstats_interface_info = {
    .name = PA_DBUSIFACE_MEMSTATS_INTERFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = NULL,
    .n_signals = 0
};

static void handle_get_current_memblocks(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    const pa_mempool_stat *stat;
    dbus_uint32_t current_memblocks;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    stat = pa_mempool_get_stat(m->core->mempool);

    current_memblocks = pa_atomic_load(&stat->n_allocated);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &current_memblocks);
}

static void handle_get_current_memblocks_size(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    const pa_mempool_stat *stat;
    dbus_uint32_t current_memblocks_size;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    stat = pa_mempool_get_stat(m->core->mempool);

    current_memblocks_size = pa_atomic_load(&stat->allocated_size);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &current_memblocks_size);
}

static void handle_get_accumulated_memblocks(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    const pa_mempool_stat *stat;
    dbus_uint32_t accumulated_memblocks;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    stat = pa_mempool_get_stat(m->core->mempool);

    accumulated_memblocks = pa_atomic_load(&stat->n_accumulated);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &accumulated_memblocks);
}

static void handle_get_accumulated_memblocks_size(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    const pa_mempool_stat *stat;
    dbus_uint32_t accumulated_memblocks_size;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    stat = pa_mempool_get_stat(m->core->mempool);

    accumulated_memblocks_size = pa_atomic_load(&stat->accumulated_size);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &accumulated_memblocks_size);
}

static void handle_get_sample_cache_size(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    dbus_uint32_t sample_cache_size;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    sample_cache_size = pa_scache_total_size(m->core);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_cache_size);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_memstats *m = userdata;
    const pa_mempool_stat *stat;
    dbus_uint32_t current_memblocks;
    dbus_uint32_t current_memblocks_size;
    dbus_uint32_t accumulated_memblocks;
    dbus_uint32_t accumulated_memblocks_size;
    dbus_uint32_t sample_cache_size;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(m);

    stat = pa_mempool_get_stat(m->core->mempool);

    current_memblocks = pa_atomic_load(&stat->n_allocated);
    current_memblocks_size = pa_atomic_load(&stat->allocated_size);
    accumulated_memblocks = pa_atomic_load(&stat->n_accumulated);
    accumulated_memblocks_size = pa_atomic_load(&stat->accumulated_size);
    sample_cache_size = pa_scache_total_size(m->core);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CURRENT_MEMBLOCKS].property_name, DBUS_TYPE_UINT32, &current_memblocks);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CURRENT_MEMBLOCKS_SIZE].property_name, DBUS_TYPE_UINT32, &current_memblocks_size);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS].property_name, DBUS_TYPE_UINT32, &accumulated_memblocks);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ACCUMULATED_MEMBLOCKS_SIZE].property_name, DBUS_TYPE_UINT32, &accumulated_memblocks_size);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_CACHE_SIZE].property_name, DBUS_TYPE_UINT32, &sample_cache_size);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);
}

pa_dbusiface_memstats *pa_dbusiface_memstats_new(pa_dbusiface_core *dbus_core, pa_core *core) {
    pa_dbusiface_memstats *m;

    pa_assert(dbus_core);
    pa_assert(core);

    m = pa_xnew(pa_dbusiface_memstats, 1);
    m->core = core;
    m->path = pa_sprintf_malloc("%s/%s", PA_DBUS_CORE_OBJECT_PATH, OBJECT_NAME);
    m->dbus_protocol = pa_dbus_protocol_get(core);

    pa_assert_se(pa_dbus_protocol_add_interface(m->dbus_protocol, m->path, &memstats_interface_info, m) >= 0);

    return m;
}

void pa_dbusiface_memstats_free(pa_dbusiface_memstats *m) {
    pa_assert(m);

    pa_assert_se(pa_dbus_protocol_remove_interface(m->dbus_protocol, m->path, memstats_interface_info.name) >= 0);

    pa_xfree(m->path);

    pa_dbus_protocol_unref(m->dbus_protocol);

    pa_xfree(m);
}

const char *pa_dbusiface_memstats_get_path(pa_dbusiface_memstats *m) {
    pa_assert(m);

    return m->path;
}
