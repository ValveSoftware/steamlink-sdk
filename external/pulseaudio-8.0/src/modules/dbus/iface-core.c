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

#include <ctype.h>

#include <dbus/dbus.h>

#include <pulse/utf8.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core-util.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/namereg.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/strbuf.h>

#include "iface-card.h"
#include "iface-client.h"
#include "iface-device.h"
#include "iface-memstats.h"
#include "iface-module.h"
#include "iface-sample.h"
#include "iface-stream.h"

#include "iface-core.h"

#define INTERFACE_REVISION 0

static void handle_get_interface_revision(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_version(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_is_local(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_username(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_hostname(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_default_channels(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_default_channels(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_default_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_default_sample_format(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_default_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_default_sample_rate(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_alternate_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_alternate_sample_rate(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_cards(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sinks(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_fallback_sink(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_fallback_sink(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_sources(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_fallback_source(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_set_fallback_source(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_get_playback_streams(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_record_streams(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_samples(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_modules(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_clients(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_my_client(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_extensions(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_card_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sink_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_source_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_sample_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_upload_sample(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_load_module(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_exit(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_listen_for_signal(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_stop_listening_for_signal(DBusConnection *conn, DBusMessage *msg, void *userdata);

struct pa_dbusiface_core {
    pa_core *core;

    pa_dbus_protocol *dbus_protocol;

    pa_hashmap *cards;
    pa_hashmap *sinks_by_index;
    pa_hashmap *sinks_by_path;
    pa_hashmap *sources_by_index;
    pa_hashmap *sources_by_path;
    pa_hashmap *playback_streams;
    pa_hashmap *record_streams;
    pa_hashmap *samples;
    pa_hashmap *modules;
    pa_hashmap *clients;

    pa_sink *fallback_sink;
    pa_source *fallback_source;

    pa_hook_slot *module_new_slot;
    pa_hook_slot *module_removed_slot;
    pa_hook_slot *default_sink_changed_slot;
    pa_hook_slot *default_source_changed_slot;
    pa_hook_slot *sample_cache_new_slot;
    pa_hook_slot *sample_cache_removed_slot;
    pa_hook_slot *card_put_slot;
    pa_hook_slot *card_unlink_slot;
    pa_hook_slot *sink_input_put_slot;
    pa_hook_slot *sink_input_unlink_slot;
    pa_hook_slot *source_output_put_slot;
    pa_hook_slot *source_output_unlink_slot;
    pa_hook_slot *client_put_slot;
    pa_hook_slot *client_unlink_slot;
    pa_hook_slot *sink_put_slot;
    pa_hook_slot *sink_unlink_slot;
    pa_hook_slot *source_put_slot;
    pa_hook_slot *source_unlink_slot;
    pa_hook_slot *extension_registered_slot;
    pa_hook_slot *extension_unregistered_slot;

    pa_dbusiface_memstats *memstats;
};

enum property_handler_index {
    PROPERTY_HANDLER_INTERFACE_REVISION,
    PROPERTY_HANDLER_NAME,
    PROPERTY_HANDLER_VERSION,
    PROPERTY_HANDLER_IS_LOCAL,
    PROPERTY_HANDLER_USERNAME,
    PROPERTY_HANDLER_HOSTNAME,
    PROPERTY_HANDLER_DEFAULT_CHANNELS,
    PROPERTY_HANDLER_DEFAULT_SAMPLE_FORMAT,
    PROPERTY_HANDLER_DEFAULT_SAMPLE_RATE,
    PROPERTY_HANDLER_ALTERNATE_SAMPLE_RATE,
    PROPERTY_HANDLER_CARDS,
    PROPERTY_HANDLER_SINKS,
    PROPERTY_HANDLER_FALLBACK_SINK,
    PROPERTY_HANDLER_SOURCES,
    PROPERTY_HANDLER_FALLBACK_SOURCE,
    PROPERTY_HANDLER_PLAYBACK_STREAMS,
    PROPERTY_HANDLER_RECORD_STREAMS,
    PROPERTY_HANDLER_SAMPLES,
    PROPERTY_HANDLER_MODULES,
    PROPERTY_HANDLER_CLIENTS,
    PROPERTY_HANDLER_MY_CLIENT,
    PROPERTY_HANDLER_EXTENSIONS,
    PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INTERFACE_REVISION]    = { .property_name = "InterfaceRevision",   .type = "u",  .get_cb = handle_get_interface_revision,    .set_cb = NULL },
    [PROPERTY_HANDLER_NAME]                  = { .property_name = "Name",                .type = "s",  .get_cb = handle_get_name,                  .set_cb = NULL },
    [PROPERTY_HANDLER_VERSION]               = { .property_name = "Version",             .type = "s",  .get_cb = handle_get_version,               .set_cb = NULL },
    [PROPERTY_HANDLER_IS_LOCAL]              = { .property_name = "IsLocal",             .type = "b",  .get_cb = handle_get_is_local,              .set_cb = NULL },
    [PROPERTY_HANDLER_USERNAME]              = { .property_name = "Username",            .type = "s",  .get_cb = handle_get_username,              .set_cb = NULL },
    [PROPERTY_HANDLER_HOSTNAME]              = { .property_name = "Hostname",            .type = "s",  .get_cb = handle_get_hostname,              .set_cb = NULL },
    [PROPERTY_HANDLER_DEFAULT_CHANNELS]      = { .property_name = "DefaultChannels",     .type = "au", .get_cb = handle_get_default_channels,      .set_cb = handle_set_default_channels },
    [PROPERTY_HANDLER_DEFAULT_SAMPLE_FORMAT] = { .property_name = "DefaultSampleFormat", .type = "u",  .get_cb = handle_get_default_sample_format, .set_cb = handle_set_default_sample_format },
    [PROPERTY_HANDLER_DEFAULT_SAMPLE_RATE]   = { .property_name = "DefaultSampleRate",   .type = "u",  .get_cb = handle_get_default_sample_rate,   .set_cb = handle_set_default_sample_rate },
    [PROPERTY_HANDLER_ALTERNATE_SAMPLE_RATE]   = { .property_name = "AlternateSampleRate",   .type = "u",  .get_cb = handle_get_alternate_sample_rate,   .set_cb = handle_set_alternate_sample_rate },
    [PROPERTY_HANDLER_CARDS]                 = { .property_name = "Cards",               .type = "ao", .get_cb = handle_get_cards,                 .set_cb = NULL },
    [PROPERTY_HANDLER_SINKS]                 = { .property_name = "Sinks",               .type = "ao", .get_cb = handle_get_sinks,                 .set_cb = NULL },
    [PROPERTY_HANDLER_FALLBACK_SINK]         = { .property_name = "FallbackSink",        .type = "o",  .get_cb = handle_get_fallback_sink,         .set_cb = handle_set_fallback_sink },
    [PROPERTY_HANDLER_SOURCES]               = { .property_name = "Sources",             .type = "ao", .get_cb = handle_get_sources,               .set_cb = NULL },
    [PROPERTY_HANDLER_FALLBACK_SOURCE]       = { .property_name = "FallbackSource",      .type = "o",  .get_cb = handle_get_fallback_source,       .set_cb = handle_set_fallback_source },
    [PROPERTY_HANDLER_PLAYBACK_STREAMS]      = { .property_name = "PlaybackStreams",     .type = "ao", .get_cb = handle_get_playback_streams,      .set_cb = NULL },
    [PROPERTY_HANDLER_RECORD_STREAMS]        = { .property_name = "RecordStreams",       .type = "ao", .get_cb = handle_get_record_streams,        .set_cb = NULL },
    [PROPERTY_HANDLER_SAMPLES]               = { .property_name = "Samples",             .type = "ao", .get_cb = handle_get_samples,               .set_cb = NULL },
    [PROPERTY_HANDLER_MODULES]               = { .property_name = "Modules",             .type = "ao", .get_cb = handle_get_modules,               .set_cb = NULL },
    [PROPERTY_HANDLER_CLIENTS]               = { .property_name = "Clients",             .type = "ao", .get_cb = handle_get_clients,               .set_cb = NULL },
    [PROPERTY_HANDLER_MY_CLIENT]             = { .property_name = "MyClient",            .type = "o",  .get_cb = handle_get_my_client,             .set_cb = NULL },
    [PROPERTY_HANDLER_EXTENSIONS]            = { .property_name = "Extensions",          .type = "as", .get_cb = handle_get_extensions,            .set_cb = NULL }
};

enum method_handler_index {
    METHOD_HANDLER_GET_CARD_BY_NAME,
    METHOD_HANDLER_GET_SINK_BY_NAME,
    METHOD_HANDLER_GET_SOURCE_BY_NAME,
    METHOD_HANDLER_GET_SAMPLE_BY_NAME,
    METHOD_HANDLER_UPLOAD_SAMPLE,
    METHOD_HANDLER_LOAD_MODULE,
    METHOD_HANDLER_EXIT,
    METHOD_HANDLER_LISTEN_FOR_SIGNAL,
    METHOD_HANDLER_STOP_LISTENING_FOR_SIGNAL,
    METHOD_HANDLER_MAX
};

static pa_dbus_arg_info get_card_by_name_args[] = { { "name", "s", "in" }, { "card", "o", "out" } };
static pa_dbus_arg_info get_sink_by_name_args[] = { { "name", "s", "in" }, { "sink", "o", "out" } };
static pa_dbus_arg_info get_source_by_name_args[] = { { "name", "s", "in" }, { "source", "o", "out" } };
static pa_dbus_arg_info get_sample_by_name_args[] = { { "name", "s", "in" }, { "sample", "o", "out" } };
static pa_dbus_arg_info upload_sample_args[] = { { "name",           "s",      "in" },
                                                 { "sample_format",  "u",      "in" },
                                                 { "sample_rate",    "u",      "in" },
                                                 { "channels",       "au",     "in" },
                                                 { "default_volume", "au",     "in" },
                                                 { "property_list",  "a{say}", "in" },
                                                 { "data",           "ay",     "in" },
                                                 { "sample",         "o",      "out" } };
static pa_dbus_arg_info load_module_args[] = { { "name", "s", "in" }, { "arguments", "a{ss}", "in" }, { "module", "o", "out" } };
static pa_dbus_arg_info listen_for_signal_args[] = { { "signal", "s", "in" }, { "objects", "ao", "in" } };
static pa_dbus_arg_info stop_listening_for_signal_args[] = { { "signal", "s", "in" } };

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_GET_CARD_BY_NAME] = {
        .method_name = "GetCardByName",
        .arguments = get_card_by_name_args,
        .n_arguments = sizeof(get_card_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_card_by_name },
    [METHOD_HANDLER_GET_SINK_BY_NAME] = {
        .method_name = "GetSinkByName",
        .arguments = get_sink_by_name_args,
        .n_arguments = sizeof(get_sink_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_sink_by_name },
    [METHOD_HANDLER_GET_SOURCE_BY_NAME] = {
        .method_name = "GetSourceByName",
        .arguments = get_source_by_name_args,
        .n_arguments = sizeof(get_source_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_source_by_name },
    [METHOD_HANDLER_GET_SAMPLE_BY_NAME] = {
        .method_name = "GetSampleByName",
        .arguments = get_sample_by_name_args,
        .n_arguments = sizeof(get_sample_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_sample_by_name },
    [METHOD_HANDLER_UPLOAD_SAMPLE] = {
        .method_name = "UploadSample",
        .arguments = upload_sample_args,
        .n_arguments = sizeof(upload_sample_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_upload_sample },
    [METHOD_HANDLER_LOAD_MODULE] = {
        .method_name = "LoadModule",
        .arguments = load_module_args,
        .n_arguments = sizeof(load_module_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_load_module },
    [METHOD_HANDLER_EXIT] = {
        .method_name = "Exit",
        .arguments = NULL,
        .n_arguments = 0,
        .receive_cb = handle_exit },
    [METHOD_HANDLER_LISTEN_FOR_SIGNAL] = {
        .method_name = "ListenForSignal",
        .arguments = listen_for_signal_args,
        .n_arguments = sizeof(listen_for_signal_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_listen_for_signal },
    [METHOD_HANDLER_STOP_LISTENING_FOR_SIGNAL] = {
        .method_name = "StopListeningForSignal",
        .arguments = stop_listening_for_signal_args,
        .n_arguments = sizeof(stop_listening_for_signal_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_stop_listening_for_signal }
};

enum signal_index {
    SIGNAL_NEW_CARD,
    SIGNAL_CARD_REMOVED,
    SIGNAL_NEW_SINK,
    SIGNAL_SINK_REMOVED,
    SIGNAL_FALLBACK_SINK_UPDATED,
    SIGNAL_FALLBACK_SINK_UNSET,
    SIGNAL_NEW_SOURCE,
    SIGNAL_SOURCE_REMOVED,
    SIGNAL_FALLBACK_SOURCE_UPDATED,
    SIGNAL_FALLBACK_SOURCE_UNSET,
    SIGNAL_NEW_PLAYBACK_STREAM,
    SIGNAL_PLAYBACK_STREAM_REMOVED,
    SIGNAL_NEW_RECORD_STREAM,
    SIGNAL_RECORD_STREAM_REMOVED,
    SIGNAL_NEW_SAMPLE,
    SIGNAL_SAMPLE_REMOVED,
    SIGNAL_NEW_MODULE,
    SIGNAL_MODULE_REMOVED,
    SIGNAL_NEW_CLIENT,
    SIGNAL_CLIENT_REMOVED,
    SIGNAL_NEW_EXTENSION,
    SIGNAL_EXTENSION_REMOVED,
    SIGNAL_MAX
};

static pa_dbus_arg_info new_card_args[] =                { { "card",            "o", NULL } };
static pa_dbus_arg_info card_removed_args[] =            { { "card",            "o", NULL } };
static pa_dbus_arg_info new_sink_args[] =                { { "sink",            "o", NULL } };
static pa_dbus_arg_info sink_removed_args[] =            { { "sink",            "o", NULL } };
static pa_dbus_arg_info fallback_sink_updated_args[] =   { { "sink",            "o", NULL } };
static pa_dbus_arg_info new_source_args[] =              { { "source",          "o", NULL } };
static pa_dbus_arg_info source_removed_args[] =          { { "source",          "o", NULL } };
static pa_dbus_arg_info fallback_source_updated_args[] = { { "source",          "o", NULL } };
static pa_dbus_arg_info new_playback_stream_args[] =     { { "playback_stream", "o", NULL } };
static pa_dbus_arg_info playback_stream_removed_args[] = { { "playback_stream", "o", NULL } };
static pa_dbus_arg_info new_record_stream_args[] =       { { "record_stream",   "o", NULL } };
static pa_dbus_arg_info record_stream_removed_args[] =   { { "record_stream",   "o", NULL } };
static pa_dbus_arg_info new_sample_args[] =              { { "sample",          "o", NULL } };
static pa_dbus_arg_info sample_removed_args[] =          { { "sample",          "o", NULL } };
static pa_dbus_arg_info new_module_args[] =              { { "module",          "o", NULL } };
static pa_dbus_arg_info module_removed_args[] =          { { "module",          "o", NULL } };
static pa_dbus_arg_info new_client_args[] =              { { "client",          "o", NULL } };
static pa_dbus_arg_info client_removed_args[] =          { { "client",          "o", NULL } };
static pa_dbus_arg_info new_extension_args[] =           { { "extension",       "s", NULL } };
static pa_dbus_arg_info extension_removed_args[] =       { { "extension",       "s", NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_NEW_CARD]                = { .name = "NewCard",               .arguments = new_card_args,                .n_arguments = 1 },
    [SIGNAL_CARD_REMOVED]            = { .name = "CardRemoved",           .arguments = card_removed_args,            .n_arguments = 1 },
    [SIGNAL_NEW_SINK]                = { .name = "NewSink",               .arguments = new_sink_args,                .n_arguments = 1 },
    [SIGNAL_SINK_REMOVED]            = { .name = "SinkRemoved",           .arguments = sink_removed_args,            .n_arguments = 1 },
    [SIGNAL_FALLBACK_SINK_UPDATED]   = { .name = "FallbackSinkUpdated",   .arguments = fallback_sink_updated_args,   .n_arguments = 1 },
    [SIGNAL_FALLBACK_SINK_UNSET]     = { .name = "FallbackSinkUnset",     .arguments = NULL,                         .n_arguments = 0 },
    [SIGNAL_NEW_SOURCE]              = { .name = "NewSource",             .arguments = new_source_args,              .n_arguments = 1 },
    [SIGNAL_SOURCE_REMOVED]          = { .name = "SourceRemoved",         .arguments = source_removed_args,          .n_arguments = 1 },
    [SIGNAL_FALLBACK_SOURCE_UPDATED] = { .name = "FallbackSourceUpdated", .arguments = fallback_source_updated_args, .n_arguments = 1 },
    [SIGNAL_FALLBACK_SOURCE_UNSET]   = { .name = "FallbackSourceUnset",   .arguments = NULL,                         .n_arguments = 0 },
    [SIGNAL_NEW_PLAYBACK_STREAM]     = { .name = "NewPlaybackStream",     .arguments = new_playback_stream_args,     .n_arguments = 1 },
    [SIGNAL_PLAYBACK_STREAM_REMOVED] = { .name = "PlaybackStreamRemoved", .arguments = playback_stream_removed_args, .n_arguments = 1 },
    [SIGNAL_NEW_RECORD_STREAM]       = { .name = "NewRecordStream",       .arguments = new_record_stream_args,       .n_arguments = 1 },
    [SIGNAL_RECORD_STREAM_REMOVED]   = { .name = "RecordStreamRemoved",   .arguments = record_stream_removed_args,   .n_arguments = 1 },
    [SIGNAL_NEW_SAMPLE]              = { .name = "NewSample",             .arguments = new_sample_args,              .n_arguments = 1 },
    [SIGNAL_SAMPLE_REMOVED]          = { .name = "SampleRemoved",         .arguments = sample_removed_args,          .n_arguments = 1 },
    [SIGNAL_NEW_MODULE]              = { .name = "NewModule",             .arguments = new_module_args,              .n_arguments = 1 },
    [SIGNAL_MODULE_REMOVED]          = { .name = "ModuleRemoved",         .arguments = module_removed_args,          .n_arguments = 1 },
    [SIGNAL_NEW_CLIENT]              = { .name = "NewClient",             .arguments = new_client_args,              .n_arguments = 1 },
    [SIGNAL_CLIENT_REMOVED]          = { .name = "ClientRemoved",         .arguments = client_removed_args,          .n_arguments = 1 },
    [SIGNAL_NEW_EXTENSION]           = { .name = "NewExtension",          .arguments = new_extension_args,           .n_arguments = 1 },
    [SIGNAL_EXTENSION_REMOVED]       = { .name = "ExtensionRemoved",      .arguments = extension_removed_args,       .n_arguments = 1 }
};

static pa_dbus_interface_info core_interface_info = {
    .name = PA_DBUS_CORE_INTERFACE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static void handle_get_interface_revision(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    dbus_uint32_t interface_revision = INTERFACE_REVISION;

    pa_assert(conn);
    pa_assert(msg);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &interface_revision);
}

static void handle_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    const char *server_name = PACKAGE_NAME;

    pa_assert(conn);
    pa_assert(msg);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &server_name);
}

static void handle_get_version(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    const char *version = PACKAGE_VERSION;

    pa_assert(conn);
    pa_assert(msg);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &version);
}

static dbus_bool_t get_is_local(DBusConnection *conn) {
    int conn_fd;

    pa_assert(conn);

    if (!dbus_connection_get_socket(conn, &conn_fd))
        return FALSE;

    return pa_socket_is_local(conn_fd);
}

static void handle_get_is_local(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    dbus_bool_t is_local;

    pa_assert(conn);
    pa_assert(msg);

    is_local = get_is_local(conn);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &is_local);
}

static void handle_get_username(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    char *username = NULL;

    pa_assert(conn);
    pa_assert(msg);

    username = pa_get_user_name_malloc();

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &username);

    pa_xfree(username);
}

static void handle_get_hostname(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    char *hostname = NULL;

    pa_assert(conn);
    pa_assert(msg);

    hostname = pa_get_host_name_malloc();

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &hostname);

    pa_xfree(hostname);
}

/* Caller frees the returned array. */
static dbus_uint32_t *get_default_channels(pa_dbusiface_core *c, unsigned *n) {
    dbus_uint32_t *default_channels = NULL;
    unsigned i;

    pa_assert(c);
    pa_assert(n);

    *n = c->core->default_channel_map.channels;
    default_channels = pa_xnew(dbus_uint32_t, *n);

    for (i = 0; i < *n; ++i)
        default_channels[i] = c->core->default_channel_map.map[i];

    return default_channels;
}

static void handle_get_default_channels(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t *default_channels = NULL;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    default_channels = get_default_channels(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_UINT32, default_channels, n);

    pa_xfree(default_channels);
}

static void handle_set_default_channels(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    DBusMessageIter array_iter;
    pa_channel_map new_channel_map;
    const dbus_uint32_t *default_channels;
    int n_channels;
    unsigned i;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    pa_channel_map_init(&new_channel_map);

    dbus_message_iter_recurse(iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &default_channels, &n_channels);

    if (n_channels <= 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Empty channel array.");
        return;
    }

    if (n_channels > (int) PA_CHANNELS_MAX) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "Too many channels: %i. The maximum number of channels is %u.", n_channels, PA_CHANNELS_MAX);
        return;
    }

    new_channel_map.channels = n_channels;

    for (i = 0; i < new_channel_map.channels; ++i) {
        if (default_channels[i] >= PA_CHANNEL_POSITION_MAX) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid channel position: %u.", default_channels[i]);
            return;
        }

        new_channel_map.map[i] = default_channels[i];
    }

    c->core->default_channel_map = new_channel_map;
    c->core->default_sample_spec.channels = n_channels;

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_default_sample_format(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t default_sample_format;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    default_sample_format = c->core->default_sample_spec.format;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &default_sample_format);
}

static void handle_set_default_sample_format(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t default_sample_format;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    dbus_message_iter_get_basic(iter, &default_sample_format);

    if (!pa_sample_format_valid(default_sample_format)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid sample format.");
        return;
    }

    c->core->default_sample_spec.format = default_sample_format;

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_default_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t default_sample_rate;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    default_sample_rate = c->core->default_sample_spec.rate;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &default_sample_rate);
}

static void handle_set_default_sample_rate(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t default_sample_rate;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    dbus_message_iter_get_basic(iter, &default_sample_rate);

    if (!pa_sample_rate_valid(default_sample_rate)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid sample rate.");
        return;
    }

    c->core->default_sample_spec.rate = default_sample_rate;

    pa_dbus_send_empty_reply(conn, msg);
}

static void handle_get_alternate_sample_rate(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t alternate_sample_rate;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    alternate_sample_rate = c->core->alternate_sample_rate;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &alternate_sample_rate);
}

static void handle_set_alternate_sample_rate(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    dbus_uint32_t alternate_sample_rate;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    dbus_message_iter_get_basic(iter, &alternate_sample_rate);

    if (!pa_sample_rate_valid(alternate_sample_rate)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid sample rate.");
        return;
    }

    c->core->alternate_sample_rate = alternate_sample_rate;

    pa_dbus_send_empty_reply(conn, msg);
}

/* The caller frees the array, but not the strings. */
static const char **get_cards(pa_dbusiface_core *c, unsigned *n) {
    const char **cards;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_card *card;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->cards);

    if (*n == 0)
        return NULL;

    cards = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(card, c->cards, state)
        cards[i++] = pa_dbusiface_card_get_path(card);

    return cards;
}

static void handle_get_cards(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **cards;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    cards = get_cards(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, cards, n);

    pa_xfree(cards);
}

/* The caller frees the array, but not the strings. */
static const char **get_sinks(pa_dbusiface_core *c, unsigned *n) {
    const char **sinks;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_device *sink;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->sinks_by_index);

    if (*n == 0)
        return NULL;

    sinks = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(sink, c->sinks_by_index, state)
        sinks[i++] = pa_dbusiface_device_get_path(sink);

    return sinks;
}

static void handle_get_sinks(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **sinks;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    sinks = get_sinks(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, sinks, n);

    pa_xfree(sinks);
}

static void handle_get_fallback_sink(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    pa_dbusiface_device *fallback_sink;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    if (!c->fallback_sink) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "There are no sinks, and therefore no fallback sink either.");
        return;
    }

    pa_assert_se((fallback_sink = pa_hashmap_get(c->sinks_by_index, PA_UINT32_TO_PTR(c->fallback_sink->index))));
    object_path = pa_dbusiface_device_get_path(fallback_sink);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_set_fallback_sink(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    pa_dbusiface_device *fallback_sink;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    if (!c->fallback_sink) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "There are no sinks, and therefore no fallback sink either.");
        return;
    }

    dbus_message_iter_get_basic(iter, &object_path);

    if (!(fallback_sink = pa_hashmap_get(c->sinks_by_path, object_path))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such sink.", object_path);
        return;
    }

    pa_namereg_set_default_sink(c->core, pa_dbusiface_device_get_sink(fallback_sink));

    pa_dbus_send_empty_reply(conn, msg);
}

/* The caller frees the array, but not the strings. */
static const char **get_sources(pa_dbusiface_core *c, unsigned *n) {
    const char **sources;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_device *source;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->sources_by_index);

    if (*n == 0)
        return NULL;

    sources = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(source, c->sources_by_index, state)
        sources[i++] = pa_dbusiface_device_get_path(source);

    return sources;
}

static void handle_get_sources(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **sources;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    sources = get_sources(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, sources, n);

    pa_xfree(sources);
}

static void handle_get_fallback_source(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    pa_dbusiface_device *fallback_source;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    if (!c->fallback_source) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "There are no sources, and therefore no fallback source either.");
        return;
    }

    pa_assert_se((fallback_source = pa_hashmap_get(c->sources_by_index, PA_UINT32_TO_PTR(c->fallback_source->index))));
    object_path = pa_dbusiface_device_get_path(fallback_source);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_set_fallback_source(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    pa_dbusiface_core *c = userdata;
    pa_dbusiface_device *fallback_source;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(c);

    if (!c->fallback_source) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NO_SUCH_PROPERTY,
                           "There are no sources, and therefore no fallback source either.");
        return;
    }

    dbus_message_iter_get_basic(iter, &object_path);

    if (!(fallback_source = pa_hashmap_get(c->sources_by_path, object_path))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such source.", object_path);
        return;
    }

    pa_namereg_set_default_source(c->core, pa_dbusiface_device_get_source(fallback_source));

    pa_dbus_send_empty_reply(conn, msg);
}

/* The caller frees the array, but not the strings. */
static const char **get_playback_streams(pa_dbusiface_core *c, unsigned *n) {
    const char **streams;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_stream *stream;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->playback_streams);

    if (*n == 0)
        return NULL;

    streams = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(stream, c->playback_streams, state)
        streams[i++] = pa_dbusiface_stream_get_path(stream);

    return streams;
}

static void handle_get_playback_streams(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **playback_streams;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    playback_streams = get_playback_streams(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, playback_streams, n);

    pa_xfree(playback_streams);
}

/* The caller frees the array, but not the strings. */
static const char **get_record_streams(pa_dbusiface_core *c, unsigned *n) {
    const char **streams;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_stream *stream;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->record_streams);

    if (*n == 0)
        return NULL;

    streams = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(stream, c->record_streams, state)
        streams[i++] = pa_dbusiface_stream_get_path(stream);

    return streams;
}

static void handle_get_record_streams(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **record_streams;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    record_streams = get_record_streams(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, record_streams, n);

    pa_xfree(record_streams);
}

/* The caller frees the array, but not the strings. */
static const char **get_samples(pa_dbusiface_core *c, unsigned *n) {
    const char **samples;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_sample *sample;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->samples);

    if (*n == 0)
        return NULL;

    samples = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(sample, c->samples, state)
        samples[i++] = pa_dbusiface_sample_get_path(sample);

    return samples;
}

static void handle_get_samples(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **samples;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    samples = get_samples(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, samples, n);

    pa_xfree(samples);
}

/* The caller frees the array, but not the strings. */
static const char **get_modules(pa_dbusiface_core *c, unsigned *n) {
    const char **modules;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_module *module;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->modules);

    if (*n == 0)
        return NULL;

    modules = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(module, c->modules, state)
        modules[i++] = pa_dbusiface_module_get_path(module);

    return modules;
}

static void handle_get_modules(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **modules;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    modules = get_modules(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, modules, n);

    pa_xfree(modules);
}

/* The caller frees the array, but not the strings. */
static const char **get_clients(pa_dbusiface_core *c, unsigned *n) {
    const char **clients;
    unsigned i = 0;
    void *state = NULL;
    pa_dbusiface_client *client;

    pa_assert(c);
    pa_assert(n);

    *n = pa_hashmap_size(c->clients);

    if (*n == 0)
        return NULL;

    clients = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(client, c->clients, state)
        clients[i++] = pa_dbusiface_client_get_path(client);

    return clients;
}

static void handle_get_clients(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **clients;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    clients = get_clients(c, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, clients, n);

    pa_xfree(clients);
}

static const char *get_my_client(pa_dbusiface_core *c, DBusConnection *conn) {
    pa_client *my_client;

    pa_assert(c);
    pa_assert(conn);

    pa_assert_se((my_client = pa_dbus_protocol_get_client(c->dbus_protocol, conn)));

    return pa_dbusiface_client_get_path(pa_hashmap_get(c->clients, PA_UINT32_TO_PTR(my_client->index)));
}

static void handle_get_my_client(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char *my_client;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    my_client = get_my_client(c, conn);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &my_client);
}

static void handle_get_extensions(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char **extensions;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    extensions = pa_dbus_protocol_get_extensions(c->dbus_protocol, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_STRING, extensions, n);

    pa_xfree(extensions);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t interface_revision;
    const char *server_name;
    const char *version;
    dbus_bool_t is_local;
    char *username;
    char *hostname;
    dbus_uint32_t *default_channels;
    unsigned n_default_channels;
    dbus_uint32_t default_sample_format;
    dbus_uint32_t default_sample_rate;
    dbus_uint32_t alternate_sample_rate;
    const char **cards;
    unsigned n_cards;
    const char **sinks;
    unsigned n_sinks;
    const char *fallback_sink;
    const char **sources;
    unsigned n_sources;
    const char *fallback_source;
    const char **playback_streams;
    unsigned n_playback_streams;
    const char **record_streams;
    unsigned n_record_streams;
    const char **samples;
    unsigned n_samples;
    const char **modules;
    unsigned n_modules;
    const char **clients;
    unsigned n_clients;
    const char *my_client;
    const char **extensions;
    unsigned n_extensions;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    interface_revision = INTERFACE_REVISION;
    server_name = PACKAGE_NAME;
    version = PACKAGE_VERSION;
    is_local = get_is_local(conn);
    username = pa_get_user_name_malloc();
    hostname = pa_get_host_name_malloc();
    default_channels = get_default_channels(c, &n_default_channels);
    default_sample_format = c->core->default_sample_spec.format;
    default_sample_rate = c->core->default_sample_spec.rate;
    alternate_sample_rate = c->core->alternate_sample_rate;
    cards = get_cards(c, &n_cards);
    sinks = get_sinks(c, &n_sinks);
    fallback_sink = c->fallback_sink
                    ? pa_dbusiface_device_get_path(pa_hashmap_get(c->sinks_by_index, PA_UINT32_TO_PTR(c->fallback_sink->index)))
                    : NULL;
    sources = get_sources(c, &n_sources);
    fallback_source = c->fallback_source
                      ? pa_dbusiface_device_get_path(pa_hashmap_get(c->sources_by_index,
                                                                    PA_UINT32_TO_PTR(c->fallback_source->index)))
                      : NULL;
    playback_streams = get_playback_streams(c, &n_playback_streams);
    record_streams = get_record_streams(c, &n_record_streams);
    samples = get_samples(c, &n_samples);
    modules = get_modules(c, &n_modules);
    clients = get_clients(c, &n_clients);
    my_client = get_my_client(c, conn);
    extensions = pa_dbus_protocol_get_extensions(c->dbus_protocol, &n_extensions);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INTERFACE_REVISION].property_name, DBUS_TYPE_UINT32, &interface_revision);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &server_name);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_VERSION].property_name, DBUS_TYPE_STRING, &version);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_IS_LOCAL].property_name, DBUS_TYPE_BOOLEAN, &is_local);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_USERNAME].property_name, DBUS_TYPE_STRING, &username);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_HOSTNAME].property_name, DBUS_TYPE_STRING, &hostname);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEFAULT_CHANNELS].property_name, DBUS_TYPE_UINT32, default_channels, n_default_channels);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEFAULT_SAMPLE_FORMAT].property_name, DBUS_TYPE_UINT32, &default_sample_format);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_DEFAULT_SAMPLE_RATE].property_name, DBUS_TYPE_UINT32, &default_sample_rate);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ALTERNATE_SAMPLE_RATE].property_name, DBUS_TYPE_UINT32, &alternate_sample_rate);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CARDS].property_name, DBUS_TYPE_OBJECT_PATH, cards, n_cards);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SINKS].property_name, DBUS_TYPE_OBJECT_PATH, sinks, n_sinks);

    if (fallback_sink)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_FALLBACK_SINK].property_name, DBUS_TYPE_OBJECT_PATH, &fallback_sink);

    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SOURCES].property_name, DBUS_TYPE_OBJECT_PATH, sources, n_sources);

    if (fallback_source)
        pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_FALLBACK_SOURCE].property_name, DBUS_TYPE_OBJECT_PATH, &fallback_source);

    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_PLAYBACK_STREAMS].property_name, DBUS_TYPE_OBJECT_PATH, playback_streams, n_playback_streams);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_RECORD_STREAMS].property_name, DBUS_TYPE_OBJECT_PATH, record_streams, n_record_streams);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_SAMPLES].property_name, DBUS_TYPE_OBJECT_PATH, samples, n_samples);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_MODULES].property_name, DBUS_TYPE_OBJECT_PATH, modules, n_modules);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_CLIENTS].property_name, DBUS_TYPE_OBJECT_PATH, clients, n_clients);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_MY_CLIENT].property_name, DBUS_TYPE_OBJECT_PATH, &my_client);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_EXTENSIONS].property_name, DBUS_TYPE_STRING, extensions, n_extensions);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);

    pa_xfree(username);
    pa_xfree(hostname);
    pa_xfree(default_channels);
    pa_xfree(cards);
    pa_xfree(sinks);
    pa_xfree(sources);
    pa_xfree(playback_streams);
    pa_xfree(record_streams);
    pa_xfree(samples);
    pa_xfree(modules);
    pa_xfree(clients);
    pa_xfree(extensions);
}

