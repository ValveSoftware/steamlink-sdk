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
#include <pulsecore/namereg.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-sample.h"

#define OBJECT_NAME "sample"

struct pa_dbusiface_sample {
    pa_dbusiface_core *core;

    pa_scache_entry *sample;
    char *path;
    pa_proplist *proplist;

    pa_hook_slot *sample_cache_changed_slot;

    pa_dbus_protocol *dbus_protocol;
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_default_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_duration(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_bytes(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_play(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_play_to_sink(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_remove(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_SAMPLE_FORMAT,
    PROPERTY_HANDLER_SAMPLE_RATE,
    PROPERTY_HANDLER_CHANNELS,
    PROPERTY_HANDLER_DEFAULT_VOLUME,
    PROPERTY_HANDLER_DURATION,
    PROPERTY_HANDLER_BYTES,
    PROPERTY_HANDLER_PROPERTY_LIST,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]          = { .property_name = "Index",         .type = "u",      .get_cb = handle_get_index,          .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]           = { .property_name = "Name",          .type = "s",      .get_cb = handle_get_name,           .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_FORMAT]  = { .property_name = "SampleFormat",  .type = "u",      .get_cb = handle_get_sample_format,  .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_RATE]    = { .property_name = "SampleRate",    .type = "u",      .get_cb = handle_get_sample_rate,    .set_cb = NULL },
    [PROPERTY_HANDLER_CHANNELS]       = { .property_name = "Channels",      .type = "au",     .get_cb = handle_get_channels,       .set_cb = NULL },
    [PROPERTY_HANDLER_DEFAULT_VOLUME] = { .property_name = "DefaultVolume", .type = "au",     .get_cb = handle_get_default_volume, .set_cb = NULL },
    [PROPERTY_HANDLER_DURATION]       = { .property_name = "Duration",      .type = "t",      .get_cb = handle_get_duration,       .set_cb = NULL },
    [PROPERTY_HANDLER_BYTES]          = { .property_name = "Bytes",         .type = "u",      .get_cb = handle_get_bytes,          .set_cb = NULL },
    [PROPERTY_HANDLER_PROPERTY_LIST]  = { .property_name = "PropertyList",  .type = "a{say}", .get_cb = handle_get_property_list,  .set_cb = NULL }
};

enum method_handler_index {
    METHOD_HANDLER_PLAY,
    METHOD_HANDLER_PLAY_TO_SINK,
    METHOD_HANDLER_REMOVE,
    METHOD_HANDLER_MAX
};

static pa_dbus_arg_info play_args[] = { { "volume", "u", "in" }, { "property_list", "a{say}", "in" } };
static pa_dbus_arg_info play_to_sink_args[] = { { "sink",          "o",      "in" },
                                                { "volume",        "u",      "in" },
                                                { "property_list", "a{say}", "in" } };

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_PLAY] = {
        .method_name = "Play",
        .arguments = play_args,
        .n_arguments = sizeof(play_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_play },
    [METHOD_HANDLER_PLAY_TO_SINK] = {
        .method_name = "PlayToSink",
        .arguments = play_to_sink_args,
        .n_arguments = sizeof(play_to_sink_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_play_to_sink },
    [METHOD_HANDLER_REMOVE] = {
        .method_name = "Remove",
        .arguments = NULL,
        .n_arguments = 0,
        .receive_cb = handle_remove }
};

enum signal_index {
    SIGNAL_PROPERTY_LIST_UPDATED,
    SIGNAL_MAX
};

static pa_dbus_arg_info property_list_updated_args[] = { { "property_list", "a{say}", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_PROPERTY_LIST_UPDATED] = { .name = "PropertyListUpdated", .arguments = property_list_updated_args, .n_arguments = 1 }
};

static pa_dbus_interface_info sample_interface_info = {
    .name = PA_DBUSIFACE_SAMPLE_INTERFACE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t idx = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    idx = s->sample->index;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &idx);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &s->sample->name);
}

static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t sample_format = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->memchunk.memblock) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s isn't loaded into memory yet, so its sample format is unknown.", s->sample->name);
        return;
    }

    sample_format = s->sample->sample_spec.format;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_format);
}

