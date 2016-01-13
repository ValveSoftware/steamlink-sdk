/***
  This file is part of PulseAudio.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2009 Colin Guthrie

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
#include <pulse/timeval.h>
#include <pulse/rtclock.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/core-subscribe.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/namereg.h>
#include <pulsecore/protocol-native.h>
#include <pulsecore/pstream.h>
#include <pulsecore/pstream-util.h>
#include <pulsecore/database.h>
#include <pulsecore/tagstruct.h>

#include "module-device-manager-symdef.h"

PA_MODULE_AUTHOR("Colin Guthrie");
PA_MODULE_DESCRIPTION("Keep track of devices (and their descriptions) both past and present and prioritise by role");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
    "do_routing=<Automatically route streams based on a priority list (unique per-role)?> "
    "on_hotplug=<When new device becomes available, recheck streams?> "
    "on_rescue=<When device becomes unavailable, recheck streams?>");

#define SAVE_INTERVAL (10 * PA_USEC_PER_SEC)
#define DUMP_DATABASE

static const char* const valid_modargs[] = {
    "do_routing",
    "on_hotplug",
    "on_rescue",
    NULL
};

#define NUM_ROLES 9
enum {
    ROLE_NONE,
    ROLE_VIDEO,
    ROLE_MUSIC,
    ROLE_GAME,
    ROLE_EVENT,
    ROLE_PHONE,
    ROLE_ANIMATION,
    ROLE_PRODUCTION,
    ROLE_A11Y,
    ROLE_MAX
};

typedef uint32_t role_indexes_t[NUM_ROLES];

static const char* role_names[NUM_ROLES] = {
    "none",
    "video",
    "music",
    "game",
    "event",
    "phone",
    "animation",
    "production",
    "a11y",
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_subscription *subscription;
    pa_hook_slot
        *sink_new_hook_slot,
        *source_new_hook_slot,
        *sink_input_new_hook_slot,
        *source_output_new_hook_slot,
        *sink_put_hook_slot,
        *source_put_hook_slot,
        *sink_unlink_hook_slot,
        *source_unlink_hook_slot,
        *connection_unlink_hook_slot;
    pa_time_event *save_time_event;
    pa_database *database;

    pa_native_protocol *protocol;
    pa_idxset *subscribed;

    bool on_hotplug;
    bool on_rescue;
    bool do_routing;

    role_indexes_t preferred_sinks;
    role_indexes_t preferred_sources;
};

#define ENTRY_VERSION 1

struct entry {
    uint8_t version;
    char *description;
    bool user_set_description;
    char *icon;
    role_indexes_t priority;
};

enum {
    SUBCOMMAND_TEST,
    SUBCOMMAND_READ,
    SUBCOMMAND_RENAME,
    SUBCOMMAND_DELETE,
    SUBCOMMAND_ROLE_DEVICE_PRIORITY_ROUTING,
    SUBCOMMAND_REORDER,
    SUBCOMMAND_SUBSCRIBE,
    SUBCOMMAND_EVENT
};

/* Forward declarations */
#ifdef DUMP_DATABASE
static void dump_database(struct userdata *);
#endif
static void notify_subscribers(struct userdata *);

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

#ifdef DUMP_DATABASE
    dump_database(u);
#endif
}

static void trigger_save(struct userdata *u) {

    pa_assert(u);

    notify_subscribers(u);

    if (u->save_time_event)
        return;

    u->save_time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + SAVE_INTERVAL, save_time_callback, u);
}

static struct entry* entry_new(void) {
    struct entry *r = pa_xnew0(struct entry, 1);
    r->version = ENTRY_VERSION;
    return r;
}

static void entry_free(struct entry* e) {
    pa_assert(e);

    pa_xfree(e->description);
    pa_xfree(e->icon);
    pa_xfree(e);
}

static bool entry_write(struct userdata *u, const char *name, const struct entry *e) {
    pa_tagstruct *t;
    pa_datum key, data;
    bool r;

    pa_assert(u);
    pa_assert(name);
    pa_assert(e);

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu8(t, e->version);
    pa_tagstruct_puts(t, e->description);
    pa_tagstruct_put_boolean(t, e->user_set_description);
    pa_tagstruct_puts(t, e->icon);
    for (uint8_t i=0; i<ROLE_MAX; ++i)
        pa_tagstruct_putu32(t, e->priority[i]);

    key.data = (char *) name;
    key.size = strlen(name);

    data.data = (void*)pa_tagstruct_data(t, &data.size);

    r = (pa_database_set(u->database, &key, &data, true) == 0);

    pa_tagstruct_free(t);

    return r;
}

#ifdef ENABLE_LEGACY_DATABASE_ENTRY_FORMAT

#define LEGACY_ENTRY_VERSION 1
static struct entry* legacy_entry_read(struct userdata *u, pa_datum *data) {
    struct legacy_entry {
        uint8_t version;
        char description[PA_NAME_MAX];
        bool user_set_description;
        char icon[PA_NAME_MAX];
        role_indexes_t priority;
    } PA_GCC_PACKED;
    struct legacy_entry *le;
    struct entry *e;

    pa_assert(u);
    pa_assert(data);

    if (data->size != sizeof(struct legacy_entry)) {
        pa_log_debug("Size does not match.");
        return NULL;
    }

    le = (struct legacy_entry*)data->data;

    if (le->version != LEGACY_ENTRY_VERSION) {
        pa_log_debug("Version mismatch.");
        return NULL;
    }

    if (!memchr(le->description, 0, sizeof(le->description))) {
        pa_log_warn("Description has missing NUL byte.");
        return NULL;
    }