static void handle_get_card_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    char *card_name;
    pa_card *card;
    pa_dbusiface_card *dbus_card;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &card_name, DBUS_TYPE_INVALID));

    if (!(card = pa_namereg_get(c->core, card_name, PA_NAMEREG_CARD))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "No such card.");
        return;
    }

    pa_assert_se((dbus_card = pa_hashmap_get(c->cards, PA_UINT32_TO_PTR(card->index))));

    object_path = pa_dbusiface_card_get_path(dbus_card);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_sink_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    char *sink_name;
    pa_sink *sink;
    pa_dbusiface_device *dbus_sink;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &sink_name, DBUS_TYPE_INVALID));

    if (!(sink = pa_namereg_get(c->core, sink_name, PA_NAMEREG_SINK))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such sink.", sink_name);
        return;
    }

    pa_assert_se((dbus_sink = pa_hashmap_get(c->sinks_by_index, PA_UINT32_TO_PTR(sink->index))));

    object_path = pa_dbusiface_device_get_path(dbus_sink);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_source_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    char *source_name;
    pa_source *source;
    pa_dbusiface_device *dbus_source;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &source_name, DBUS_TYPE_INVALID));

    if (!(source = pa_namereg_get(c->core, source_name, PA_NAMEREG_SOURCE))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "%s: No such source.", source_name);
        return;
    }

    pa_assert_se((dbus_source = pa_hashmap_get(c->sources_by_index, PA_UINT32_TO_PTR(source->index))));

    object_path = pa_dbusiface_device_get_path(dbus_source);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_get_sample_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    char *sample_name;
    pa_scache_entry *sample;
    pa_dbusiface_sample *dbus_sample;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &sample_name, DBUS_TYPE_INVALID));

    if (!(sample = pa_namereg_get(c->core, sample_name, PA_NAMEREG_SAMPLE))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "No such sample.");
        return;
    }

    pa_assert_se((dbus_sample = pa_hashmap_get(c->samples, PA_UINT32_TO_PTR(sample->index))));

    object_path = pa_dbusiface_sample_get_path(dbus_sample);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);
}

