/***
  This file is part of PulseAudio.

  Copyright 2009 Tanu Kaskinen
  Copyright 2009 Vincent Filali-Ansary <filali.v@azurdigitalnetworks.net>

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
#include <pulsecore/protocol-dbus.h>

#include "iface-stream.h"

#define PLAYBACK_OBJECT_NAME "playback_stream"
#define RECORD_OBJECT_NAME "record_stream"

enum stream_type {
    STREAM_TYPE_PLAYBACK,
    STREAM_TYPE_RECORD
};

struct pa_dbusiface_stream {
    pa_dbusiface_core *core;

    union {
        pa_sink_input *sink_input;
        pa_source_output *source_output;
    };
    enum stream_type type;
    char *path;
    union {
        pa_sink *sink;
        pa_source *source;
    };
    uint32_t sample_rate;
    pa_cvolume volume;
    dbus_bool_t mute;
    pa_proplist *proplist;

    bool has_volume;

    pa_dbus_protocol *dbus_protocol;
    pa_hook_slot *send_event_slot;
    pa_hook_slot *move_finish_slot;
    pa_hook_slot *volume_changed_slot;
    pa_hook_slot *mute_changed_slot;
    pa_hook_slot *proplist_changed_slot;
    pa_hook_slot *state_changed_slot;
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_driver(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_owner_module(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_client(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_device(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_volume(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_mute(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_mute(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_buffer_latency(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_device_latency(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_resample_method(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_move(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_kill(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum property_handler_index {
    PROPERTY_HANDLER_INDEX,
    PROPERTY_HANDLER_DRIVER,
    PROPERTY_HANDLER_OWNER_MODULE,
    PROPERTY_HANDLER_CLIENT,
    PROPERTY_HANDLER_DEVICE,
    PROPERTY_HANDLER_SAMPLE_FORMAT,
    PROPERTY_HANDLER_SAMPLE_RATE,
    PROPERTY_HANDLER_CHANNELS,
    PROPERTY_HANDLER_VOLUME,
    PROPERTY_HANDLER_MUTE,
    PROPERTY_HANDLER_BUFFER_LATENCY,
    PROPERTY_HANDLER_DEVICE_LATENCY,
    PROPERTY_HANDLER_RESAMPLE_METHOD,
    PROPERTY_HANDLER_PROPERTY_LIST,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INDEX]           = { .property_name = "Index",          .type = "u",      .get_cb = handle_get_index,           .set_cb = NULL },
    [PROPERTY_HANDLER_DRIVER]          = { .property_name = "Driver",         .type = "s",      .get_cb = handle_get_driver,          .set_cb = NULL },
    [PROPERTY_HANDLER_OWNER_MODULE]    = { .property_name = "OwnerModule",    .type = "o",      .get_cb = handle_get_owner_module,    .set_cb = NULL },
    [PROPERTY_HANDLER_CLIENT]          = { .property_name = "Client",         .type = "o",      .get_cb = handle_get_client,          .set_cb = NULL },
    [PROPERTY_HANDLER_DEVICE]          = { .property_name = "Device",         .type = "o",      .get_cb = handle_get_device,          .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_FORMAT]   = { .property_name = "SampleFormat",   .type = "u",      .get_cb = handle_get_sample_format,   .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLE_RATE]     = { .property_name = "SampleRate",     .type = "u",      .get_cb = handle_get_sample_rate,     .set_cb = NULL },
    [PROPERTY_HANDLER_CHANNELS]        = { .property_name = "Channels",       .type = "au",     .get_cb = handle_get_channels,        .set_cb = NULL },
    [PROPERTY_HANDLER_VOLUME]          = { .property_name = "Volume",         .type = "au",     .get_cb = handle_get_volume,          .set_cb = handle_set_volume },
    [PROPERTY_HANDLER_MUTE]            = { .property_name = "Mute",           .type = "b",      .get_cb = handle_get_mute,            .set_cb = handle_set_mute },
    [PROPERTY_HANDLER_BUFFER_LATENCY]  = { .property_name = "BufferLatency",  .type = "t",      .get_cb = handle_get_buffer_latency,  .set_cb = NULL },
    [PROPERTY_HANDLER_DEVICE_LATENCY]  = { .property_name = "DeviceLatency",  .type = "t",      .get_cb = handle_get_device_latency,  .set_cb = NULL },
    [PROPERTY_HANDLER_RESAMPLE_METHOD] = { .property_name = "ResampleMethod", .type = "s",      .get_cb = handle_get_resample_method, .set_cb = NULL },
    [PROPERTY_HANDLER_PROPERTY_LIST]   = { .property_name = "PropertyList",   .type = "a{say}", .get_cb = handle_get_property_list,   .set_cb = NULL }
};

enum method_handler_index {
    METHOD_HANDLER_MOVE,
    METHOD_HANDLER_KILL,
    METHOD_HANDLER_MAX
};

static pa_dbus_arg_info move_args[] = { { "device", "o", "in" } };

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_MOVE] = {
        .method_name = "Move",
        .arguments = move_args,
        .n_arguments = sizeof(move_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_move },
    [METHOD_HANDLER_KILL] = {
        .method_name = "Kill",
        .arguments = NULL,
        .n_arguments = 0,
        .receive_cb = handle_kill }
};

enum signal_index {
    SIGNAL_DEVICE_UPDATED,
    SIGNAL_SAMPLE_RATE_UPDATED,
    SIGNAL_VOLUME_UPDATED,
    SIGNAL_MUTE_UPDATED,
    SIGNAL_PROPERTY_LIST_UPDATED,
    SIGNAL_STREAM_EVENT,
    SIGNAL_MAX
};

static pa_dbus_arg_info device_updated_args[]        = { { "device",        "o",      NULL } };
static pa_dbus_arg_info sample_rate_updated_args[]   = { { "sample_rate",   "u",      NULL } };
static pa_dbus_arg_info volume_updated_args[]        = { { "volume",        "au",     NULL } };
static pa_dbus_arg_info mute_updated_args[]          = { { "muted",         "b",      NULL } };
static pa_dbus_arg_info property_list_updated_args[] = { { "property_list", "a{say}", NULL } };
static pa_dbus_arg_info stream_event_args[]          = { { "name",          "s",      NULL }, { "property_list", "a{say}", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_DEVICE_UPDATED]        = { .name = "DeviceUpdated",       .arguments = device_updated_args,        .n_arguments = 1 },
    [SIGNAL_SAMPLE_RATE_UPDATED]   = { .name = "SampleRateUpdated",   .arguments = sample_rate_updated_args,   .n_arguments = 1 },
    [SIGNAL_VOLUME_UPDATED]        = { .name = "VolumeUpdated",       .arguments = volume_updated_args,        .n_arguments = 1 },
    [SIGNAL_MUTE_UPDATED]          = { .name = "MuteUpdated",         .arguments = mute_updated_args,          .n_arguments = 1 },
    [SIGNAL_PROPERTY_LIST_UPDATED] = { .name = "PropertyListUpdated", .arguments = property_list_updated_args, .n_arguments = 1 },
    [SIGNAL_STREAM_EVENT]          = { .name = "StreamEvent",         .arguments = stream_event_args,          .n_arguments = sizeof(stream_event_args) / sizeof(pa_dbus_arg_info) }
};

static pa_dbus_interface_info stream_interface_info = {
    .name = PA_DBUSIFACE_STREAM_INTERFACE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static void handle_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_uint32_t idx;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    idx = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->index : s->source_output->index;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &idx);
}

/* The returned string has to be freed with pa_xfree() by the caller. */
static char *stream_to_string(pa_dbusiface_stream *s) {
    if (s->type == STREAM_TYPE_PLAYBACK)
        return pa_sprintf_malloc("Playback stream %u", (unsigned) s->sink_input->index);
    else
        return pa_sprintf_malloc("Record stream %u", (unsigned) s->source_output->index);
}