    if (!le->description[0]) {
        pa_log_warn("Description is empty.");
        return NULL;
    }

    if (!memchr(le->icon, 0, sizeof(le->icon))) {
        pa_log_warn("Icon has missing NUL byte.");
        return NULL;
    }

    e = entry_new();
    e->description = pa_xstrdup(le->description);
    e->icon = pa_xstrdup(le->icon);
    return e;
}
#endif

static struct entry* entry_read(struct userdata *u, const char *name) {
    pa_datum key, data;
    struct entry *e = NULL;
    pa_tagstruct *t = NULL;
    const char *description, *icon;

    pa_assert(u);
    pa_assert(name);

    key.data = (char*) name;
    key.size = strlen(name);

    pa_zero(data);

    if (!pa_database_get(u->database, &key, &data))
        goto fail;

    t = pa_tagstruct_new(data.data, data.size);
    e = entry_new();

    if (pa_tagstruct_getu8(t, &e->version) < 0 ||
        e->version > ENTRY_VERSION ||
        pa_tagstruct_gets(t, &description) < 0 ||
        pa_tagstruct_get_boolean(t, &e->user_set_description) < 0 ||
        pa_tagstruct_gets(t, &icon) < 0) {

        goto fail;
    }

    if (e->user_set_description && !description) {
        pa_log("Entry has user_set_description set, but the description is NULL.");
        goto fail;
    }

    if (e->user_set_description && !*description) {
        pa_log("Entry has user_set_description set, but the description is empty.");
        goto fail;
    }

    e->description = pa_xstrdup(description);
    e->icon = pa_xstrdup(icon);

    for (uint8_t i=0; i<ROLE_MAX; ++i) {
        if (pa_tagstruct_getu32(t, &e->priority[i]) < 0)
            goto fail;
    }

    if (!pa_tagstruct_eof(t))
        goto fail;

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
    pa_log_debug("Attempting to load legacy (pre-v1.0) data for key: %s", name);
    if ((e = legacy_entry_read(u, &data))) {
        pa_log_debug("Success. Saving new format for key: %s", name);
        if (entry_write(u, name, e))
            trigger_save(u);
        pa_datum_free(&data);
        return e;
    } else
        pa_log_debug("Unable to load legacy (pre-v1.0) data for key: %s. Ignoring.", name);
#endif

    pa_datum_free(&data);
    return NULL;
}

#ifdef DUMP_DATABASE
static void dump_database_helper(struct userdata *u, uint32_t role_index, const char* human, bool sink_mode) {
    pa_assert(u);
    pa_assert(human);

    if (sink_mode) {
        pa_sink *s;
        if (PA_INVALID_INDEX != u->preferred_sinks[role_index] && (s = pa_idxset_get_by_index(u->core->sinks, u->preferred_sinks[role_index])))
            pa_log_debug("   %s %s (%s)", human, pa_strnull(pa_proplist_gets(s->proplist, PA_PROP_DEVICE_DESCRIPTION)), s->name);
        else
            pa_log_debug("   %s No sink specified", human);
    } else {
        pa_source *s;
        if (PA_INVALID_INDEX != u->preferred_sources[role_index] && (s = pa_idxset_get_by_index(u->core->sources, u->preferred_sources[role_index])))
            pa_log_debug("   %s %s (%s)", human, pa_strnull(pa_proplist_gets(s->proplist, PA_PROP_DEVICE_DESCRIPTION)), s->name);
        else
            pa_log_debug("   %s No source specified", human);
    }
}

static void dump_database(struct userdata *u) {
    pa_datum key;
    bool done;

    pa_assert(u);

    done = !pa_database_first(u->database, &key, NULL);

    pa_log_debug("Dumping database");
    while (!done) {
        char *name;
        struct entry *e;
        pa_datum next_key;

        done = !pa_database_next(u->database, &key, &next_key, NULL);

        name = pa_xstrndup(key.data, key.size);

        if ((e = entry_read(u, name))) {
            pa_log_debug(" Got entry: %s", name);
            pa_log_debug("  Description: %s", e->description);
            pa_log_debug("  Priorities: None:   %3u, Video: %3u, Music:  %3u, Game: %3u, Event: %3u",
                         e->priority[ROLE_NONE], e->priority[ROLE_VIDEO], e->priority[ROLE_MUSIC], e->priority[ROLE_GAME], e->priority[ROLE_EVENT]);
            pa_log_debug("              Phone:  %3u, Anim:  %3u, Prodtn: %3u, A11y: %3u",
                         e->priority[ROLE_PHONE], e->priority[ROLE_ANIMATION], e->priority[ROLE_PRODUCTION], e->priority[ROLE_A11Y]);
            entry_free(e);
        }

        pa_xfree(name);

        pa_datum_free(&key);
        key = next_key;
    }

    if (u->do_routing) {
        pa_log_debug(" Highest priority devices per-role:");

        pa_log_debug("  Sinks:");
        for (uint32_t role = ROLE_NONE; role < NUM_ROLES; ++role) {
            char name[13];
            uint32_t len = PA_MIN(12u, strlen(role_names[role]));
            strncpy(name, role_names[role], len);
            for (int i = len+1; i < 12; ++i) name[i] = ' ';
            name[len] = ':'; name[0] -= 32; name[12] = '\0';
            dump_database_helper(u, role, name, true);
        }

        pa_log_debug("  Sources:");
        for (uint32_t role = ROLE_NONE; role < NUM_ROLES; ++role) {
            char name[13];
            uint32_t len = PA_MIN(12u, strlen(role_names[role]));
            strncpy(name, role_names[role], len);
            for (int i = len+1; i < 12; ++i) name[i] = ' ';
            name[len] = ':'; name[0] -= 32; name[12] = '\0';
            dump_database_helper(u, role, name, false);
        }
    }

    pa_log_debug("Completed database dump");
}
#endif