static void handle_upload_sample(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    DBusMessageIter msg_iter;
    DBusMessageIter array_iter;
    const char *name;
    dbus_uint32_t sample_format;
    dbus_uint32_t sample_rate;
    const dbus_uint32_t *channels;
    int n_channels;
    const dbus_uint32_t *default_volume;
    int n_volume_entries;
    pa_proplist *property_list;
    const uint8_t *data;
    int data_length;
    int i;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_memchunk chunk;
    uint32_t idx;
    pa_dbusiface_sample *dbus_sample = NULL;
    pa_scache_entry *sample = NULL;
    const char *object_path;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    chunk.memblock = NULL;

    pa_assert_se(dbus_message_iter_init(msg, &msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &name);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &sample_format);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &sample_rate);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_recurse(&msg_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &channels, &n_channels);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_recurse(&msg_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &default_volume, &n_volume_entries);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    if (!(property_list = pa_dbus_get_proplist_arg(conn, msg, &msg_iter)))
        return;

    dbus_message_iter_recurse(&msg_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &data, &data_length);

    if (!pa_sample_format_valid(sample_format)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid sample format.");
        goto finish;
    }

    if (!pa_sample_rate_valid(sample_rate)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid sample rate.");
        goto finish;
    }

    if (n_channels <= 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Empty channel map.");
        goto finish;
    }

    if (n_channels > (int) PA_CHANNELS_MAX) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "Too many channels: %i. The maximum is %u.", n_channels, PA_CHANNELS_MAX);
        goto finish;
    }

    for (i = 0; i < n_channels; ++i) {
        if (channels[i] >= PA_CHANNEL_POSITION_MAX) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid channel position.");
            goto finish;
        }
    }

    if (n_volume_entries != 0 && n_volume_entries != n_channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "The channels and default_volume arguments have different number of elements (%i and %i, resp).",
                           n_channels, n_volume_entries);
        goto finish;
    }

    for (i = 0; i < n_volume_entries; ++i) {
        if (!PA_VOLUME_IS_VALID(default_volume[i])) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid volume: %u.", default_volume[i]);
            goto finish;
        }
    }

    if (data_length == 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Empty data.");
        goto finish;
    }

    if (data_length > PA_SCACHE_ENTRY_SIZE_MAX) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "Too big sample: %i bytes. The maximum sample length is %u bytes.",
                           data_length, PA_SCACHE_ENTRY_SIZE_MAX);
        goto finish;
    }

    ss.format = sample_format;
    ss.rate = sample_rate;
    ss.channels = n_channels;

    pa_assert(pa_sample_spec_valid(&ss));

    if (!pa_frame_aligned(data_length, &ss)) {
        char buf[PA_SAMPLE_SPEC_SNPRINT_MAX];
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
                           "The sample length (%i bytes) doesn't align with the sample format and channels (%s).",
                           data_length, pa_sample_spec_snprint(buf, sizeof(buf), &ss));
        goto finish;
    }

    map.channels = n_channels;
    for (i = 0; i < n_channels; ++i)
        map.map[i] = channels[i];

    chunk.memblock = pa_memblock_new(c->core->mempool, data_length);
    chunk.index = 0;
    chunk.length = data_length;

    memcpy(pa_memblock_acquire(chunk.memblock), data, data_length);
    pa_memblock_release(chunk.memblock);

    if (pa_scache_add_item(c->core, name, &ss, &map, &chunk, property_list, &idx) < 0) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Adding the sample failed.");
        goto finish;
    }

    pa_assert_se(sample = pa_idxset_get_by_index(c->core->scache, idx));

    if (n_volume_entries > 0) {
        sample->volume.channels = n_channels;
        for (i = 0; i < n_volume_entries; ++i)
            sample->volume.values[i] = default_volume[i];
        sample->volume_is_set = true;
    } else {
        sample->volume_is_set = false;
    }

    dbus_sample = pa_dbusiface_sample_new(c, sample);
    pa_hashmap_put(c->samples, PA_UINT32_TO_PTR(idx), dbus_sample);

    object_path = pa_dbusiface_sample_get_path(dbus_sample);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);