static void handle_get_driver(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    const char *driver = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    driver = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->driver : s->source_output->driver;

    if (!driver) {
        char *str = stream_to_string(s);

        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s doesn't have a driver.", str);
        pa_xfree(str);

        return;
    }

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &driver);
}

static void handle_get_owner_module(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    pa_module *owner_module = NULL;
    const char *object_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    owner_module = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->module : s->source_output->module;

    if (!owner_module) {
        char *str = stream_to_string(s);

        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s doesn't have an owner module.", str);
        pa_xfree(str);

        return;
    }

    object_path = pa_dbusiface_core_get_module_path(s->core, owner_module);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_client(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    pa_client *client = NULL;
    const char *object_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    client = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->client : s->source_output->client;

    if (!client) {
        char *str = stream_to_string(s);

        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s isn't associated to any client.", str);
        pa_xfree(str);

        return;
    }

    object_path = pa_dbusiface_core_get_client_path(s->core, client);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_device(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    const char *device = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK)
        device = pa_dbusiface_core_get_sink_path(s->core, s->sink);
    else
        device = pa_dbusiface_core_get_source_path(s->core, s->source);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &device);
}

static void handle_get_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_uint32_t sample_format = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    sample_format = (s->type == STREAM_TYPE_PLAYBACK)
                    ? s->sink_input->sample_spec.format
                    : s->source_output->sample_spec.format;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &sample_format);
}