static void notify_subscribers(struct userdata *u) {

    pa_native_connection *c;
    uint32_t idx;

    pa_assert(u);

    PA_IDXSET_FOREACH(c, u->subscribed, idx) {
        pa_tagstruct *t;

        t = pa_tagstruct_new(NULL, 0);
        pa_tagstruct_putu32(t, PA_COMMAND_EXTENSION);
        pa_tagstruct_putu32(t, 0);
        pa_tagstruct_putu32(t, u->module->index);
        pa_tagstruct_puts(t, u->module->name);
        pa_tagstruct_putu32(t, SUBCOMMAND_EVENT);

        pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), t);
    }
}

static bool entries_equal(const struct entry *a, const struct entry *b) {

    pa_assert(a);
    pa_assert(b);

    if (!pa_streq(a->description, b->description)
        || a->user_set_description != b->user_set_description
        || !pa_streq(a->icon, b->icon))
        return false;

    for (int i=0; i < NUM_ROLES; ++i)
        if (a->priority[i] != b->priority[i])
            return false;

    return true;
}

static char *get_name(const char *key, const char *prefix) {
    char *t;

    if (strncmp(key, prefix, strlen(prefix)))
        return NULL;

    t = pa_xstrdup(key + strlen(prefix));
    return t;
}

static inline struct entry *load_or_initialize_entry(struct userdata *u, struct entry *entry, const char *name, const char *prefix) {
    struct entry *old;

    pa_assert(u);
    pa_assert(entry);
    pa_assert(name);
    pa_assert(prefix);

    if ((old = entry_read(u, name))) {
        *entry = *old;
        entry->description = pa_xstrdup(old->description);
        entry->icon = pa_xstrdup(old->icon);
    } else {
        /* This is a new device, so make sure we write its priority list correctly */
        role_indexes_t max_priority;
        pa_datum key;
        bool done;

        pa_zero(max_priority);
        done = !pa_database_first(u->database, &key, NULL);

        /* Find all existing devices with the same prefix so we calculate the current max priority for each role */
        while (!done) {
            pa_datum next_key;

            done = !pa_database_next(u->database, &key, &next_key, NULL);

            if (key.size > strlen(prefix) && strncmp(key.data, prefix, strlen(prefix)) == 0) {
                char *name2;
                struct entry *e;

                name2 = pa_xstrndup(key.data, key.size);

                if ((e = entry_read(u, name2))) {
                    for (uint32_t i = 0; i < NUM_ROLES; ++i) {
                        max_priority[i] = PA_MAX(max_priority[i], e->priority[i]);
                    }

                    entry_free(e);
                }

                pa_xfree(name2);
            }
            pa_datum_free(&key);
            key = next_key;
        }

        /* Actually initialise our entry now we've calculated it */
        for (uint32_t i = 0; i < NUM_ROLES; ++i) {
            entry->priority[i] = max_priority[i] + 1;
        }
        entry->user_set_description = false;
    }

    return old;
}

static uint32_t get_role_index(const char* role) {
    pa_assert(role);

    for (uint32_t i = ROLE_NONE; i < NUM_ROLES; ++i)
        if (pa_streq(role, role_names[i]))
            return i;

    return PA_INVALID_INDEX;
}

static void update_highest_priority_device_indexes(struct userdata *u, const char *prefix, void *ignore_device) {
    role_indexes_t *indexes, highest_priority_available;
    pa_datum key;
    bool done, sink_mode;

    pa_assert(u);
    pa_assert(prefix);

    sink_mode = pa_streq(prefix, "sink:");

    if (sink_mode)
        indexes = &u->preferred_sinks;
    else
        indexes = &u->preferred_sources;

    for (uint32_t i = 0; i < NUM_ROLES; ++i) {
        (*indexes)[i] = PA_INVALID_INDEX;
    }
    pa_zero(highest_priority_available);

    done = !pa_database_first(u->database, &key, NULL);

    /* Find all existing devices with the same prefix so we find the highest priority device for each role */
    while (!done) {
        pa_datum next_key;

        done = !pa_database_next(u->database, &key, &next_key, NULL);

        if (key.size > strlen(prefix) && strncmp(key.data, prefix, strlen(prefix)) == 0) {
            char *name, *device_name;
            struct entry *e;

            name = pa_xstrndup(key.data, key.size);
            pa_assert_se(device_name = get_name(name, prefix));

            if ((e = entry_read(u, name))) {
                for (uint32_t i = 0; i < NUM_ROLES; ++i) {
                    if (!highest_priority_available[i] || e->priority[i] < highest_priority_available[i]) {
                        /* We've found a device with a higher priority than that we've currently got,
                           so see if it is currently available or not and update our list */
                        uint32_t idx;
                        bool found = false;

                        if (sink_mode) {
                            pa_sink *sink;

                            PA_IDXSET_FOREACH(sink, u->core->sinks, idx) {
                                if ((pa_sink*) ignore_device == sink)
                                    continue;
                                if (!PA_SINK_IS_LINKED(sink->state))
                                    continue;
                                if (pa_streq(sink->name, device_name)) {
                                    found = true;
                                    idx = sink->index; /* Is this needed? */
                                    break;
                                }
                            }
                        } else {
                            pa_source *source;

                            PA_IDXSET_FOREACH(source, u->core->sources, idx) {
                                if ((pa_source*) ignore_device == source)
                                    continue;
                                if (!PA_SOURCE_IS_LINKED(source->state))
                                    continue;
                                if (pa_streq(source->name, device_name)) {
                                    found = true;
                                    idx = source->index; /* Is this needed? */
                                    break;
                                }
                            }
                        }
                        if (found) {
                            highest_priority_available[i] = e->priority[i];
                            (*indexes)[i] = idx;
                        }

                    }
                }

                entry_free(e);
            }

            pa_xfree(name);
            pa_xfree(device_name);
        }

        pa_datum_free(&key);
        key = next_key;
    }
}