finish:
    if (property_list)
        pa_proplist_free(property_list);

    if (chunk.memblock)
        pa_memblock_unref(chunk.memblock);
}

static bool contains_space(const char *string) {
    const char *p;

    pa_assert(string);

    for (p = string; *p; ++p) {
        if (isspace((unsigned char)*p))
            return true;
    }

    return false;
}

static void handle_load_module(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    char *name = NULL;
    const char *key = NULL;
    const char *value = NULL;
    char *escaped_value = NULL;
    pa_strbuf *arg_buffer = NULL;
    char *arg_string = NULL;
    pa_module *module = NULL;
    pa_dbusiface_module *dbus_module = NULL;
    const char *object_path = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    if (c->core->disallow_module_loading) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_ACCESS_DENIED, "The server is configured to disallow module loading.");
        return;
    }

    pa_assert_se(dbus_message_iter_init(msg, &msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &name);

    arg_buffer = pa_strbuf_new();

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_recurse(&msg_iter, &dict_iter);

    while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
        if (!pa_strbuf_isempty(arg_buffer))
            pa_strbuf_putc(arg_buffer, ' ');

        dbus_message_iter_recurse(&dict_iter, &dict_entry_iter);

        dbus_message_iter_get_basic(&dict_entry_iter, &key);

        if (strlen(key) <= 0 || !pa_ascii_valid(key) || contains_space(key)) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid module argument name: %s", key);
            goto finish;
        }

        pa_assert_se(dbus_message_iter_next(&dict_entry_iter));
        dbus_message_iter_get_basic(&dict_entry_iter, &value);

        escaped_value = pa_escape(value, "\"");
        pa_strbuf_printf(arg_buffer, "%s=\"%s\"", key, escaped_value);
        pa_xfree(escaped_value);

        dbus_message_iter_next(&dict_iter);
    }

    arg_string = pa_strbuf_to_string(arg_buffer);

    if (!(module = pa_module_load(c->core, name, arg_string))) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Failed to load module.");
        goto finish;
    }

    dbus_module = pa_dbusiface_module_new(module);
    pa_hashmap_put(c->modules, PA_UINT32_TO_PTR(module->index), dbus_module);

    object_path = pa_dbusiface_module_get_path(dbus_module);

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &object_path);