static void handle_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &s->sample_rate);
}

static void handle_get_channels(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    pa_channel_map *channel_map = NULL;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    channel_map = (s->type == STREAM_TYPE_PLAYBACK) ? &s->sink_input->channel_map : &s->source_output->channel_map;

    for (i = 0; i < channel_map->channels; ++i)
        channels[i] = channel_map->map[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, channels, channel_map->channels);
}

static void handle_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_uint32_t volume[PA_CHANNELS_MAX];
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (!s->has_volume) {
        char *str = stream_to_string(s);

        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s doesn't have volume.", str);
        pa_xfree(str);

        return;
    }

    for (i = 0; i < s->volume.channels; ++i)
        volume[i] = s->volume.values[i];

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, volume, s->volume.channels);
}

static void handle_set_volume(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    bool volume_writable = true;
    DBusMessageIter array_iter;
    int stream_channels = 0;
    dbus_uint32_t *volume = NULL;
    int n_volume_entries = 0;
    pa_cvolume new_vol;
    int i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(s);

    volume_writable = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->volume_writable : false;

    if (!s->has_volume || !volume_writable) {
        char *str = stream_to_string(s);

        if (!s->has_volume)
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "%s doesn't have volume.", str);
        else if (!volume_writable)
            pa_dbus_send_error(conn, msg, DBUS_ERROR_ACCESS_DENIED, "%s has read-only volume.", str);
        pa_xfree(str);

        return;
    }

    stream_channels = s->sink_input->channel_map.channels;

    dbus_message_iter_recurse(iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &volume, &n_volume_entries);

    if (n_volume_entries != stream_channels && n_volume_entries != 1) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "Expected %u volume entries, got %u.", stream_channels, n_volume_entries);
        return;
    }

    pa_cvolume_init(&new_vol);
    new_vol.channels = n_volume_entries;

    for (i = 0; i < n_volume_entries; ++i) {
        if (!PA_VOLUME_IS_VALID(volume[i])) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid volume: %u", volume[i]);
            return;
        }
        new_vol.values[i] = volume[i];
    }

    pa_sink_input_set_volume(s->sink_input, &new_vol, true, true);

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_mute(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_RECORD) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "Record streams don't have mute.");
        return;
    }

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &s->mute);
}

static void handle_set_mute(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_bool_t mute = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(s);

    dbus_message_iter_get_basic(iter, &mute);

    if (s->type == STREAM_TYPE_RECORD) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY, "Record streams don't have mute.");
        return;
    }

    pa_sink_input_set_mute(s->sink_input, mute, true);

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_buffer_latency(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_uint64_t buffer_latency = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK)
        buffer_latency = pa_sink_input_get_latency(s->sink_input, NULL);
    else
        buffer_latency = pa_source_output_get_latency(s->source_output, NULL);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT64, &buffer_latency);
}

static void handle_get_device_latency(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    dbus_uint64_t device_latency = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK)
        pa_sink_input_get_latency(s->sink_input, &device_latency);
    else
        pa_source_output_get_latency(s->source_output, &device_latency);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT64, &device_latency);
}

