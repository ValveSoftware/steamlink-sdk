/***
  This file is part of PulseAudio.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2011 Colin Guthrie

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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <pulse/gccmacro.h>
#include <pulse/xmalloc.h>
#include <pulse/volume.h>
#include <pulse/timeval.h>
#include <pulse/rtclock.h>
#include <pulse/format.h>
#include <pulse/internal.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/core-subscribe.h>
#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/namereg.h>
#include <pulsecore/protocol-native.h>
#include <pulsecore/pstream.h>
#include <pulsecore/pstream-util.h>
#include <pulsecore/database.h>
#include <pulsecore/tagstruct.h>

#include "module-device-restore-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Automatically restore the volume/mute state of devices");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "restore_port=<Save/restore port?> "
        "restore_volume=<Save/restore volumes?> "
        "restore_muted=<Save/restore muted states?> "
        "restore_formats=<Save/restore saved formats?>");

#define SAVE_INTERVAL (10 * PA_USEC_PER_SEC)

static const char* const valid_modargs[] = {
    "restore_volume",
    "restore_muted",
    "restore_port",
    "restore_formats",
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_subscription *subscription;
    pa_time_event *save_time_event;
    pa_database *database;

    pa_native_protocol *protocol;
    pa_idxset *subscribed;

    bool restore_volume:1;
    bool restore_muted:1;
    bool restore_port:1;
    bool restore_formats:1;
};

/* Protocol extension commands */
enum {
    SUBCOMMAND_TEST,
    SUBCOMMAND_SUBSCRIBE,
    SUBCOMMAND_EVENT,
    SUBCOMMAND_READ_FORMATS_ALL,
    SUBCOMMAND_READ_FORMATS,
    SUBCOMMAND_SAVE_FORMATS
};

#define ENTRY_VERSION 1

struct entry {
    uint8_t version;
    bool port_valid;
    char *port;
};

#define PERPORTENTRY_VERSION 1

struct perportentry {
    uint8_t version;
    bool muted_valid, volume_valid;
    bool muted;
    pa_channel_map channel_map;
    pa_cvolume volume;
    pa_idxset *formats;
};

static void save_time_callback(pa_mainloop_api*a, pa_time_event* e, const struct timeval *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(a);
    pa_assert(e);
    pa_assert(u);

    pa_assert(e == u->save_time_event);
    u->core->mainloop->time_free(u->save_time_event);
    u->save_time_event = NULL;

    pa_database_sync(u->database);
    pa_log_info("Synced.");
}

static void trigger_save(struct userdata *u, pa_device_type_t type, uint32_t sink_idx) {
    pa_native_connection *c;
    uint32_t idx;

    if (sink_idx != PA_INVALID_INDEX) {
        PA_IDXSET_FOREACH(c, u->subscribed, idx) {
            pa_tagstruct *t;

            t = pa_tagstruct_new();
            pa_tagstruct_putu32(t, PA_COMMAND_EXTENSION);
            pa_tagstruct_putu32(t, 0);
            pa_tagstruct_putu32(t, u->module->index);
            pa_tagstruct_puts(t, u->module->name);
            pa_tagstruct_putu32(t, SUBCOMMAND_EVENT);
            pa_tagstruct_putu32(t, type);
            pa_tagstruct_putu32(t, sink_idx);

            pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), t);
        }
    }

    if (u->save_time_event)
        return;

    u->save_time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + SAVE_INTERVAL, save_time_callback, u);
}

#ifdef ENABLE_LEGACY_DATABASE_ENTRY_FORMAT
/* Some forward declarations */
static bool legacy_entry_read(struct userdata *u, pa_datum *data, struct entry **entry, struct perportentry **perportentry);
static struct perportentry* perportentry_read(struct userdata *u, const char *basekeyname, const char *port);
static bool perportentry_write(struct userdata *u, const char *basekeyname, const char *port, const struct perportentry *e);
static void perportentry_free(struct perportentry* e);
#endif

static struct entry* entry_new(void) {
    struct entry *r = pa_xnew0(struct entry, 1);
    r->version = ENTRY_VERSION;
    return r;
}

static void entry_free(struct entry* e) {
    pa_assert(e);

    pa_xfree(e->port);
    pa_xfree(e);
}