finish:
    if (arg_buffer)
        pa_strbuf_free(arg_buffer);

    pa_xfree(arg_string);
}

static void handle_exit(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    if (c->core->disallow_exit) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_ACCESS_DENIED, "The server is configured to disallow exiting.");
        return;
    }

    pa_dbus_send_empty_reply(conn, msg);

    pa_core_exit(c->core, false, 0);
}

static void handle_listen_for_signal(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char *signal_str;
    char **objects = NULL;
    int n_objects;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL,
                                       DBUS_TYPE_STRING, &signal_str,
                                       DBUS_TYPE_ARRAY, DBUS_TYPE_OBJECT_PATH, &objects, &n_objects,
                                       DBUS_TYPE_INVALID));

    pa_dbus_protocol_add_signal_listener(c->dbus_protocol, conn, *signal_str ? signal_str : NULL, objects, n_objects);

    pa_dbus_send_empty_reply(conn, msg);

    dbus_free_string_array(objects);
}

static void handle_stop_listening_for_signal(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    pa_dbusiface_core *c = userdata;
    const char *signal_str;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &signal_str, DBUS_TYPE_INVALID));

    pa_dbus_protocol_remove_signal_listener(c->dbus_protocol, conn, *signal_str ? signal_str : NULL);

    pa_dbus_send_empty_reply(conn, msg);
}