static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t sample_rate = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->memchunk.memblock) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s isn't loaded into memory yet, so its sample rate is unknown.", s->sample->name);
        return;
    }

    sample_rate = s->sample->sample_spec.rate;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_rate);
}

static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->memchunk.memblock) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s isn't loaded into memory yet, so its channel map is unknown.", s->sample->name);
        return;
    }

    for (i = 0; i < s->sample->channel_map.channels; ++i)
        channels[i] = s->sample->channel_map.map[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, channels, s->sample->channel_map.channels);
}

static void handle_get_default_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t default_volume[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->volume_is_set) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s doesn't have default volume stored.", s->sample->name);
        return;
    }

    for (i = 0; i < s->sample->volume.channels; ++i)
        default_volume[i] = s->sample->volume.values[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, default_volume, s->sample->volume.channels);
}

static void handle_get_duration(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint64_t duration = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->memchunk.memblock) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s isn't loaded into memory yet, so its duration is unknown.", s->sample->name);
        return;
    }

    duration = pa_bytes_to_usec(s->sample->memchunk.length, &s->sample->sample_spec);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT64, &duration);
}

static void handle_get_bytes(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    dbus_uint32_t bytes = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->sample->memchunk.memblock) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "Sample %s isn't loaded into memory yet, so its size is unknown.", s->sample->name);
        return;
    }

    bytes = s->sample->memchunk.length;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &bytes);
}

static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_dbus_send_proplist_variant_reply(conn, msg, s->proplist);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t idx = 0;
    dbus_uint32_t sample_format = 0;
    dbus_uint32_t sample_rate = 0;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    dbus_uint32_t default_volume[PA_CHANNELS_MAX];
    dbus_uint64_t duration = 0;
    dbus_uint32_t bytes = 0;
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    idx = s->sample->index;
    if (s->sample->memchunk.memblock) {
        sample_format = s->sample->sample_spec.format;
        sample_rate = s->sample->sample_spec.rate;
        for (i = 0; i < s->sample->channel_map.channels; ++i)
            channels[i] = s->sample->channel_map.map[i];
        duration = pa_bytes_to_usec(s->sample->memchunk.length, &s->sample->sample_spec);
        bytes = s->sample->memchunk.length;
    }
    if (s->sample->volume_is_set) {
        for (i = 0; i < s->sample->volume.channels; ++i)
            default_volume[i] = s->sample->volume.values[i];
    }

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &idx);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &s->sample->name);

    if (s->sample->memchunk.memblock) {
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_FORMAT].property_name, DBUS_TYPE_UINT32, &sample_format);
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_RATE].property_name, DBUS_TYPE_UINT32, &sample_rate);
        pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CHANNELS].property_name, DBUS_TYPE_UINT32, channels, s->sample->channel_map.channels);
    }

    if (s->sample->volume_is_set)
        pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEFAULT_VOLUME].property_name, DBUS_TYPE_UINT32, default_volume, s->sample->volume.channels);

    if (s->sample->memchunk.memblock) {
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DURATION].property_name, DBUS_TYPE_UINT64, &duration);
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_BYTES].property_name, DBUS_TYPE_UINT32, &bytes);
    }

    pa_dbus_append_proplist_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PROPERTY_LIST].property_name, s->proplist);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

static void handle_play(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    DBusMessageIter msg_iter;
    dbus_uint32_t volume = 0;
    pa_proplist *property_list = NULL;
    pa_sink *sink = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_assert_se(dbus_message_iter_init(msg, &msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &volume);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    if (!(property_list = pa_dbus_get_proplist_arg(conn, msg, &msg_iter)))
        return;

    if (!PA_VOLUME_IS_VALID(volume)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid volume.");
        goto finish;
    }

    if (!(sink = pa_namereg_get_default_sink(s->sample->core))) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED,
                           "Can't play sample %s, because there are no sinks available.", s->sample->name);
        goto finish;
    }

    if (pa_scache_play_item(s->sample->core, s->sample->name, sink, volume, property_list, NULL) < 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Playing sample %s failed.", s->sample->name);
        goto finish;
    }

    pa_dbus_send_empty_reply(conn, msg);