static bool entry_write(struct userdata *u, const char *name, const struct entry *e) {
    pa_tagstruct *t;
    pa_datum key, data;
    bool r;

    pa_assert(u);
    pa_assert(name);
    pa_assert(e);

    t = pa_tagstruct_new();
    pa_tagstruct_putu8(t, e->version);
    pa_tagstruct_put_boolean(t, e->port_valid);
    pa_tagstruct_puts(t, e->port);

    key.data = (char *) name;
    key.size = strlen(name);

    data.data = (void*)pa_tagstruct_data(t, &data.size);

    r = (pa_database_set(u->database, &key, &data, true) == 0);

    pa_tagstruct_free(t);

    return r;
}

static struct entry* entry_read(struct userdata *u, const char *name) {
    pa_datum key, data;
    struct entry *e = NULL;
    pa_tagstruct *t = NULL;
    const char* port;

    pa_assert(u);
    pa_assert(name);

    key.data = (char*) name;
    key.size = strlen(name);

    pa_zero(data);

    if (!pa_database_get(u->database, &key, &data)) {
        pa_log_debug("Database contains no data for key: %s", name);
        return NULL;
    }

    t = pa_tagstruct_new_fixed(data.data, data.size);
    e = entry_new();

    if (pa_tagstruct_getu8(t, &e->version) < 0 ||
        e->version > ENTRY_VERSION ||
        pa_tagstruct_get_boolean(t, &e->port_valid) < 0 ||
        pa_tagstruct_gets(t, &port) < 0) {

        goto fail;
    }

    if (!pa_tagstruct_eof(t))
        goto fail;

    e->port = pa_xstrdup(port);

    pa_tagstruct_free(t);
    pa_datum_free(&data);

    return e;

fail:

    pa_log_debug("Database contains invalid data for key: %s (probably pre-v1.0 data)", name);

    if (e)
        entry_free(e);
    if (t)
        pa_tagstruct_free(t);

#ifdef ENABLE_LEGACY_DATABASE_ENTRY_FORMAT
{
    struct perportentry *ppe;
    pa_log_debug("Attempting to load legacy (pre-v1.0) data for key: %s", name);
    if (legacy_entry_read(u, &data, &e, &ppe)) {
        bool written = false;

        pa_log_debug("Success. Saving new format for key: %s", name);
        written = entry_write(u, name, e);

        /* Now convert the legacy entry into per-port entries */
        if (0 == strncmp("sink:", name, 5)) {
            pa_sink *sink;

            if ((sink = pa_namereg_get(u->core, name+5, PA_NAMEREG_SINK))) {
                /* Write a "null" port entry. The read code will automatically try this
                 * if it cannot find a specific port-named entry. */
                written = perportentry_write(u, name, NULL, ppe) || written;
            }
        } else if (0 == strncmp("source:", name, 7)) {
            pa_source *source;

            if ((source = pa_namereg_get(u->core, name+7, PA_NAMEREG_SOURCE))) {
                /* Write a "null" port entry. The read code will automatically try this
                 * if it cannot find a specific port-named entry. */
                written = perportentry_write(u, name, NULL, ppe) || written;
            }
        }
        perportentry_free(ppe);

        if (written)
            /* NB The device type doesn't matter when we pass in an invalid index. */
            trigger_save(u, PA_DEVICE_TYPE_SINK, PA_INVALID_INDEX);

        pa_datum_free(&data);
        return e;
    }
    pa_log_debug("Unable to load legacy (pre-v1.0) data for key: %s. Ignoring.", name);
}
#endif

    pa_datum_free(&data);
    return NULL;
}

static struct entry* entry_copy(const struct entry *e) {
    struct entry* r;

    pa_assert(e);
    r = entry_new();
    r->version = e->version;
    r->port_valid = e->port_valid;
    r->port = pa_xstrdup(e->port);

    return r;
}

static bool entries_equal(const struct entry *a, const struct entry *b) {

    pa_assert(a && b);

    if (a->port_valid != b->port_valid ||
        (a->port_valid && !pa_streq(a->port, b->port)))
        return false;

    return true;
}

static struct perportentry* perportentry_new(bool add_pcm_format) {
    struct perportentry *r = pa_xnew0(struct perportentry, 1);
    r->version = PERPORTENTRY_VERSION;
    r->formats = pa_idxset_new(NULL, NULL);
    if (add_pcm_format) {
        pa_format_info *f = pa_format_info_new();
        f->encoding = PA_ENCODING_PCM;
        pa_idxset_put(r->formats, f, NULL);
    }
    return r;
}