static pa_hook_result_t module_new_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_module * module = call_data;
    pa_dbusiface_module *module_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(module);

    if (!(module_iface = pa_hashmap_get(c->modules, PA_UINT32_TO_PTR(module->index)))) {
        module_iface = pa_dbusiface_module_new(module);
        pa_assert_se(pa_hashmap_put(c->modules, PA_UINT32_TO_PTR(module->index), module_iface) >= 0);

        object_path = pa_dbusiface_module_get_path(module_iface);

        pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                           PA_DBUS_CORE_INTERFACE,
                                                           signals[SIGNAL_NEW_MODULE].name)));
        pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

        pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t module_removed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_module * module = call_data;
    pa_dbusiface_module *module_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(module);

    pa_assert_se((module_iface = pa_hashmap_remove(c->modules, PA_UINT32_TO_PTR(module->index))));

    object_path = pa_dbusiface_module_get_path(module_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_MODULE_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_module_free(module_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sample_cache_new_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_scache_entry *sample = call_data;
    pa_dbusiface_sample *sample_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(sample);

    sample_iface = pa_dbusiface_sample_new(c, sample);
    pa_assert_se(pa_hashmap_put(c->samples, PA_UINT32_TO_PTR(sample->index), sample_iface) >= 0);

    object_path = pa_dbusiface_sample_get_path(sample_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_SAMPLE].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sample_cache_removed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_scache_entry *sample = call_data;
    pa_dbusiface_sample *sample_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(sample);

    pa_assert_se((sample_iface = pa_hashmap_remove(c->samples, PA_UINT32_TO_PTR(sample->index))));

    object_path = pa_dbusiface_sample_get_path(sample_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_SAMPLE_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_sample_free(sample_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t default_sink_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_sink *new_fallback_sink = call_data;
    pa_dbusiface_device *device_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);

    if (c->fallback_sink != new_fallback_sink) {
        if (c->fallback_sink)
            pa_sink_unref(c->fallback_sink);
        c->fallback_sink = new_fallback_sink ? pa_sink_ref(new_fallback_sink) : NULL;

        if (c->fallback_sink) {
            pa_assert_se((device_iface = pa_hashmap_get(c->sinks_by_index, PA_UINT32_TO_PTR(c->fallback_sink->index))));
            object_path = pa_dbusiface_device_get_path(device_iface);

            pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                               PA_DBUS_CORE_INTERFACE,
                                                               signals[SIGNAL_FALLBACK_SINK_UPDATED].name)));
            pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

        } else {
            pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                               PA_DBUS_CORE_INTERFACE,
                                                               signals[SIGNAL_FALLBACK_SINK_UNSET].name)));
        }
    }

    if (signal_msg) {
        pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t default_source_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_source *new_fallback_source = call_data;
    pa_dbusiface_device *device_iface;
    const char *object_path;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);

    if (c->fallback_source != new_fallback_source) {
        if (c->fallback_source)
            pa_source_unref(c->fallback_source);
        c->fallback_source = new_fallback_source ? pa_source_ref(new_fallback_source) : NULL;

        if (c->fallback_source) {
            pa_assert_se((device_iface = pa_hashmap_get(c->sources_by_index, PA_UINT32_TO_PTR(c->fallback_source->index))));
            object_path = pa_dbusiface_device_get_path(device_iface);

            pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                               PA_DBUS_CORE_INTERFACE,
                                                               signals[SIGNAL_FALLBACK_SOURCE_UPDATED].name)));
            pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

        } else {
            pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                               PA_DBUS_CORE_INTERFACE,
                                                               signals[SIGNAL_FALLBACK_SOURCE_UNSET].name)));
        }
    }

    if (signal_msg) {
        pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
        dbus_message_unref(signal_msg);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t card_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_card *card = call_data;
    pa_dbusiface_card *card_iface = NULL;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(card);

    card_iface = pa_dbusiface_card_new(c, card);
    pa_assert_se(pa_hashmap_put(c->cards, PA_UINT32_TO_PTR(card->index), card_iface) >= 0);

    object_path = pa_dbusiface_card_get_path(card_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_CARD].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t card_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_card *card = call_data;
    pa_dbusiface_card *card_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(card);

    pa_assert_se((card_iface = pa_hashmap_remove(c->cards, PA_UINT32_TO_PTR(card->index))));

    object_path = pa_dbusiface_card_get_path(card_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_CARD_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_card_free(card_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_sink_input *sink_input = call_data;
    pa_dbusiface_stream *stream_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(sink_input);

    stream_iface = pa_dbusiface_stream_new_playback(c, sink_input);
    pa_assert_se(pa_hashmap_put(c->playback_streams, PA_UINT32_TO_PTR(sink_input->index), stream_iface) >= 0);

    object_path = pa_dbusiface_stream_get_path(stream_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_PLAYBACK_STREAM].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_sink_input *sink_input = call_data;
    pa_dbusiface_stream *stream_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(sink_input);

    pa_assert_se((stream_iface = pa_hashmap_remove(c->playback_streams, PA_UINT32_TO_PTR(sink_input->index))));

    object_path = pa_dbusiface_stream_get_path(stream_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_PLAYBACK_STREAM_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_stream_free(stream_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_source_output *source_output = call_data;
    pa_dbusiface_stream *stream_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(source_output);

    stream_iface = pa_dbusiface_stream_new_record(c, source_output);
    pa_assert_se(pa_hashmap_put(c->record_streams, PA_UINT32_TO_PTR(source_output->index), stream_iface) >= 0);

    object_path = pa_dbusiface_stream_get_path(stream_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_RECORD_STREAM].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_source_output *source_output = call_data;
    pa_dbusiface_stream *stream_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(source_output);

    pa_assert_se((stream_iface = pa_hashmap_remove(c->record_streams, PA_UINT32_TO_PTR(source_output->index))));

    object_path = pa_dbusiface_stream_get_path(stream_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_RECORD_STREAM_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_stream_free(stream_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t client_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_client *client = call_data;
    pa_dbusiface_client *client_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(client);

    client_iface = pa_dbusiface_client_new(c, client);
    pa_assert_se(pa_hashmap_put(c->clients, PA_UINT32_TO_PTR(client->index), client_iface) >= 0);

    object_path = pa_dbusiface_client_get_path(client_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_CLIENT].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t client_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_client *client = call_data;
    pa_dbusiface_client *client_iface;
    const char *object_path;
    DBusMessage *signal_msg;

    pa_assert(c);
    pa_assert(client);

    pa_assert_se((client_iface = pa_hashmap_remove(c->clients, PA_UINT32_TO_PTR(client->index))));

    object_path = pa_dbusiface_client_get_path(client_iface);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_CLIENT_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbusiface_client_free(client_iface);

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_sink *s = call_data;
    pa_dbusiface_device *d = NULL;
    const char *object_path = NULL;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(s);

    d = pa_dbusiface_device_new_sink(c, s);
    object_path = pa_dbusiface_device_get_path(d);

    pa_assert_se(pa_hashmap_put(c->sinks_by_index, PA_UINT32_TO_PTR(s->index), d) >= 0);
    pa_assert_se(pa_hashmap_put(c->sinks_by_path, (char *) object_path, d) >= 0);

    pa_assert_se(signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                      PA_DBUS_CORE_INTERFACE,
                                                      signals[SIGNAL_NEW_SINK].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_sink *s = call_data;
    pa_dbusiface_device *d = NULL;
    const char *object_path = NULL;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(s);

    pa_assert_se(d = pa_hashmap_remove(c->sinks_by_index, PA_UINT32_TO_PTR(s->index)));
    object_path = pa_dbusiface_device_get_path(d);
    pa_assert_se(pa_hashmap_remove(c->sinks_by_path, object_path));

    pa_assert_se(signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                      PA_DBUS_CORE_INTERFACE,
                                                      signals[SIGNAL_SINK_REMOVED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    pa_dbusiface_device_free(d);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_put_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_source *s = call_data;
    pa_dbusiface_device *d = NULL;
    const char *object_path = NULL;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(s);

    d = pa_dbusiface_device_new_source(c, s);
    object_path = pa_dbusiface_device_get_path(d);

    pa_assert_se(pa_hashmap_put(c->sources_by_index, PA_UINT32_TO_PTR(s->index), d) >= 0);
    pa_assert_se(pa_hashmap_put(c->sources_by_path, (char *) object_path, d) >= 0);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_SOURCE].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_unlink_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    pa_source *s = call_data;
    pa_dbusiface_device *d = NULL;
    const char *object_path = NULL;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(s);

    pa_assert_se(d = pa_hashmap_remove(c->sources_by_index, PA_UINT32_TO_PTR(s->index)));
    object_path = pa_dbusiface_device_get_path(d);
    pa_assert_se(pa_hashmap_remove(c->sources_by_path, object_path));

    pa_assert_se(signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                      PA_DBUS_CORE_INTERFACE,
                                                      signals[SIGNAL_SOURCE_REMOVED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &object_path, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    pa_dbusiface_device_free(d);

    return PA_HOOK_OK;
}

static pa_hook_result_t extension_registered_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    const char *ext_name = call_data;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(ext_name);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_NEW_EXTENSION].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_STRING, &ext_name, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

static pa_hook_result_t extension_unregistered_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_dbusiface_core *c = slot_data;
    const char *ext_name = call_data;
    DBusMessage *signal_msg = NULL;

    pa_assert(c);
    pa_assert(ext_name);

    pa_assert_se((signal_msg = dbus_message_new_signal(PA_DBUS_CORE_OBJECT_PATH,
                                                       PA_DBUS_CORE_INTERFACE,
                                                       signals[SIGNAL_EXTENSION_REMOVED].name)));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_STRING, &ext_name, DBUS_TYPE_INVALID));

    pa_dbus_protocol_send_signal(c->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);

    return PA_HOOK_OK;
}

pa_dbusiface_core *pa_dbusiface_core_new(pa_core *core) {
    pa_dbusiface_core *c;
    pa_card *card;
    pa_sink *sink;
    pa_source *source;
    pa_dbusiface_device *device;
    pa_sink_input *sink_input;
    pa_source_output *source_output;
    pa_scache_entry *sample;
    pa_module *module;
    pa_client *client;
    uint32_t idx;

    pa_assert(core);

    c = pa_xnew(pa_dbusiface_core, 1);
    c->core = core;
    c->dbus_protocol = pa_dbus_protocol_get(core);
    c->cards = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL, (pa_free_cb_t) pa_dbusiface_card_free);
    c->sinks_by_index = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL,
                                            (pa_free_cb_t) pa_dbusiface_device_free);
    c->sinks_by_path = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    c->sources_by_index = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL,
                                              (pa_free_cb_t) pa_dbusiface_device_free);
    c->sources_by_path = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    c->playback_streams = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL,
                                              (pa_free_cb_t) pa_dbusiface_stream_free);
    c->record_streams = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL,
                                            (pa_free_cb_t) pa_dbusiface_stream_free);
    c->samples = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL, (pa_free_cb_t) pa_dbusiface_sample_free);
    c->modules = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL, (pa_free_cb_t) pa_dbusiface_module_free);
    c->clients = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL, (pa_free_cb_t) pa_dbusiface_client_free);
    c->fallback_sink = pa_namereg_get_default_sink(core);
    c->fallback_source = pa_namereg_get_default_source(core);
    c->default_sink_changed_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_DEFAULT_SINK_CHANGED],
                                                   PA_HOOK_NORMAL, default_sink_changed_cb, c);
    c->default_source_changed_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_DEFAULT_SOURCE_CHANGED],
                                                     PA_HOOK_NORMAL, default_source_changed_cb, c);
    c->module_new_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_MODULE_NEW],
                                         PA_HOOK_NORMAL, module_new_cb, c);
    c->module_removed_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_MODULE_UNLINK],
                                             PA_HOOK_NORMAL, module_removed_cb, c);
    c->sample_cache_new_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SAMPLE_CACHE_NEW],
                                               PA_HOOK_NORMAL, sample_cache_new_cb, c);
    c->sample_cache_removed_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SAMPLE_CACHE_UNLINK],
                                                   PA_HOOK_NORMAL, sample_cache_removed_cb, c);
    c->card_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_CARD_PUT],
                                       PA_HOOK_NORMAL, card_put_cb, c);
    c->card_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_CARD_UNLINK],
                                          PA_HOOK_NORMAL, card_unlink_cb, c);
    c->sink_input_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SINK_INPUT_PUT],
                                             PA_HOOK_NORMAL, sink_input_put_cb, c);
    c->sink_input_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SINK_INPUT_UNLINK],
                                                PA_HOOK_NORMAL, sink_input_unlink_cb, c);
    c->source_output_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_PUT],
                                                PA_HOOK_NORMAL, source_output_put_cb, c);
    c->source_output_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_UNLINK],
                                                   PA_HOOK_NORMAL, source_output_unlink_cb, c);
    c->client_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_CLIENT_PUT],
                                         PA_HOOK_NORMAL, client_put_cb, c);
    c->client_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_CLIENT_UNLINK],
                                            PA_HOOK_NORMAL, client_unlink_cb, c);
    c->sink_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_NORMAL, sink_put_cb, c);
    c->sink_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_NORMAL, sink_unlink_cb, c);
    c->source_put_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_NORMAL, source_put_cb, c);
    c->source_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_NORMAL, source_unlink_cb, c);
    c->extension_registered_slot = pa_dbus_protocol_hook_connect(c->dbus_protocol,
                                                                 PA_DBUS_PROTOCOL_HOOK_EXTENSION_REGISTERED,
                                                                 PA_HOOK_NORMAL,
                                                                 extension_registered_cb,
                                                                 c);
    c->extension_unregistered_slot = pa_dbus_protocol_hook_connect(c->dbus_protocol,
                                                                   PA_DBUS_PROTOCOL_HOOK_EXTENSION_UNREGISTERED,
                                                                   PA_HOOK_NORMAL,
                                                                   extension_unregistered_cb,
                                                                   c);
    c->memstats = pa_dbusiface_memstats_new(c, core);

    if (c->fallback_sink)
        pa_sink_ref(c->fallback_sink);
    if (c->fallback_source)
        pa_source_ref(c->fallback_source);

    PA_IDXSET_FOREACH(card, core->cards, idx)
        pa_hashmap_put(c->cards, PA_UINT32_TO_PTR(idx), pa_dbusiface_card_new(c, card));

    PA_IDXSET_FOREACH(sink, core->sinks, idx) {
        device = pa_dbusiface_device_new_sink(c, sink);
        pa_hashmap_put(c->sinks_by_index, PA_UINT32_TO_PTR(idx), device);
        pa_hashmap_put(c->sinks_by_path, (char *) pa_dbusiface_device_get_path(device), device);
    }

    PA_IDXSET_FOREACH(source, core->sources, idx) {
        device = pa_dbusiface_device_new_source(c, source);
        pa_hashmap_put(c->sources_by_index, PA_UINT32_TO_PTR(idx), device);
        pa_hashmap_put(c->sources_by_path, (char *) pa_dbusiface_device_get_path(device), device);
    }

    PA_IDXSET_FOREACH(sink_input, core->sink_inputs, idx)
        pa_hashmap_put(c->playback_streams, PA_UINT32_TO_PTR(idx), pa_dbusiface_stream_new_playback(c, sink_input));

    PA_IDXSET_FOREACH(source_output, core->source_outputs, idx)
        pa_hashmap_put(c->record_streams, PA_UINT32_TO_PTR(idx), pa_dbusiface_stream_new_record(c, source_output));

    PA_IDXSET_FOREACH(sample, core->scache, idx)
        pa_hashmap_put(c->samples, PA_UINT32_TO_PTR(idx), pa_dbusiface_sample_new(c, sample));

    PA_IDXSET_FOREACH(module, core->modules, idx)
        pa_hashmap_put(c->modules, PA_UINT32_TO_PTR(idx), pa_dbusiface_module_new(module));

    PA_IDXSET_FOREACH(client, core->clients, idx)
        pa_hashmap_put(c->clients, PA_UINT32_TO_PTR(idx), pa_dbusiface_client_new(c, client));

    pa_assert_se(pa_dbus_protocol_add_interface(c->dbus_protocol, PA_DBUS_CORE_OBJECT_PATH, &core_interface_info, c) >= 0);

    return c;
}