static void handle_get_resample_method(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    const char *resample_method = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK)
        resample_method = pa_resample_method_to_string(s->sink_input->actual_resample_method);
    else
        resample_method = pa_resample_method_to_string(s->source_output->actual_resample_method);

    if (!resample_method)
        resample_method = "";

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &resample_method);
}

static void handle_get_property_list(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_dbus_send_proplist_variant_reply(conn, msg, s->proplist);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t idx = 0;
    const char *driver = NULL;
    pa_module *owner_module = NULL;
    const char *owner_module_path = NULL;
    pa_client *client = NULL;
    const char *client_path = NULL;
    const char *device = NULL;
    dbus_uint32_t sample_format = 0;
    pa_channel_map *channel_map = NULL;
    dbus_uint32_t channels[PA_CHANNELS_MAX];
    dbus_uint32_t volume[PA_CHANNELS_MAX];
    dbus_uint64_t buffer_latency = 0;
    dbus_uint64_t device_latency = 0;
    const char *resample_method = NULL;
    unsigned i = 0;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->has_volume) {
        for (i = 0; i < s->volume.channels; ++i)
            volume[i] = s->volume.values[i];
    }

    if (s->type == STREAM_TYPE_PLAYBACK) {
        idx = s->sink_input->index;
        driver = s->sink_input->driver;
        owner_module = s->sink_input->module;
        client = s->sink_input->client;
        device = pa_dbusiface_core_get_sink_path(s->core, s->sink);
        sample_format = s->sink_input->sample_spec.format;
        channel_map = &s->sink_input->channel_map;
        buffer_latency = pa_sink_input_get_latency(s->sink_input, &device_latency);
        resample_method = pa_resample_method_to_string(s->sink_input->actual_resample_method);
    } else {
        idx = s->source_output->index;
        driver = s->source_output->driver;
        owner_module = s->source_output->module;
        client = s->source_output->client;
        device = pa_dbusiface_core_get_source_path(s->core, s->source);
        sample_format = s->source_output->sample_spec.format;
        channel_map = &s->source_output->channel_map;
        buffer_latency = pa_source_output_get_latency(s->source_output, &device_latency);
        resample_method = pa_resample_method_to_string(s->source_output->actual_resample_method);
    }
    if (owner_module)
        owner_module_path = pa_dbusiface_core_get_module_path(s->core, owner_module);
    if (client)
        client_path = pa_dbusiface_core_get_client_path(s->core, client);
    for (i = 0; i < channel_map->channels; ++i)
        channels[i] = channel_map->map[i];
    if (!resample_method)
        resample_method = "";

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &idx);

    if (driver)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DRIVER].property_name, DBUS_TYPE_STRING, &driver);

    if (owner_module)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_OWNER_MODULE].property_name, DBUS_TYPE_OBJECT_PATH, &owner_module_path);

    if (client)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CLIENT].property_name, DBUS_TYPE_OBJECT_PATH, &client_path);

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEVICE].property_name, DBUS_TYPE_OBJECT_PATH, &device);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_FORMAT].property_name, DBUS_TYPE_UINT32, &sample_format);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLE_RATE].property_name, DBUS_TYPE_UINT32, &s->sample_rate);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CHANNELS].property_name, DBUS_TYPE_UINT32, channels, channel_map->channels);

    if (s->has_volume) {
        pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_VOLUME].property_name, DBUS_TYPE_UINT32, volume, s->volume.channels);
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_MUTE].property_name, DBUS_TYPE_BOOLEAN, &s->mute);
    }

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_BUFFER_LATENCY].property_name, DBUS_TYPE_UINT64, &buffer_latency);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEVICE_LATENCY].property_name, DBUS_TYPE_UINT64, &device_latency);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_RESAMPLE_METHOD].property_name, DBUS_TYPE_STRING, &resample_method);
    pa_dbus_append_proplist_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PROPERTY_LIST].property_name, s->proplist);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