static void perportentry_free(struct perportentry* e) {
    pa_assert(e);

    pa_idxset_free(e->formats, (pa_free_cb_t) pa_format_info_free);
    pa_xfree(e);
}

static bool perportentry_write(struct userdata *u, const char *basekeyname, const char *port, const struct perportentry *e) {
    pa_tagstruct *t;
    pa_datum key, data;
    bool r;
    uint32_t i;
    pa_format_info *f;
    uint8_t n_formats;
    char *name;

    pa_assert(u);
    pa_assert(basekeyname);
    pa_assert(e);

    name = pa_sprintf_malloc("%s:%s", basekeyname, (port ? port : "null"));

    n_formats = pa_idxset_size(e->formats);
    pa_assert(n_formats > 0);

    t = pa_tagstruct_new();
    pa_tagstruct_putu8(t, e->version);
    pa_tagstruct_put_boolean(t, e->volume_valid);
    pa_tagstruct_put_channel_map(t, &e->channel_map);
    pa_tagstruct_put_cvolume(t, &e->volume);
    pa_tagstruct_put_boolean(t, e->muted_valid);
    pa_tagstruct_put_boolean(t, e->muted);
    pa_tagstruct_putu8(t, n_formats);

    PA_IDXSET_FOREACH(f, e->formats, i) {
        pa_tagstruct_put_format_info(t, f);
    }

    key.data = (char *) name;
    key.size = strlen(name);

    data.data = (void*)pa_tagstruct_data(t, &data.size);

    r = (pa_database_set(u->database, &key, &data, true) == 0);

    pa_tagstruct_free(t);
    pa_xfree(name);

    return r;
}

static struct perportentry* perportentry_read(struct userdata *u, const char *basekeyname, const char *port) {
    pa_datum key, data;
    struct perportentry *e = NULL;
    pa_tagstruct *t = NULL;
    uint8_t i, n_formats;
    char *name;

    pa_assert(u);
    pa_assert(basekeyname);

    name = pa_sprintf_malloc("%s:%s", basekeyname, (port ? port : "null"));

    key.data = name;
    key.size = strlen(name);

    pa_zero(data);

    if (!pa_database_get(u->database, &key, &data))
        goto fail;

    t = pa_tagstruct_new_fixed(data.data, data.size);
    e = perportentry_new(false);

    if (pa_tagstruct_getu8(t, &e->version) < 0 ||
        e->version > PERPORTENTRY_VERSION ||
        pa_tagstruct_get_boolean(t, &e->volume_valid) < 0 ||
        pa_tagstruct_get_channel_map(t, &e->channel_map) < 0 ||
        pa_tagstruct_get_cvolume(t, &e->volume) < 0 ||
        pa_tagstruct_get_boolean(t, &e->muted_valid) < 0 ||
        pa_tagstruct_get_boolean(t, &e->muted) < 0 ||
        pa_tagstruct_getu8(t, &n_formats) < 0 || n_formats < 1) {

        goto fail;
    }

    for (i = 0; i < n_formats; ++i) {
        pa_format_info *f = pa_format_info_new();
        if (pa_tagstruct_get_format_info(t, f) < 0) {
            pa_format_info_free(f);
            goto fail;
        }
        pa_idxset_put(e->formats, f, NULL);
    }

    if (!pa_tagstruct_eof(t))
        goto fail;

    if (e->volume_valid && !pa_channel_map_valid(&e->channel_map)) {
        pa_log_warn("Invalid channel map stored in database for device %s", name);
        goto fail;
    }

    if (e->volume_valid && (!pa_cvolume_valid(&e->volume) || !pa_cvolume_compatible_with_channel_map(&e->volume, &e->channel_map))) {
        pa_log_warn("Volume and channel map don't match in database entry for device %s", name);
        goto fail;
    }

    pa_tagstruct_free(t);
    pa_datum_free(&data);
    pa_xfree(name);

    return e;

fail:

    if (e)
        perportentry_free(e);
    if (t)
        pa_tagstruct_free(t);

    pa_datum_free(&data);

#ifdef ENABLE_LEGACY_DATABASE_ENTRY_FORMAT
    /* Try again with a null port. This is used when dealing with migration from older versions */
    if (port) {
        pa_xfree(name);
        return perportentry_read(u, basekeyname, NULL);
    }
#endif

    pa_log_debug("Database contains no (or invalid) data for key: %s", name);

    pa_xfree(name);

    return NULL;
}