void pa_dbusiface_core_free(pa_dbusiface_core *c) {
    pa_assert(c);

    pa_assert_se(pa_dbus_protocol_remove_interface(c->dbus_protocol, PA_DBUS_CORE_OBJECT_PATH, core_interface_info.name) >= 0);

    /* Note that the order of freeing is important below.
     * Do not change it for the sake of tidiness without checking! */
    pa_hashmap_free(c->cards);
    pa_hashmap_free(c->sinks_by_path);
    pa_hashmap_free(c->sinks_by_index);
    pa_hashmap_free(c->sources_by_path);
    pa_hashmap_free(c->sources_by_index);
    pa_hashmap_free(c->playback_streams);
    pa_hashmap_free(c->record_streams);
    pa_hashmap_free(c->samples);
    pa_hashmap_free(c->modules);
    pa_hashmap_free(c->clients);
    pa_hook_slot_free(c->module_new_slot);
    pa_hook_slot_free(c->module_removed_slot);
    pa_hook_slot_free(c->default_sink_changed_slot);
    pa_hook_slot_free(c->default_source_changed_slot);
    pa_hook_slot_free(c->sample_cache_new_slot);
    pa_hook_slot_free(c->sample_cache_removed_slot);
    pa_hook_slot_free(c->card_put_slot);
    pa_hook_slot_free(c->card_unlink_slot);
    pa_hook_slot_free(c->sink_input_put_slot);
    pa_hook_slot_free(c->sink_input_unlink_slot);
    pa_hook_slot_free(c->source_output_put_slot);
    pa_hook_slot_free(c->source_output_unlink_slot);
    pa_hook_slot_free(c->client_put_slot);
    pa_hook_slot_free(c->client_unlink_slot);
    pa_hook_slot_free(c->sink_put_slot);
    pa_hook_slot_free(c->sink_unlink_slot);
    pa_hook_slot_free(c->source_put_slot);
    pa_hook_slot_free(c->source_unlink_slot);
    pa_hook_slot_free(c->extension_registered_slot);
    pa_hook_slot_free(c->extension_unregistered_slot);
    pa_dbusiface_memstats_free(c->memstats);

    if (c->fallback_sink)
        pa_sink_unref(c->fallback_sink);
    if (c->fallback_source)
        pa_source_unref(c->fallback_source);

    pa_dbus_protocol_unref(c->dbus_protocol);

    pa_xfree(c);
}