static void handle_move(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;
    const char *device = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &device, DBUS_TYPE_INVALID));

    if (s->type == STREAM_TYPE_PLAYBACK) {
        pa_sink *sink = pa_dbusiface_core_get_sink(s->core, device);

        if (!sink) {
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such sink.", device);
            return;
        }

        if (pa_sink_input_move_to(s->sink_input, sink, true) < 0) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED,
                               "Moving playback stream %u to sink %s failed.", s->sink_input->index, sink->name);
            return;
        }
    } else {
        pa_source *source = pa_dbusiface_core_get_source(s->core, device);

        if (!source) {
            pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such source.", device);
            return;
        }

        if (pa_source_output_move_to(s->source_output, source, true) < 0) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED,
                               "Moving record stream %u to source %s failed.", s->source_output->index, source->name);
            return;
        }
    }

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_kill(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_stream *s = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK)
        pa_sink_input_kill(s->sink_input);
    else
        pa_source_output_kill(s->source_output);

    pa_dbus_send_empty_reply(conn, msg);
}

static void check_and_signal_rate(pa_dbusiface_stream *s) {
    DBusMessage *signal_msg = NULL;
    uint32_t new_sample_rate = 0;

    pa_assert(s);

    new_sample_rate = (s->type == STREAM_TYPE_PLAYBACK)
                      ? s->sink_input->sample_spec.rate
                      : s->source_output->sample_spec.rate;

    if (s->sample_rate != new_sample_rate) {
        s->sample_rate = new_sample_rate;

        pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                          PA_DBUSIFACE_STREAM_INTERFACE,
                                                          signals[SIGNAL_SAMPLE_RATE_UPDATED].name));
        pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_UINT32, &s->sample_rate, DBUS_TYPE_INVALID));

        pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }
}

static pa_hook_result_t move_finish_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;
    const char *new_device_path = NULL;
    DBusMessage *signal_msg = NULL;

    if ((s->type == STREAM_TYPE_PLAYBACK && s->sink_input != call_data) ||
        (s->type == STREAM_TYPE_RECORD && s->source_output != call_data))
        return PA_HOOK_OK;

    if (s->type == STREAM_TYPE_PLAYBACK) {
        pa_sink *new_sink = s->sink_input->sink;

        if (s->sink != new_sink) {
            pa_sink_unref(s->sink);
            s->sink = pa_sink_ref(new_sink);

            new_device_path = pa_dbusiface_core_get_sink_path(s->core, new_sink);

            pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                              PA_DBUSIFACE_STREAM_INTERFACE,
                                                              signals[SIGNAL_DEVICE_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &new_device_path, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
            dbus_message_unref(signal_msg);
        }
    } else {
        pa_source *new_source = s->source_output->source;

        if (s->source != new_source) {
            pa_source_unref(s->source);
            s->source = pa_source_ref(new_source);

            new_device_path = pa_dbusiface_core_get_source_path(s->core, new_source);

            pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                              PA_DBUSIFACE_STREAM_INTERFACE,
                                                              signals[SIGNAL_DEVICE_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &new_device_path, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
            dbus_message_unref(signal_msg);
        }
    }

    check_and_signal_rate(s);

    return PA_HOOK_OK;
}

static pa_hook_result_t volume_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;
    DBusMessage *signal_msg = NULL;
    unsigned i = 0;

    if ((s->type == STREAM_TYPE_PLAYBACK && s->sink_input != call_data) ||
        (s->type == STREAM_TYPE_RECORD && s->source_output != call_data))
        return PA_HOOK_OK;

    if (s->type == STREAM_TYPE_PLAYBACK && s->has_volume) {
        pa_cvolume new_volume;

        pa_sink_input_get_volume(s->sink_input, &new_volume, true);

        if (!pa_cvolume_equal(&s->volume, &new_volume)) {
            dbus_uint32_t volume[PA_CHANNELS_MAX];
            dbus_uint32_t *volume_ptr = volume;

            s->volume = new_volume;

            for (i = 0; i < s->volume.channels; ++i)
                volume[i] = s->volume.values[i];

            pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                              PA_DBUSIFACE_STREAM_INTERFACE,
                                                              signals[SIGNAL_VOLUME_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal_msg,
                                                  DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &volume_ptr, s->volume.channels,
                                                  DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
            dbus_message_unref(signal_msg);
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t mute_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;
    DBusMessage *signal_msg = NULL;

    if ((s->type == STREAM_TYPE_PLAYBACK && s->sink_input != call_data) ||
        (s->type == STREAM_TYPE_RECORD && s->source_output != call_data))
        return PA_HOOK_OK;

    if (s->type == STREAM_TYPE_PLAYBACK) {
        bool new_mute = false;

        new_mute = s->sink_input->muted;

        if (s->mute != new_mute) {
            s->mute = new_mute;

            pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                              PA_DBUSIFACE_STREAM_INTERFACE,
                                                              signals[SIGNAL_MUTE_UPDATED].name));
            pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_BOOLEAN, &s->mute, DBUS_TYPE_INVALID));

            pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
            dbus_message_unref(signal_msg);
            signal_msg = NULL;
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t proplist_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;
    DBusMessage *signal_msg = NULL;
    pa_proplist *new_proplist = NULL;

    if ((s->type == STREAM_TYPE_PLAYBACK && s->sink_input != call_data) ||
        (s->type == STREAM_TYPE_RECORD && s->source_output != call_data))
        return PA_HOOK_OK;

    new_proplist = (s->type == STREAM_TYPE_PLAYBACK) ? s->sink_input->proplist : s->source_output->proplist;

    if (!pa_proplist_equal(s->proplist, new_proplist)) {
        DBusMessageIter msg_iter;

        pa_proplist_update(s->proplist, PA_UPDATE_SET, new_proplist);

        pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                          PA_DBUSIFACE_STREAM_INTERFACE,
                                                          signals[SIGNAL_PROPERTY_LIST_UPDATED].name));
        dbus_message_iter_init_append(signal_msg, &msg_iter);
        pa_dbus_append_proplist(&msg_iter, s->proplist);

        pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t state_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;

    pa_assert(s);

    if ((s->type == STREAM_TYPE_PLAYBACK && s->sink_input != call_data) ||
        (s->type == STREAM_TYPE_RECORD && s->source_output != call_data))
        return PA_HOOK_OK;

    check_and_signal_rate(s);

    return PA_HOOK_OK;
}