finish:
    if (property_list)
        pa_proplist_free(property_list);
}

static void handle_play_to_sink(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;
    DBusMessageIter msg_iter;
    const char *sink_path = NULL;
    dbus_uint32_t volume = 0;
    pa_proplist *property_list = NULL;
    pa_sink *sink = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_assert_se(dbus_message_iter_init(msg, &msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &sink_path);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &volume);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    if (!(property_list = pa_dbus_get_proplist_arg(conn, msg, &msg_iter)))
        return;

    if (!(sink = pa_dbusiface_core_get_sink(s->core, sink_path))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such sink.", sink_path);
        goto finish;
    }

    if (!PA_VOLUME_IS_VALID(volume)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid volume.");
        goto finish;
    }

    if (pa_scache_play_item(s->sample->core, s->sample->name, sink, volume, property_list, NULL) < 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Playing sample %s failed.", s->sample->name);
        goto finish;
    }

    pa_dbus_send_empty_reply(conn, msg);

finish:
    if (property_list)
        pa_proplist_free(property_list);
}

static void handle_remove(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_sample *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (pa_scache_remove_item(s->sample->core, s->sample->name) < 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Removing sample %s failed.", s->sample->name);
        return;
    }

    pa_dbus_send_empty_reply(conn, msg);
}

static pa_hook_result_t sample_cache_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_sample *sample_iface = slot_data;
    pa_scache_entry *sample = call_data;
    DBusMessage *signal_msg;

    pa_assert(sample);
    pa_assert(sample_iface);

    if (sample_iface->sample != sample)
        return PA_HOOK_OK;

    if (!pa_proplist_equal(sample_iface->proplist, sample_iface->sample->proplist)) {
        DBusMessageIter msg_iter;

        pa_proplist_update(sample_iface->proplist, PA_UPDATE_SET, sample_iface->sample->proplist);

        pa_assert_se(signal_msg = dbus_message_new_signal(sample_iface->path,
                                                          PA_DBUSIFACE_SAMPLE_INTERFACE,
                                                          signals[SIGNAL_PROPERTY_LIST_UPDATED].name));
        dbus_message_iter_init_append(signal_msg, &msg_iter);
        pa_dbus_append_proplist(&msg_iter, sample_iface->proplist);

        pa_dbus_protocol_send_signal(sample_iface->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

pa_dbusiface_sample *pa_dbusiface_sample_new(pa_dbusiface_core *core, pa_scache_entry *sample) {
    pa_dbusiface_sample *s = NULL;

    pa_assert(core);
    pa_assert(sample);

    s = pa_xnew0(pa_dbusiface_sample, 1);
    s->core = core;
    s->sample = sample;
    s->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, OBJECT_NAME, sample->index);
    s->proplist = pa_proplist_copy(sample->proplist);
    s->dbus_protocol = pa_dbus_protocol_get(sample->core);
    s->sample_cache_changed_slot = pa_hook_connect(&sample->core->hooks[PA_CORE_HOOK_SAMPLE_CACHE_CHANGED],
                                                   PA_HOOK_NORMAL, sample_cache_changed_cb, s);

    pa_assert_se(pa_dbus_protocol_add_interface(s->dbus_protocol, s->path, &sample_interface_info, s) >= 0);

    return s;
}

void pa_dbusiface_sample_free(pa_dbusiface_sample *s) {
    pa_assert(s);

    pa_assert_se(pa_dbus_protocol_remove_interface(s->dbus_protocol, s->path, sample_interface_info.name) >= 0);

    pa_hook_slot_free(s->sample_cache_changed_slot);
    pa_proplist_free(s->proplist);
    pa_dbus_protocol_unref(s->dbus_protocol);

    pa_xfree(s->path);
    pa_xfree(s);
}

const char *pa_dbusiface_sample_get_path(pa_dbusiface_sample *s) {
    pa_assert(s);

    return s->path;
}