const char *pa_dbusiface_core_get_card_path(pa_dbusiface_core *c, const pa_card *card) {
    pa_assert(c);
    pa_assert(card);

    return pa_dbusiface_card_get_path(pa_hashmap_get(c->cards, PA_UINT32_TO_PTR(card->index)));
}

const char *pa_dbusiface_core_get_sink_path(pa_dbusiface_core *c, const pa_sink *sink) {
    pa_assert(c);
    pa_assert(sink);

    return pa_dbusiface_device_get_path(pa_hashmap_get(c->sinks_by_index, PA_UINT32_TO_PTR(sink->index)));
}

const char *pa_dbusiface_core_get_source_path(pa_dbusiface_core *c, const pa_source *source) {
    pa_assert(c);
    pa_assert(source);

    return pa_dbusiface_device_get_path(pa_hashmap_get(c->sources_by_index, PA_UINT32_TO_PTR(source->index)));
}

const char *pa_dbusiface_core_get_playback_stream_path(pa_dbusiface_core *c, const pa_sink_input *sink_input) {
    pa_assert(c);
    pa_assert(sink_input);

    return pa_dbusiface_stream_get_path(pa_hashmap_get(c->playback_streams, PA_UINT32_TO_PTR(sink_input->index)));
}

const char *pa_dbusiface_core_get_record_stream_path(pa_dbusiface_core *c, const pa_source_output *source_output) {
    pa_assert(c);
    pa_assert(source_output);

    return pa_dbusiface_stream_get_path(pa_hashmap_get(c->record_streams, PA_UINT32_TO_PTR(source_output->index)));
}

const char *pa_dbusiface_core_get_module_path(pa_dbusiface_core *c, const pa_module *module) {
    pa_assert(c);
    pa_assert(module);

    return pa_dbusiface_module_get_path(pa_hashmap_get(c->modules, PA_UINT32_TO_PTR(module->index)));
}

const char *pa_dbusiface_core_get_client_path(pa_dbusiface_core *c, const pa_client *client) {
    pa_assert(c);
    pa_assert(client);

    return pa_dbusiface_client_get_path(pa_hashmap_get(c->clients, PA_UINT32_TO_PTR(client->index)));
}

pa_sink *pa_dbusiface_core_get_sink(pa_dbusiface_core *c, const char *object_path) {
    pa_dbusiface_device *device = NULL;

    pa_assert(c);
    pa_assert(object_path);

    device = pa_hashmap_get(c->sinks_by_path, object_path);

    if (device)
        return pa_dbusiface_device_get_sink(device);
    else
        return NULL;
}

pa_source *pa_dbusiface_core_get_source(pa_dbusiface_core *c, const char *object_path) {
    pa_dbusiface_device *device = NULL;

    pa_assert(c);
    pa_assert(object_path);

    device = pa_hashmap_get(c->sources_by_path, object_path);

    if (device)
        return pa_dbusiface_device_get_source(device);
    else
        return NULL;
}