static void route_sink_input(struct userdata *u, pa_sink_input *si) {
    const char *role;
    uint32_t role_index, device_index;
    pa_sink *sink;

    pa_assert(u);
    pa_assert(u->do_routing);

    if (si->save_sink)
        return;

    /* Skip this if it is already in the process of being moved anyway */
    if (!si->sink)
        return;

    /* It might happen that a stream and a sink are set up at the
    same time, in which case we want to make sure we don't
    interfere with that */
    if (!PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(si)))
        return;

    if (!(role = pa_proplist_gets(si->proplist, PA_PROP_MEDIA_ROLE)))
        role_index = get_role_index("none");
    else
        role_index = get_role_index(role);

    if (PA_INVALID_INDEX == role_index)
        return;

    device_index = u->preferred_sinks[role_index];
    if (PA_INVALID_INDEX == device_index)
        return;

    if (!(sink = pa_idxset_get_by_index(u->core->sinks, device_index)))
        return;

    if (si->sink != sink)
        pa_sink_input_move_to(si, sink, false);
}

static pa_hook_result_t route_sink_inputs(struct userdata *u, pa_sink *ignore_sink) {
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(u);

    if (!u->do_routing)
        return PA_HOOK_OK;

    update_highest_priority_device_indexes(u, "sink:", ignore_sink);

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        route_sink_input(u, si);
    }

    return PA_HOOK_OK;
}

static void route_source_output(struct userdata *u, pa_source_output *so) {
    const char *role;
    uint32_t role_index, device_index;
    pa_source *source;

    pa_assert(u);
    pa_assert(u->do_routing);

    if (so->save_source)
        return;

    if (so->direct_on_input)
        return;

    /* Skip this if it is already in the process of being moved anyway */
    if (!so->source)
        return;

    /* It might happen that a stream and a source are set up at the
    same time, in which case we want to make sure we don't
    interfere with that */
    if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(so)))
        return;

    if (!(role = pa_proplist_gets(so->proplist, PA_PROP_MEDIA_ROLE)))
        role_index = get_role_index("none");
    else
        role_index = get_role_index(role);

    if (PA_INVALID_INDEX == role_index)
        return;

    device_index = u->preferred_sources[role_index];
    if (PA_INVALID_INDEX == device_index)
        return;

    if (!(source = pa_idxset_get_by_index(u->core->sources, device_index)))
        return;

    if (so->source != source)
        pa_source_output_move_to(so, source, false);
}

static pa_hook_result_t route_source_outputs(struct userdata *u, pa_source* ignore_source) {
    pa_source_output *so;
    uint32_t idx;

    pa_assert(u);

    if (!u->do_routing)
        return PA_HOOK_OK;

    update_highest_priority_device_indexes(u, "source:", ignore_source);

    PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
        route_source_output(u, so);
    }

    return PA_HOOK_OK;
}

static void subscribe_callback(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    struct userdata *u = userdata;
    struct entry *entry, *old = NULL;
    char *name = NULL;

    pa_assert(c);
    pa_assert(u);

    if (t != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_CHANGE) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_CHANGE) &&

        /*t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_NEW) &&*/
        t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_CHANGE) &&
        /*t != (PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT|PA_SUBSCRIPTION_EVENT_NEW) &&*/
        t != (PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT|PA_SUBSCRIPTION_EVENT_CHANGE))
        return;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        pa_sink_input *si;

        if (!u->do_routing)
            return;
        if (!(si = pa_idxset_get_by_index(c->sink_inputs, idx)))
            return;

        /* The role may change mid-stream, so we reroute */
        route_sink_input(u, si);

        return;
    } else if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
        pa_source_output *so;

        if (!u->do_routing)
            return;
        if (!(so = pa_idxset_get_by_index(c->source_outputs, idx)))
            return;

        /* The role may change mid-stream, so we reroute */
        route_source_output(u, so);

        return;
    } else if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
        pa_sink *sink;

        if (!(sink = pa_idxset_get_by_index(c->sinks, idx)))
            return;

        entry = entry_new();
        name = pa_sprintf_malloc("sink:%s", sink->name);

        old = load_or_initialize_entry(u, entry, name, "sink:");

        if (!entry->user_set_description) {
            pa_xfree(entry->description);
            entry->description = pa_xstrdup(pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_DESCRIPTION));
        } else if (!pa_streq(entry->description, pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_DESCRIPTION))) {
            /* Warning: If two modules fight over the description, this could cause an infinite loop.
               by changing the description here, we retrigger this subscription callback. The only thing stopping us from
               looping is the fact that the string comparison will fail on the second iteration. If another module tries to manage
               the description, this will fail... */
            pa_sink_set_description(sink, entry->description);
        }

        pa_xfree(entry->icon);
        entry->icon = pa_xstrdup(pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_ICON_NAME));

    } else if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE) {
        pa_source *source;

        pa_assert((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE);

        if (!(source = pa_idxset_get_by_index(c->sources, idx)))
            return;

        if (source->monitor_of)
            return;

        entry = entry_new();
        name = pa_sprintf_malloc("source:%s", source->name);

        old = load_or_initialize_entry(u, entry, name, "source:");

        if (!entry->user_set_description) {
            pa_xfree(entry->description);
            entry->description = pa_xstrdup(pa_proplist_gets(source->proplist, PA_PROP_DEVICE_DESCRIPTION));
        } else if (!pa_streq(entry->description, pa_proplist_gets(source->proplist, PA_PROP_DEVICE_DESCRIPTION))) {
            /* Warning: If two modules fight over the description, this could cause an infinite loop.
               by changing the description here, we retrigger this subscription callback. The only thing stopping us from
               looping is the fact that the string comparison will fail on the second iteration. If another module tries to manage
               the description, this will fail... */
            pa_source_set_description(source, entry->description);
        }

        pa_xfree(entry->icon);
        entry->icon = pa_xstrdup(pa_proplist_gets(source->proplist, PA_PROP_DEVICE_ICON_NAME));
    } else {
        pa_assert_not_reached();
    }

    pa_assert(name);

    if (old) {

        if (entries_equal(old, entry)) {
            entry_free(old);
            entry_free(entry);
            pa_xfree(name);

            return;
        }

        entry_free(old);
    }

    pa_log_info("Storing device %s.", name);

    if (entry_write(u, name, entry))
        trigger_save(u);
    else
        pa_log_warn("Could not save device");;

    entry_free(entry);
    pa_xfree(name);
}