static struct perportentry* perportentry_copy(const struct perportentry *e) {
    struct perportentry* r;
    uint32_t idx;
    pa_format_info *f;

    pa_assert(e);
    r = perportentry_new(false);
    r->version = e->version;
    r->muted_valid = e->muted_valid;
    r->volume_valid = e->volume_valid;
    r->muted = e->muted;
    r->channel_map = e->channel_map;
    r->volume = e->volume;

    PA_IDXSET_FOREACH(f, e->formats, idx) {
        pa_idxset_put(r->formats, pa_format_info_copy(f), NULL);
    }
    return r;
}

static bool perportentries_equal(const struct perportentry *a, const struct perportentry *b) {
    pa_cvolume t;

    pa_assert(a && b);

    if (a->muted_valid != b->muted_valid ||
        (a->muted_valid && (a->muted != b->muted)))
        return false;

    t = b->volume;
    if (a->volume_valid != b->volume_valid ||
        (a->volume_valid && !pa_cvolume_equal(pa_cvolume_remap(&t, &b->channel_map, &a->channel_map), &a->volume)))
        return false;

    if (pa_idxset_size(a->formats) != pa_idxset_size(b->formats))
        return false;

    /** TODO: Compare a bit better */

    return true;
}

#ifdef ENABLE_LEGACY_DATABASE_ENTRY_FORMAT

#define LEGACY_ENTRY_VERSION 2
static bool legacy_entry_read(struct userdata *u, pa_datum *data, struct entry **entry, struct perportentry **perportentry) {
    struct legacy_entry {
        uint8_t version;
        bool muted_valid:1, volume_valid:1, port_valid:1;
        bool muted:1;
        pa_channel_map channel_map;
        pa_cvolume volume;
        char port[PA_NAME_MAX];
    } PA_GCC_PACKED;
    struct legacy_entry *le;

    pa_assert(u);
    pa_assert(data);
    pa_assert(entry);
    pa_assert(perportentry);

    if (data->size != sizeof(struct legacy_entry)) {
        pa_log_debug("Size does not match.");
        return false;
    }

    le = (struct legacy_entry*)data->data;

    if (le->version != LEGACY_ENTRY_VERSION) {
        pa_log_debug("Version mismatch.");
        return false;
    }

    if (!memchr(le->port, 0, sizeof(le->port))) {
        pa_log_warn("Port has missing NUL byte.");
        return false;
    }

    if (le->volume_valid && !pa_channel_map_valid(&le->channel_map)) {
        pa_log_warn("Invalid channel map.");
        return false;
    }

    if (le->volume_valid && (!pa_cvolume_valid(&le->volume) || !pa_cvolume_compatible_with_channel_map(&le->volume, &le->channel_map))) {
        pa_log_warn("Volume and channel map don't match.");
        return false;
    }

    *entry = entry_new();
    (*entry)->port_valid = le->port_valid;
    (*entry)->port = pa_xstrdup(le->port);

    *perportentry = perportentry_new(true);
    (*perportentry)->muted_valid = le->muted_valid;
    (*perportentry)->volume_valid = le->volume_valid;
    (*perportentry)->muted = le->muted;
    (*perportentry)->channel_map = le->channel_map;
    (*perportentry)->volume = le->volume;

    return true;
}
#endif