static pa_hook_result_t send_event_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_stream *s = slot_data;
    DBusMessage *signal_msg = NULL;
    DBusMessageIter msg_iter;
    const char *name = NULL;
    pa_proplist *property_list = NULL;

    pa_assert(call_data);
    pa_assert(s);

    if (s->type == STREAM_TYPE_PLAYBACK) {
        pa_sink_input_send_event_hook_data *data = call_data;

        if (data->sink_input != s->sink_input)
            return PA_HOOK_OK;

        name = data->event;
        property_list = data->data;
    } else {
        pa_source_output_send_event_hook_data *data = call_data;

        if (data->source_output != s->source_output)
            return PA_HOOK_OK;

        name = data->event;
        property_list = data->data;
    }

    pa_assert_se(signal_msg = dbus_message_new_signal(s->path,
                                                      PA_DBUSIFACE_STREAM_INTERFACE,
                                                      signals[SIGNAL_STREAM_EVENT].name));
    dbus_message_iter_init_append(signal_msg, &msg_iter);
    pa_assert_se(dbus_message_iter_append_basic(&msg_iter, DBUS_TYPE_STRING, &name));
    pa_dbus_append_proplist(&msg_iter, property_list);

    pa_dbus_protocol_send_signal(s->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

pa_dbusiface_stream *pa_dbusiface_stream_new_playback(pa_dbusiface_core *core, pa_sink_input *sink_input) {
    pa_dbusiface_stream *s;

    pa_assert(core);
    pa_assert(sink_input);

    s = pa_xnew(pa_dbusiface_stream, 1);
    s->core = core;
    s->sink_input = pa_sink_input_ref(sink_input);
    s->type = STREAM_TYPE_PLAYBACK;
    s->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, PLAYBACK_OBJECT_NAME, sink_input->index);
    s->sink = pa_sink_ref(sink_input->sink);
    s->sample_rate = sink_input->sample_spec.rate;
    s->has_volume = pa_sink_input_is_volume_readable(sink_input);

    if (s->has_volume)
        pa_sink_input_get_volume(sink_input, &s->volume, true);
    else
        pa_cvolume_init(&s->volume);

    s->mute = sink_input->muted;
    s->proplist = pa_proplist_copy(sink_input->proplist);
    s->dbus_protocol = pa_dbus_protocol_get(sink_input->core);
    s->send_event_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_SEND_EVENT],
                                         PA_HOOK_NORMAL,
                                         send_event_cb,
                                         s);
    s->move_finish_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FINISH],
                                          PA_HOOK_NORMAL, move_finish_cb, s);
    s->volume_changed_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_VOLUME_CHANGED],
                                             PA_HOOK_NORMAL, volume_changed_cb, s);
    s->mute_changed_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_MUTE_CHANGED],
                                           PA_HOOK_NORMAL, mute_changed_cb, s);
    s->proplist_changed_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_PROPLIST_CHANGED],
                                               PA_HOOK_NORMAL, proplist_changed_cb, s);
    s->state_changed_slot = pa_hook_connect(&sink_input->core->hooks[PA_CORE_HOOK_SINK_INPUT_STATE_CHANGED],
                                            PA_HOOK_NORMAL, state_changed_cb, s);

    pa_assert_se(pa_dbus_protocol_add_interface(s->dbus_protocol, s->path, &stream_interface_info, s) >= 0);

    return s;
}