static pa_hook_result_t sink_new_hook_callback(pa_core *c, pa_sink_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    name = pa_sprintf_malloc("sink:%s", new_data->name);

    if ((e = entry_read(u, name))) {
        if (e->user_set_description && !pa_safe_streq(e->description, pa_proplist_gets(new_data->proplist, PA_PROP_DEVICE_DESCRIPTION))) {
            pa_log_info("Restoring description for sink %s.", new_data->name);
            pa_proplist_sets(new_data->proplist, PA_PROP_DEVICE_DESCRIPTION, e->description);
        }

        entry_free(e);
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

    name = pa_sprintf_malloc("source:%s", new_data->name);

    if ((e = entry_read(u, name))) {
        if (e->user_set_description && !pa_safe_streq(e->description, pa_proplist_gets(new_data->proplist, PA_PROP_DEVICE_DESCRIPTION))) {
            /* NB, We cannot detect if we are a monitor here... this could mess things up a bit... */
            pa_log_info("Restoring description for source %s.", new_data->name);
            pa_proplist_sets(new_data->proplist, PA_PROP_DEVICE_DESCRIPTION, e->description);
        }

        entry_free(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_new_hook_callback(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u) {
    const char *role;
    uint32_t role_index;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!u->do_routing)
        return PA_HOOK_OK;

    if (new_data->sink)
        pa_log_debug("Not restoring device for stream because already set.");
    else {
        if (!(role = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_ROLE)))
            role_index = get_role_index("none");
        else
            role_index = get_role_index(role);

        if (PA_INVALID_INDEX != role_index) {
            uint32_t device_index;

            device_index = u->preferred_sinks[role_index];
            if (PA_INVALID_INDEX != device_index) {
                pa_sink *sink;

                if ((sink = pa_idxset_get_by_index(u->core->sinks, device_index))) {
                    if (!pa_sink_input_new_data_set_sink(new_data, sink, false))
                        pa_log_debug("Not restoring device for stream because no supported format was found");
                }
            }
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_new_hook_callback(pa_core *c, pa_source_output_new_data *new_data, struct userdata *u) {
    const char *role;
    uint32_t role_index;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);

    if (!u->do_routing)
        return PA_HOOK_OK;

    if (new_data->direct_on_input)
        return PA_HOOK_OK;

    if (new_data->source)
        pa_log_debug("Not restoring device for stream because already set.");
    else {
        if (!(role = pa_proplist_gets(new_data->proplist, PA_PROP_MEDIA_ROLE)))
            role_index = get_role_index("none");
        else
            role_index = get_role_index(role);

        if (PA_INVALID_INDEX != role_index) {
            uint32_t device_index;

            device_index = u->preferred_sources[role_index];
            if (PA_INVALID_INDEX != device_index) {
                pa_source *source;

                if ((source = pa_idxset_get_by_index(u->core->sources, device_index)))
                    if (!pa_source_output_new_data_set_source(new_data, source, false))
                        pa_log_debug("Not restoring device for stream because no supported format was found");
            }
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_put_hook_callback(pa_core *c, PA_GCC_UNUSED pa_sink *sink, struct userdata *u) {
    pa_assert(c);
    pa_assert(u);
    pa_assert(u->core == c);
    pa_assert(u->on_hotplug);

    notify_subscribers(u);

    return route_sink_inputs(u, NULL);
}

static pa_hook_result_t source_put_hook_callback(pa_core *c, PA_GCC_UNUSED pa_source *source, struct userdata *u) {
    pa_assert(c);
    pa_assert(u);
    pa_assert(u->core == c);
    pa_assert(u->on_hotplug);

    notify_subscribers(u);

    return route_source_outputs(u, NULL);
}

static pa_hook_result_t sink_unlink_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->core == c);
    pa_assert(u->on_rescue);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    notify_subscribers(u);

    return route_sink_inputs(u, sink);
}

static pa_hook_result_t source_unlink_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->core == c);
    pa_assert(u->on_rescue);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    notify_subscribers(u);

    return route_source_outputs(u, source);
}

static void apply_entry(struct userdata *u, const char *name, struct entry *e) {
    uint32_t idx;
    char *n;

    pa_assert(u);
    pa_assert(name);
    pa_assert(e);

    if (!e->user_set_description)
        return;

    if ((n = get_name(name, "sink:"))) {
        pa_sink *s;
        PA_IDXSET_FOREACH(s, u->core->sinks, idx) {
            if (!pa_streq(s->name, n)) {
                continue;
            }

            pa_log_info("Setting description for sink %s to '%s'", s->name, e->description);
            pa_sink_set_description(s, e->description);
        }
        pa_xfree(n);
    }
    else if ((n = get_name(name, "source:"))) {
        pa_source *s;
        PA_IDXSET_FOREACH(s, u->core->sources, idx) {
            if (!pa_streq(s->name, n)) {
                continue;
            }

            if (s->monitor_of) {
                pa_log_warn("Cowardly refusing to set the description for monitor source %s.", s->name);
                continue;
            }

            pa_log_info("Setting description for source %s to '%s'", s->name, e->description);
            pa_source_set_description(s, e->description);
        }
        pa_xfree(n);
    }
}

#define EXT_VERSION 1

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

  reply = pa_tagstruct_new(NULL, 0);
  pa_tagstruct_putu32(reply, PA_COMMAND_REPLY);
  pa_tagstruct_putu32(reply, tag);

  switch (command) {
    case SUBCOMMAND_TEST: {
      if (!pa_tagstruct_eof(t))
        goto fail;

      pa_tagstruct_putu32(reply, EXT_VERSION);
      break;
    }

    case SUBCOMMAND_READ: {
      pa_datum key;
      bool done;

      if (!pa_tagstruct_eof(t))
        goto fail;

      done = !pa_database_first(u->database, &key, NULL);

      while (!done) {
        pa_datum next_key;
        struct entry *e;
        char *name;

        done = !pa_database_next(u->database, &key, &next_key, NULL);

        name = pa_xstrndup(key.data, key.size);
        pa_datum_free(&key);

        if ((e = entry_read(u, name))) {
            uint32_t idx;
            char *device_name;
            uint32_t found_index = PA_INVALID_INDEX;

            if ((device_name = get_name(name, "sink:"))) {
                pa_sink* s;
                PA_IDXSET_FOREACH(s, u->core->sinks, idx) {
                    if (pa_streq(s->name, device_name)) {
                        found_index = s->index;
                        break;
                    }
                }
                pa_xfree(device_name);
            } else if ((device_name = get_name(name, "source:"))) {
                pa_source* s;
                PA_IDXSET_FOREACH(s, u->core->sources, idx) {
                    if (pa_streq(s->name, device_name)) {
                        found_index = s->index;
                        break;
                    }
                }
                pa_xfree(device_name);
            }

            pa_tagstruct_puts(reply, name);
            pa_tagstruct_puts(reply, e->description);
            pa_tagstruct_puts(reply, e->icon);
            pa_tagstruct_putu32(reply, found_index);
            pa_tagstruct_putu32(reply, NUM_ROLES);

            for (uint32_t i = ROLE_NONE; i < NUM_ROLES; ++i) {
                pa_tagstruct_puts(reply, role_names[i]);
                pa_tagstruct_putu32(reply, e->priority[i]);
            }

            entry_free(e);
        }

        pa_xfree(name);

        key = next_key;
      }

      break;
    }

    case SUBCOMMAND_RENAME: {

        struct entry *e;
        const char *device, *description;

        if (pa_tagstruct_gets(t, &device) < 0 ||
          pa_tagstruct_gets(t, &description) < 0)
          goto fail;

        if (!device || !*device || !description || !*description)
          goto fail;

        if ((e = entry_read(u, device))) {
            pa_xfree(e->description);
            e->description = pa_xstrdup(description);
            e->user_set_description = true;

            if (entry_write(u, (char *)device, e)) {
                apply_entry(u, device, e);

                trigger_save(u);
            }
            else
                pa_log_warn("Could not save device");

            entry_free(e);
        }
        else
            pa_log_warn("Could not rename device %s, no entry in database", device);

      break;
    }

    case SUBCOMMAND_DELETE:

      while (!pa_tagstruct_eof(t)) {
        const char *name;
        pa_datum key;

        if (pa_tagstruct_gets(t, &name) < 0)
          goto fail;

        key.data = (char*) name;
        key.size = strlen(name);

        /** @todo: Reindex the priorities */
        pa_database_unset(u->database, &key);
      }

      trigger_save(u);

      break;

    case SUBCOMMAND_ROLE_DEVICE_PRIORITY_ROUTING: {

        bool enable;

        if (pa_tagstruct_get_boolean(t, &enable) < 0)
            goto fail;

        if ((u->do_routing = enable)) {
            /* Update our caches */
            update_highest_priority_device_indexes(u, "sink:", NULL);
            update_highest_priority_device_indexes(u, "source:", NULL);
        }

        break;
    }

    case SUBCOMMAND_REORDER: {

        const char *role;
        struct entry *e;
        uint32_t role_index, n_devices;
        pa_datum key;
        bool done, sink_mode = true;
        struct device_t { uint32_t prio; char *device; };
        struct device_t *device;
        struct device_t **devices;
        uint32_t i, idx, offset;
        pa_hashmap *h;
        /*void *state;*/
        bool first;

        if (pa_tagstruct_gets(t, &role) < 0 ||
            pa_tagstruct_getu32(t, &n_devices) < 0 ||
            n_devices < 1)
            goto fail;

        if (PA_INVALID_INDEX == (role_index = get_role_index(role)))
            goto fail;

        /* Cycle through the devices given and make sure they exist */
        h = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
        first = true;
        idx = 0;
        for (i = 0; i < n_devices; ++i) {
            const char *s;
            if (pa_tagstruct_gets(t, &s) < 0) {
                while ((device = pa_hashmap_steal_first(h))) {
                    pa_xfree(device->device);
                    pa_xfree(device);
                }

                pa_hashmap_free(h);
                pa_log_error("Protocol error on reorder");
                goto fail;
            }

            /* Ensure this is a valid entry */
            if (!(e = entry_read(u, s))) {
                while ((device = pa_hashmap_steal_first(h))) {
                    pa_xfree(device->device);
                    pa_xfree(device);
                }

                pa_hashmap_free(h);
                pa_log_error("Client specified an unknown device in its reorder list.");
                goto fail;
            }
            entry_free(e);

            if (first) {
                first = false;
                sink_mode = (0 == strncmp("sink:", s, 5));
            } else if ((sink_mode && 0 != strncmp("sink:", s, 5)) || (!sink_mode && 0 != strncmp("source:", s, 7))) {
                while ((device = pa_hashmap_steal_first(h))) {
                    pa_xfree(device->device);
                    pa_xfree(device);
                }

                pa_hashmap_free(h);
                pa_log_error("Attempted to reorder mixed devices (sinks and sources)");
                goto fail;
            }

            /* Add the device to our hashmap. If it's already in it, free it now and carry on */
            device = pa_xnew(struct device_t, 1);
            device->device = pa_xstrdup(s);
            if (pa_hashmap_put(h, device->device, device) == 0) {
                device->prio = idx;
                idx++;
            } else {
                pa_xfree(device->device);
                pa_xfree(device);
            }
        }

        /*pa_log_debug("Hashmap contents (received from client)");
        PA_HASHMAP_FOREACH(device, h, state) {
            pa_log_debug("  - %s (%d)", device->device, device->prio);
        }*/

        /* Now cycle through our list and add all the devices.
           This has the effect of adding in any in our DB,
           not specified in the device list (and thus will be
           tacked on at the end) */
        offset = idx;
        done = !pa_database_first(u->database, &key, NULL);

        while (!done && idx < 256) {
            pa_datum next_key;

            done = !pa_database_next(u->database, &key, &next_key, NULL);

            device = pa_xnew(struct device_t, 1);
            device->device = pa_xstrndup(key.data, key.size);
            if ((sink_mode && 0 == strncmp("sink:", device->device, 5))
                || (!sink_mode && 0 == strncmp("source:", device->device, 7))) {

                /* Add the device to our hashmap. If it's already in it, free it now and carry on */
                if (pa_hashmap_put(h, device->device, device) == 0
                    && (e = entry_read(u, device->device))) {
                    /* We add offset on to the existing priority so that when we order, the
                       existing entries are always lower priority than the new ones. */
                    device->prio = (offset + e->priority[role_index]);
                    pa_xfree(e);
                }
                else {
                    pa_xfree(device->device);
                    pa_xfree(device);
                }
            } else {
                pa_xfree(device->device);
                pa_xfree(device);
            }

            pa_datum_free(&key);

            key = next_key;
        }

        /*pa_log_debug("Hashmap contents (combined with database)");
        PA_HASHMAP_FOREACH(device, h, state) {
            pa_log_debug("  - %s (%d)", device->device, device->prio);
        }*/

        /* Now we put all the entries in a simple list for sorting it. */
        n_devices = pa_hashmap_size(h);
        devices = pa_xnew(struct device_t *,  n_devices);
        idx = 0;
        while ((device = pa_hashmap_steal_first(h))) {
            devices[idx++] = device;
        }
        pa_hashmap_free(h);

        /* Simple bubble sort */
        for (i = 0; i < n_devices; ++i) {
            for (uint32_t j = i; j < n_devices; ++j) {
                if (devices[i]->prio > devices[j]->prio) {
                    struct device_t *tmp;
                    tmp = devices[i];
                    devices[i] = devices[j];
                    devices[j] = tmp;
                }
            }
        }

        /*pa_log_debug("Sorted device list");
        for (i = 0; i < n_devices; ++i) {
            pa_log_debug("  - %s (%d)", devices[i]->device, devices[i]->prio);
        }*/

        /* Go through in order and write the new entry and cleanup our own list */
        idx = 1;
        first = true;
        for (i = 0; i < n_devices; ++i) {
            if ((e = entry_read(u, devices[i]->device))) {
                if (e->priority[role_index] == idx)
                    idx++;
                else {
                    e->priority[role_index] = idx;

                    if (entry_write(u, (char *) devices[i]->device, e)) {
                        first = false;
                        idx++;
                    }
                }

                pa_xfree(e);
            }
            pa_xfree(devices[i]->device);
            pa_xfree(devices[i]);
        }

        pa_xfree(devices);

        if (!first) {
            trigger_save(u);

            if (sink_mode)
                route_sink_inputs(u, NULL);
            else
                route_source_outputs(u, NULL);
        }

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

struct prioritised_indexes {
    uint32_t index;
    int32_t priority;
};

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    char *fname;
    pa_sink *sink;
    pa_source *source;
    uint32_t idx;
    bool do_routing = false, on_hotplug = true, on_rescue = true;
    uint32_t total_devices;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "do_routing", &do_routing) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_hotplug", &on_hotplug) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_rescue", &on_rescue) < 0) {
        pa_log("on_hotplug= and on_rescue= expect boolean arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->do_routing = do_routing;
    u->on_hotplug = on_hotplug;
    u->on_rescue = on_rescue;
    u->subscribed = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->protocol = pa_native_protocol_get(m->core);
    pa_native_protocol_install_ext(u->protocol, m, extension_cb);

    u->connection_unlink_hook_slot = pa_hook_connect(&pa_native_protocol_hooks(u->protocol)[PA_NATIVE_HOOK_CONNECTION_UNLINK], PA_HOOK_NORMAL, (pa_hook_cb_t) connection_unlink_hook_cb, u);

    u->subscription = pa_subscription_new(m->core, PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SINK_INPUT|PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT, subscribe_callback, u);

    /* Used to handle device description management */
    u->sink_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) sink_new_hook_callback, u);
    u->source_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) source_new_hook_callback, u);

    /* The following slots are used to deal with routing */
    /* A little bit later than module-stream-restore, but before module-intended-roles */
    u->sink_input_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_NEW], PA_HOOK_EARLY+5, (pa_hook_cb_t) sink_input_new_hook_callback, u);
    u->source_output_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_NEW], PA_HOOK_EARLY+5, (pa_hook_cb_t) source_output_new_hook_callback, u);

    if (on_hotplug) {
        /* A little bit later than module-stream-restore, but before module-intended-roles */
        u->sink_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE+5, (pa_hook_cb_t) sink_put_hook_callback, u);
        u->source_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE+5, (pa_hook_cb_t) source_put_hook_callback, u);
    }

    if (on_rescue) {
        /* A little bit later than module-stream-restore, a little bit earlier than module-intended-roles, module-rescue-streams, ... */
        u->sink_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE+5, (pa_hook_cb_t) sink_unlink_hook_callback, u);
        u->source_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE+5, (pa_hook_cb_t) source_unlink_hook_callback, u);
    }

    if (!(fname = pa_state_path("device-manager", true)))
        goto fail;

    if (!(u->database = pa_database_open(fname, true))) {
        pa_log("Failed to open volume database '%s': %s", fname, pa_cstrerror(errno));
        pa_xfree(fname);
        goto fail;
    }

    pa_log_info("Successfully opened database file '%s'.", fname);
    pa_xfree(fname);

    /* Attempt to inject the devices into the list in priority order */
    total_devices = PA_MAX(pa_idxset_size(m->core->sinks), pa_idxset_size(m->core->sources));
    if (total_devices > 0 && total_devices < 128) {
        uint32_t i;
        struct prioritised_indexes p_i[128];

        /* We cycle over all the available sinks so that they are added to our database if they are not in it yet */
        i = 0;
        PA_IDXSET_FOREACH(sink, m->core->sinks, idx) {
            pa_log_debug("Found sink index %u", sink->index);
            p_i[i  ].index = sink->index;
            p_i[i++].priority = sink->priority;
        }
        /* Bubble sort it (only really useful for first time creation) */
        if (i > 1)
          for (uint32_t j = 0; j < i; ++j)
              for (uint32_t k = 0; k < i; ++k)
                  if (p_i[j].priority > p_i[k].priority) {
                      struct prioritised_indexes tmp_pi = p_i[k];
                      p_i[k] = p_i[j];
                      p_i[j] = tmp_pi;
                  }
        /* Register it */
        for (uint32_t j = 0; j < i; ++j)
            subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_NEW, p_i[j].index, u);

        /* We cycle over all the available sources so that they are added to our database if they are not in it yet */
        i = 0;
        PA_IDXSET_FOREACH(source, m->core->sources, idx) {
            p_i[i  ].index = source->index;
            p_i[i++].priority = source->priority;
        }
        /* Bubble sort it (only really useful for first time creation) */
        if (i > 1)
          for (uint32_t j = 0; j < i; ++j)
              for (uint32_t k = 0; k < i; ++k)
                  if (p_i[j].priority > p_i[k].priority) {
                      struct prioritised_indexes tmp_pi = p_i[k];
                      p_i[k] = p_i[j];
                      p_i[j] = tmp_pi;
                  }
        /* Register it */
        for (uint32_t j = 0; j < i; ++j)
            subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_NEW, p_i[j].index, u);
    }
    else if (total_devices > 0) {
        /* This user has a *lot* of devices... */
        PA_IDXSET_FOREACH(sink, m->core->sinks, idx)
            subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_NEW, sink->index, u);

        PA_IDXSET_FOREACH(source, m->core->sources, idx)
            subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_NEW, source->index, u);
    }

    /* Perform the routing (if it's enabled) which will update our priority list cache too */
    for (uint32_t i = 0; i < NUM_ROLES; ++i) {
        u->preferred_sinks[i] = u->preferred_sources[i] = PA_INVALID_INDEX;
    }

    route_sink_inputs(u, NULL);
    route_source_outputs(u, NULL);

#ifdef DUMP_DATABASE
    dump_database(u);
#endif

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

    if (u->sink_new_hook_slot)
        pa_hook_slot_free(u->sink_new_hook_slot);
    if (u->source_new_hook_slot)
        pa_hook_slot_free(u->source_new_hook_slot);

    if (u->sink_input_new_hook_slot)
        pa_hook_slot_free(u->sink_input_new_hook_slot);
    if (u->source_output_new_hook_slot)
        pa_hook_slot_free(u->source_output_new_hook_slot);

    if (u->sink_put_hook_slot)
        pa_hook_slot_free(u->sink_put_hook_slot);
    if (u->source_put_hook_slot)
        pa_hook_slot_free(u->source_put_hook_slot);

    if (u->sink_unlink_hook_slot)
        pa_hook_slot_free(u->sink_unlink_hook_slot);
    if (u->source_unlink_hook_slot)
        pa_hook_slot_free(u->source_unlink_hook_slot);

    if (u->connection_unlink_hook_slot)
        pa_hook_slot_free(u->connection_unlink_hook_slot);

    if (u->save_time_event)
        u->core->mainloop->time_free(u->save_time_event);

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