static void subscribe_callback(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    struct userdata *u = userdata;
    struct entry *e, *olde;
    struct perportentry *ppe, *oldppe;
    char *name;
    const char *port = NULL;
    pa_device_type_t type;
    bool written = false;

    pa_assert(c);
    pa_assert(u);

    if (t != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_CHANGE) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_CHANGE))
        return;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
        pa_sink *sink;

        if (!(sink = pa_idxset_get_by_index(c->sinks, idx)))
            return;

        type = PA_DEVICE_TYPE_SINK;
        name = pa_sprintf_malloc("sink:%s", sink->name);
        if (sink->active_port)
            port = sink->active_port->name;

        if ((olde = entry_read(u, name)))
            e = entry_copy(olde);
        else
            e = entry_new();

        if (sink->save_port) {
            pa_xfree(e->port);
            e->port = pa_xstrdup(port ? port : "");
            e->port_valid = true;
        }

        if ((oldppe = perportentry_read(u, name, port)))
            ppe = perportentry_copy(oldppe);
        else
            ppe = perportentry_new(true);

        if (sink->save_volume) {
            ppe->channel_map = sink->channel_map;
            ppe->volume = *pa_sink_get_volume(sink, false);
            ppe->volume_valid = true;
        }

        if (sink->save_muted) {
            ppe->muted = pa_sink_get_mute(sink, false);
            ppe->muted_valid = true;
        }
    } else {
        pa_source *source;

        pa_assert((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE);

        if (!(source = pa_idxset_get_by_index(c->sources, idx)))
            return;

        type = PA_DEVICE_TYPE_SOURCE;
        name = pa_sprintf_malloc("source:%s", source->name);
        if (source->active_port)
            port = source->active_port->name;

        if ((olde = entry_read(u, name)))
            e = entry_copy(olde);
        else
            e = entry_new();

        if (source->save_port) {
            pa_xfree(e->port);
            e->port = pa_xstrdup(port ? port : "");
            e->port_valid = true;
        }

        if ((oldppe = perportentry_read(u, name, port)))
            ppe = perportentry_copy(oldppe);
        else
            ppe = perportentry_new(true);

        if (source->save_volume) {
            ppe->channel_map = source->channel_map;
            ppe->volume = *pa_source_get_volume(source, false);
            ppe->volume_valid = true;
        }

        if (source->save_muted) {
            ppe->muted = pa_source_get_mute(source, false);
            ppe->muted_valid = true;
        }
    }

    pa_assert(e);

    if (olde) {

        if (entries_equal(olde, e)) {
            entry_free(olde);
            entry_free(e);
            e = NULL;
        } else
            entry_free(olde);
    }

    if (e) {
        pa_log_info("Storing port for device %s.", name);

        written = entry_write(u, name, e);

        entry_free(e);
    }

    pa_assert(ppe);

    if (oldppe) {

        if (perportentries_equal(oldppe, ppe)) {
            perportentry_free(oldppe);
            perportentry_free(ppe);
            ppe = NULL;
        } else
            perportentry_free(oldppe);
    }

    if (ppe) {
        pa_log_info("Storing volume/mute for device+port %s:%s.", name, (port ? port : "null"));

        written = perportentry_write(u, name, port, ppe) || written;

        perportentry_free(ppe);
    }
    pa_xfree(name);

    if (written)
        trigger_save(u, type, idx);
}