pa_dbusiface_stream *pa_dbusiface_stream_new_record(pa_dbusiface_core *core, pa_source_output *source_output) {
    pa_dbusiface_stream *s;

    pa_assert(core);
    pa_assert(source_output);

    s = pa_xnew(pa_dbusiface_stream, 1);
    s->core = core;
    s->source_output = pa_source_output_ref(source_output);
    s->type = STREAM_TYPE_RECORD;
    s->path = pa_sprintf_malloc("%s/%s%u", PA_DBUS_CORE_OBJECT_PATH, RECORD_OBJECT_NAME, source_output->index);
    s->source = pa_source_ref(source_output->source);
    s->sample_rate = source_output->sample_spec.rate;
    pa_cvolume_init(&s->volume);
    s->mute = false;
    s->proplist = pa_proplist_copy(source_output->proplist);
    s->has_volume = false;
    s->dbus_protocol = pa_dbus_protocol_get(source_output->core);
    s->send_event_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_SEND_EVENT],
                                         PA_HOOK_NORMAL,
                                         send_event_cb,
                                         s);
    s->move_finish_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_MOVE_FINISH],
                                          PA_HOOK_NORMAL, move_finish_cb, s);
    s->volume_changed_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_VOLUME_CHANGED],
                                             PA_HOOK_NORMAL, volume_changed_cb, s);
    s->mute_changed_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_MUTE_CHANGED],
                                           PA_HOOK_NORMAL, mute_changed_cb, s);
    s->proplist_changed_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PROPLIST_CHANGED],
                                               PA_HOOK_NORMAL, proplist_changed_cb, s);
    s->state_changed_slot = pa_hook_connect(&source_output->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_STATE_CHANGED],
                                            PA_HOOK_NORMAL, state_changed_cb, s);

    pa_assert_se(pa_dbus_protocol_add_interface(s->dbus_protocol, s->path, &stream_interface_info, s) >= 0);

    return s;
}

void pa_dbusiface_stream_free(pa_dbusiface_stream *s) {
    pa_assert(s);

    pa_assert_se(pa_dbus_protocol_remove_interface(s->dbus_protocol, s->path, stream_interface_info.name) >= 0);

    if (s->type == STREAM_TYPE_PLAYBACK) {
        pa_sink_input_unref(s->sink_input);
        pa_sink_unref(s->sink);
    } else {
        pa_source_output_unref(s->source_output);
        pa_source_unref(s->source);
    }

    pa_proplist_free(s->proplist);
    pa_dbus_protocol_unref(s->dbus_protocol);
    pa_hook_slot_free(s->send_event_slot);
    pa_hook_slot_free(s->move_finish_slot);
    pa_hook_slot_free(s->volume_changed_slot);
    pa_hook_slot_free(s->mute_changed_slot);
    pa_hook_slot_free(s->proplist_changed_slot);
    pa_hook_slot_free(s->state_changed_slot);

    pa_xfree(s->path);
    pa_xfree(s);
}

const char *pa_dbusiface_stream_get_path(pa_dbusiface_stream *s) {
    pa_assert(s);

    return s->path;
}