static pa_hook_result_t sink_new_hook_callback(pa_core *c, pa_sink_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_port);

    name = pa_sprintf_malloc("sink:%s", new_data->name);

    if ((e = entry_read(u, name))) {

        if (e->port_valid) {
            if (!new_data->active_port) {
                pa_log_info("Restoring port for sink %s.", name);
                pa_sink_new_data_set_port(new_data, e->port);
                new_data->save_port = true;
            } else
                pa_log_debug("Not restoring port for sink %s, because already set.", name);
        }

        entry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_fixate_hook_callback(pa_core *c, pa_sink_new_data *new_data, struct userdata *u) {
    char *name;
    struct perportentry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_volume || u->restore_muted);

    name = pa_sprintf_malloc("sink:%s", new_data->name);

    if ((e = perportentry_read(u, name, new_data->active_port))) {

        if (u->restore_volume && e->volume_valid) {

            if (!new_data->volume_is_set) {
                pa_cvolume v;
                char buf[PA_CVOLUME_SNPRINT_VERBOSE_MAX];

                v = e->volume;
                pa_cvolume_remap(&v, &e->channel_map, &new_data->channel_map);
                pa_sink_new_data_set_volume(new_data, &v);
                pa_log_info("Restoring volume for sink %s: %s", new_data->name,
                            pa_cvolume_snprint_verbose(buf, sizeof(buf), &new_data->volume, &new_data->channel_map, false));

                new_data->save_volume = true;
            } else
                pa_log_debug("Not restoring volume for sink %s, because already set.", new_data->name);
        }

        if (u->restore_muted && e->muted_valid) {

            if (!new_data->muted_is_set) {
                pa_sink_new_data_set_muted(new_data, e->muted);
                new_data->save_muted = true;
                pa_log_info("Restoring mute state for sink %s: %smuted", new_data->name,
                            new_data->muted ? "" : "un");
            } else
                pa_log_debug("Not restoring mute state for sink %s, because already set.", new_data->name);
        }

        perportentry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_port_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    char *name;
    struct perportentry *e;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->restore_volume || u->restore_muted);

    name = pa_sprintf_malloc("sink:%s", sink->name);

    if ((e = perportentry_read(u, name, (sink->active_port ? sink->active_port->name : NULL)))) {

        if (u->restore_volume && e->volume_valid) {
            pa_cvolume v;

            pa_log_info("Restoring volume for sink %s.", sink->name);
            v = e->volume;
            pa_cvolume_remap(&v, &e->channel_map, &sink->channel_map);
            pa_sink_set_volume(sink, &v, true, false);

            sink->save_volume = true;
        }

        if (u->restore_muted && e->muted_valid) {

            pa_log_info("Restoring mute state for sink %s.", sink->name);
            pa_sink_set_mute(sink, e->muted, false);
            sink->save_muted = true;
        }

        perportentry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    char *name;
    struct perportentry *e;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->restore_formats);

    name = pa_sprintf_malloc("sink:%s", sink->name);

    if ((e = perportentry_read(u, name, (sink->active_port ? sink->active_port->name : NULL)))) {

        if (!pa_sink_set_formats(sink, e->formats))
            pa_log_debug("Could not set format on sink %s", sink->name);

        perportentry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_new_hook_callback(pa_core *c, pa_source_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_port);

    name = pa_sprintf_malloc("source:%s", new_data->name);

    if ((e = entry_read(u, name))) {

        if (e->port_valid) {
            if (!new_data->active_port) {
                pa_log_info("Restoring port for source %s.", name);
                pa_source_new_data_set_port(new_data, e->port);
                new_data->save_port = true;
            } else
                pa_log_debug("Not restoring port for source %s, because already set.", name);
        }

        entry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_fixate_hook_callback(pa_core *c, pa_source_new_data *new_data, struct userdata *u) {
    char *name;
    struct perportentry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_volume || u->restore_muted);

    name = pa_sprintf_malloc("source:%s", new_data->name);

    if ((e = perportentry_read(u, name, new_data->active_port))) {

        if (u->restore_volume && e->volume_valid) {

            if (!new_data->volume_is_set) {
                pa_cvolume v;
                char buf[PA_CVOLUME_SNPRINT_VERBOSE_MAX];

                v = e->volume;
                pa_cvolume_remap(&v, &e->channel_map, &new_data->channel_map);
                pa_source_new_data_set_volume(new_data, &v);
                pa_log_info("Restoring volume for source %s: %s", new_data->name,
                            pa_cvolume_snprint_verbose(buf, sizeof(buf), &new_data->volume, &new_data->channel_map, false));

                new_data->save_volume = true;
            } else
                pa_log_debug("Not restoring volume for source %s, because already set.", new_data->name);
        }

        if (u->restore_muted && e->muted_valid) {

            if (!new_data->muted_is_set) {
                pa_source_new_data_set_muted(new_data, e->muted);
                new_data->save_muted = true;
                pa_log_info("Restoring mute state for source %s: %smuted", new_data->name,
                            new_data->muted ? "" : "un");
            } else
                pa_log_debug("Not restoring mute state for source %s, because already set.", new_data->name);
        }

        perportentry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_port_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    char *name;
    struct perportentry *e;

    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->restore_volume || u->restore_muted);

    name = pa_sprintf_malloc("source:%s", source->name);

    if ((e = perportentry_read(u, name, (source->active_port ? source->active_port->name : NULL)))) {

        if (u->restore_volume && e->volume_valid) {
            pa_cvolume v;

            pa_log_info("Restoring volume for source %s.", source->name);
            v = e->volume;
            pa_cvolume_remap(&v, &e->channel_map, &source->channel_map);
            pa_source_set_volume(source, &v, true, false);

            source->save_volume = true;
        }

        if (u->restore_muted && e->muted_valid) {

            pa_log_info("Restoring mute state for source %s.", source->name);
            pa_source_set_mute(source, e->muted, false);
            source->save_muted = true;
        }

        perportentry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

#define EXT_VERSION 1

static void read_sink_format_reply(struct userdata *u, pa_tagstruct *reply, pa_sink *sink) {
    struct perportentry *e;
    char *name;

    pa_assert(u);
    pa_assert(reply);
    pa_assert(sink);

    pa_tagstruct_putu32(reply, PA_DEVICE_TYPE_SINK);
    pa_tagstruct_putu32(reply, sink->index);

    /* Read or create an entry */
    name = pa_sprintf_malloc("sink:%s", sink->name);
    if (!(e = perportentry_read(u, name, (sink->active_port ? sink->active_port->name : NULL)))) {
        /* Fake a reply with PCM encoding supported */
        pa_format_info *f = pa_format_info_new();

        pa_tagstruct_putu8(reply, 1);
        f->encoding = PA_ENCODING_PCM;
        pa_tagstruct_put_format_info(reply, f);

        pa_format_info_free(f);
    } else {
        uint32_t idx;
        pa_format_info *f;

        /* Write all the formats from the entry to the reply */
        pa_tagstruct_putu8(reply, pa_idxset_size(e->formats));
        PA_IDXSET_FOREACH(f, e->formats, idx) {
            pa_tagstruct_put_format_info(reply, f);
        }
        perportentry_free(e);
    }
    pa_xfree(name);
}

static int extension_cb(pa_native_protocol *p, pa_module *m, pa_native_connection *c, uint32_t tag, pa_tagstruct *t) {
    struct userdata *u;
    uint32_t command;
    pa_tagstruct *reply = NULL;

    pa_assert(p);
    pa_assert(m);
    pa_assert(c);
    pa_assert(t);

    u = m->userdata;

    if (pa_tagstruct_getu32(t, &command) < 0)
        goto fail;

    reply = pa_tagstruct_new();
    pa_tagstruct_putu32(reply, PA_COMMAND_REPLY);
    pa_tagstruct_putu32(reply, tag);

    switch (command) {
        case SUBCOMMAND_TEST: {
            if (!pa_tagstruct_eof(t))
                goto fail;

            pa_tagstruct_putu32(reply, EXT_VERSION);
            break;
        }

        case SUBCOMMAND_SUBSCRIBE: {

            bool enabled;

            if (pa_tagstruct_get_boolean(t, &enabled) < 0 ||
                !pa_tagstruct_eof(t))
                goto fail;

            if (enabled)
                pa_idxset_put(u->subscribed, c, NULL);
            else
                pa_idxset_remove_by_data(u->subscribed, c, NULL);

            break;
        }

        case SUBCOMMAND_READ_FORMATS_ALL: {
            pa_sink *sink;
            uint32_t idx;

            if (!pa_tagstruct_eof(t))
                goto fail;

            PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
                read_sink_format_reply(u, reply, sink);
            }

            break;
        }
        case SUBCOMMAND_READ_FORMATS: {
            pa_device_type_t type;
            uint32_t sink_index;
            pa_sink *sink;

            pa_assert(reply);

            /* Get the sink index and the number of formats from the tagstruct */
            if (pa_tagstruct_getu32(t, &type) < 0 ||
                pa_tagstruct_getu32(t, &sink_index) < 0)
                goto fail;

            if (type != PA_DEVICE_TYPE_SINK) {
                pa_log("Device format reading is only supported on sinks");
                goto fail;
            }

            if (!pa_tagstruct_eof(t))
                goto fail;

            /* Now find our sink */
            if (!(sink = pa_idxset_get_by_index(u->core->sinks, sink_index)))
                goto fail;

            read_sink_format_reply(u, reply, sink);

            break;
        }

        case SUBCOMMAND_SAVE_FORMATS: {

            struct perportentry *e;
            pa_device_type_t type;
            uint32_t sink_index;
            char *name;
            pa_sink *sink;
            uint8_t i, n_formats;

            /* Get the sink index and the number of formats from the tagstruct */
            if (pa_tagstruct_getu32(t, &type) < 0 ||
                pa_tagstruct_getu32(t, &sink_index) < 0 ||
                pa_tagstruct_getu8(t, &n_formats) < 0 || n_formats < 1) {

                goto fail;
            }

            if (type != PA_DEVICE_TYPE_SINK) {
                pa_log("Device format saving is only supported on sinks");
                goto fail;
            }

            /* Now find our sink */
            if (!(sink = pa_idxset_get_by_index(u->core->sinks, sink_index))) {
                pa_log("Could not find sink #%d", sink_index);
                goto fail;
            }

            /* Read or create an entry */
            name = pa_sprintf_malloc("sink:%s", sink->name);
            if (!(e = perportentry_read(u, name, (sink->active_port ? sink->active_port->name : NULL))))
                e = perportentry_new(false);
            else {
                /* Clean out any saved formats */
                pa_idxset_free(e->formats, (pa_free_cb_t) pa_format_info_free);
                e->formats = pa_idxset_new(NULL, NULL);
            }

            /* Read all the formats from our tagstruct */
            for (i = 0; i < n_formats; ++i) {
                pa_format_info *f = pa_format_info_new();
                if (pa_tagstruct_get_format_info(t, f) < 0) {
                    pa_format_info_free(f);
                    perportentry_free(e);
                    pa_xfree(name);
                    goto fail;
                }
                pa_idxset_put(e->formats, f, NULL);
            }

            if (!pa_tagstruct_eof(t)) {
                perportentry_free(e);
                pa_xfree(name);
                goto fail;
            }

            if (pa_sink_set_formats(sink, e->formats) && perportentry_write(u, name, (sink->active_port ? sink->active_port->name : NULL), e))
                trigger_save(u, type, sink_index);
            else
                pa_log_warn("Could not save format info for sink %s", sink->name);

            pa_xfree(name);
            perportentry_free(e);

            break;
        }

        default:
            goto fail;
    }

    pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), reply);
    return 0;

fail:

    if (reply)
        pa_tagstruct_free(reply);

    return -1;
}

static pa_hook_result_t connection_unlink_hook_cb(pa_native_protocol *p, pa_native_connection *c, struct userdata *u) {
    pa_assert(p);
    pa_assert(c);
    pa_assert(u);

    pa_idxset_remove_by_data(u->subscribed, c, NULL);
    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    char *fname;
    pa_sink *sink;
    pa_source *source;
    uint32_t idx;
    bool restore_volume = true, restore_muted = true, restore_port = true, restore_formats = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "restore_volume", &restore_volume) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_muted", &restore_muted) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_port", &restore_port) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_formats", &restore_formats) < 0) {
        pa_log("restore_port, restore_volume, restore_muted and restore_formats expect boolean arguments");
        goto fail;
    }

    if (!restore_muted && !restore_volume && !restore_port && !restore_formats)
        pa_log_warn("Neither restoring volume, nor restoring muted, nor restoring port enabled!");

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->restore_volume = restore_volume;
    u->restore_muted = restore_muted;
    u->restore_port = restore_port;
    u->restore_formats = restore_formats;

    u->subscribed = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->protocol = pa_native_protocol_get(m->core);
    pa_native_protocol_install_ext(u->protocol, m, extension_cb);

    pa_module_hook_connect(m, &pa_native_protocol_hooks(u->protocol)[PA_NATIVE_HOOK_CONNECTION_UNLINK], PA_HOOK_NORMAL, (pa_hook_cb_t) connection_unlink_hook_cb, u);

    u->subscription = pa_subscription_new(m->core, PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE, subscribe_callback, u);

    if (restore_port) {
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) sink_new_hook_callback, u);
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) source_new_hook_callback, u);
    }

    if (restore_muted || restore_volume) {
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_FIXATE], PA_HOOK_EARLY, (pa_hook_cb_t) sink_fixate_hook_callback, u);
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_FIXATE], PA_HOOK_EARLY, (pa_hook_cb_t) source_fixate_hook_callback, u);

        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_PORT_CHANGED], PA_HOOK_EARLY, (pa_hook_cb_t) sink_port_hook_callback, u);
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_PORT_CHANGED], PA_HOOK_EARLY, (pa_hook_cb_t) source_port_hook_callback, u);
    }

    if (restore_formats)
        pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_EARLY, (pa_hook_cb_t) sink_put_hook_callback, u);

    if (!(fname = pa_state_path("device-volumes", true)))
        goto fail;

    if (!(u->database = pa_database_open(fname, true))) {
        pa_log("Failed to open volume database '%s': %s", fname, pa_cstrerror(errno));
        pa_xfree(fname);
        goto fail;
    }

    pa_log_info("Successfully opened database file '%s'.", fname);
    pa_xfree(fname);

    PA_IDXSET_FOREACH(sink, m->core->sinks, idx)
        subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_NEW, sink->index, u);

    PA_IDXSET_FOREACH(source, m->core->sources, idx)
        subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_NEW, source->index, u);

    pa_modargs_free(ma);
    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->subscription)
        pa_subscription_free(u->subscription);

    if (u->save_time_event) {
        u->core->mainloop->time_free(u->save_time_event);
        pa_database_sync(u->database);
    }

    if (u->database)
        pa_database_close(u->database);

    if (u->protocol) {
        pa_native_protocol_remove_ext(u->protocol, m);
        pa_native_protocol_unref(u->protocol);
    }

    if (u->subscribed)
        pa_idxset_free(u->subscribed, NULL);

    pa_xfree(u);
}
