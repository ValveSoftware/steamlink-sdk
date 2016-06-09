/***
  This file is part of PulseAudio.

  Copyright 2006-2008 Lennart Poettering

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
#include <pulsecore/card.h>
#include <pulsecore/namereg.h>
#include <pulsecore/database.h>
#include <pulsecore/tagstruct.h>

#include "module-card-restore-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Automatically restore profile of cards");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define SAVE_INTERVAL (10 * PA_USEC_PER_SEC)

static const char* const valid_modargs[] = {
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_time_event *save_time_event;
    pa_database *database;
};

#define ENTRY_VERSION 3

struct port_info {
    char *name;
    int64_t offset;
    char *profile;
};

struct entry {
    char *profile;
    pa_hashmap *ports; /* Port name -> struct port_info */
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

static void trigger_save(struct userdata *u) {
    if (u->save_time_event)
        return;

    u->save_time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + SAVE_INTERVAL, save_time_callback, u);
}

static void port_info_free(struct port_info *p_info) {
    pa_assert(p_info);

    pa_xfree(p_info->profile);
    pa_xfree(p_info->name);
    pa_xfree(p_info);
}

static struct entry* entry_new(void) {
    struct entry *r = pa_xnew0(struct entry, 1);
    r->ports = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) port_info_free);
    return r;
}

static struct port_info *port_info_new(pa_device_port *port) {
    struct port_info *p_info;

    if (port) {
        p_info = pa_xnew0(struct port_info, 1);
        p_info->name = pa_xstrdup(port->name);
        p_info->offset = port->latency_offset;
        if (port->preferred_profile)
            p_info->profile = pa_xstrdup(port->preferred_profile);
    } else
        p_info = pa_xnew0(struct port_info, 1);

    return p_info;
}

static void entry_free(struct entry* e) {
    pa_assert(e);

    pa_xfree(e->profile);
    pa_hashmap_free(e->ports);

    pa_xfree(e);
}

static struct entry *entry_from_card(pa_card *card) {
    struct port_info *p_info;
    struct entry *entry;
    pa_device_port *port;
    void *state;

    pa_assert(card);

    entry = entry_new();
    if (card->save_profile)
        entry->profile = pa_xstrdup(card->active_profile->name);

    PA_HASHMAP_FOREACH(port, card->ports, state) {
        p_info = port_info_new(port);
        pa_assert_se(pa_hashmap_put(entry->ports, p_info->name, p_info) >= 0);
    }

    return entry;
}

static bool entrys_equal(struct entry *a, struct entry *b) {
    struct port_info *Ap_info, *Bp_info;
    void *state;

    pa_assert(a);
    pa_assert(b);

    if (!pa_streq(a->profile, b->profile) ||
            pa_hashmap_size(a->ports) != pa_hashmap_size(b->ports))
        return false;

    PA_HASHMAP_FOREACH(Ap_info, a->ports, state) {
        if ((Bp_info = pa_hashmap_get(b->ports, Ap_info->name))) {
            if (Ap_info->offset != Bp_info->offset)
                return false;
        } else
            return false;
    }

    return true;
}

static bool entry_write(struct userdata *u, const char *name, const struct entry *e) {
    pa_tagstruct *t;
    pa_datum key, data;
    bool r;
    void *state;
    struct port_info *p_info;

    pa_assert(u);
    pa_assert(name);
    pa_assert(e);

    t = pa_tagstruct_new();
    pa_tagstruct_putu8(t, ENTRY_VERSION);
    pa_tagstruct_puts(t, e->profile);
    pa_tagstruct_putu32(t, pa_hashmap_size(e->ports));

    PA_HASHMAP_FOREACH(p_info, e->ports, state) {
        pa_tagstruct_puts(t, p_info->name);
        pa_tagstruct_puts64(t, p_info->offset);
        pa_tagstruct_puts(t, p_info->profile);
    }

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
        char profile[PA_NAME_MAX];
    } PA_GCC_PACKED ;
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

    if (!memchr(le->profile, 0, sizeof(le->profile))) {
        pa_log_warn("Profile has missing NUL byte.");
        return NULL;
    }

    e = entry_new();
    e->profile = pa_xstrdup(le->profile);
    return e;
}
#endif

static struct entry* entry_read(struct userdata *u, const char *name) {
    pa_datum key, data;
    struct entry *e = NULL;
    pa_tagstruct *t = NULL;
    const char* profile;
    uint8_t version;

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

    if (pa_tagstruct_getu8(t, &version) < 0 ||
        version > ENTRY_VERSION ||
        pa_tagstruct_gets(t, &profile) < 0) {

        goto fail;
    }

    if (!profile)
        profile = "";

    e->profile = pa_xstrdup(profile);

    if (version >= 2) {
        uint32_t port_count = 0;
        const char *port_name = NULL, *profile_name = NULL;
        int64_t port_offset = 0;
        struct port_info *p_info;
        unsigned i;

        if (pa_tagstruct_getu32(t, &port_count) < 0)
            goto fail;

        for (i = 0; i < port_count; i++) {
            if (pa_tagstruct_gets(t, &port_name) < 0 ||
                !port_name ||
                pa_hashmap_get(e->ports, port_name) ||
                pa_tagstruct_gets64(t, &port_offset) < 0)
                goto fail;
            if (version >= 3 && pa_tagstruct_gets(t, &profile_name) < 0)
                goto fail;

            p_info = port_info_new(NULL);
            p_info->name = pa_xstrdup(port_name);
            p_info->offset = port_offset;
            if (profile_name)
                p_info->profile = pa_xstrdup(profile_name);

            pa_assert_se(pa_hashmap_put(e->ports, p_info->name, p_info) >= 0);
        }
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

static void show_full_info(pa_card *card) {
    pa_assert(card);

    if (card->save_profile)
        pa_log_info("Storing profile and port latency offsets for card %s.", card->name);
    else
        pa_log_info("Storing port latency offsets for card %s.", card->name);
}

static pa_hook_result_t card_put_hook_callback(pa_core *c, pa_card *card, struct userdata *u) {
    struct entry *entry, *old;

    pa_assert(card);

    entry = entry_from_card(card);

    if ((old = entry_read(u, card->name))) {
        if (!card->save_profile)
            entry->profile = pa_xstrdup(old->profile);
        if (entrys_equal(entry, old))
            goto finish;
    }

    show_full_info(card);

    if (entry_write(u, card->name, entry))
        trigger_save(u);

finish:
    entry_free(entry);
    if (old)
        entry_free(old);

    return PA_HOOK_OK;
}

static void update_profile_for_port(struct entry *entry, pa_card *card, pa_device_port *p) {
    struct port_info *p_info;

    if (p == NULL)
        return;

    if (!(p_info = pa_hashmap_get(entry->ports, p->name))) {
        p_info = port_info_new(p);
        pa_assert_se(pa_hashmap_put(entry->ports, p_info->name, p_info) >= 0);
    }

    if (!pa_safe_streq(p_info->profile, p->preferred_profile)) {
        pa_xfree(p_info->profile);
        p_info->profile = pa_xstrdup(p->preferred_profile);
        pa_log_info("Storing profile %s for port %s on card %s.", p_info->profile, p->name, card->name);
    }
}

static pa_hook_result_t card_profile_changed_callback(pa_core *c, pa_card *card, struct userdata *u) {
    struct entry *entry;
    pa_sink *sink;
    pa_source *source;
    uint32_t state;

    pa_assert(card);

    if (!card->save_profile)
        return PA_HOOK_OK;

    if ((entry = entry_read(u, card->name))) {
        pa_xfree(entry->profile);
        entry->profile = pa_xstrdup(card->active_profile->name);
        pa_log_info("Storing card profile for card %s.", card->name);
    } else {
        entry = entry_from_card(card);
        show_full_info(card);
    }

    PA_IDXSET_FOREACH(sink, card->sinks, state)
        update_profile_for_port(entry, card, sink->active_port);
    PA_IDXSET_FOREACH(source, card->sources, state)
        update_profile_for_port(entry, card, source->active_port);

    if (entry_write(u, card->name, entry))
        trigger_save(u);

    entry_free(entry);
    return PA_HOOK_OK;
}

static pa_hook_result_t card_profile_added_callback(pa_core *c, pa_card_profile *profile, struct userdata *u) {
    struct entry *entry;

    pa_assert(profile);

    if (profile->available == PA_AVAILABLE_NO)
        return PA_HOOK_OK;

    if (!(entry = entry_read(u, profile->card->name)))
        return PA_HOOK_OK;

    if (pa_safe_streq(entry->profile, profile->name)) {
        if (pa_card_set_profile(profile->card, profile, true) >= 0)
            pa_log_info("Restored profile '%s' for card %s.", profile->name, profile->card->name);
    }

    entry_free(entry);

    return PA_HOOK_OK;
}

static pa_hook_result_t profile_available_changed_callback(void *hook_data, void *call_data, void *userdata) {
    pa_card_profile *profile = call_data;
    pa_card *card;
    struct userdata *u = userdata;
    struct entry *entry;

    pa_assert(profile);
    pa_assert(u);

    card = profile->card;

    if (profile->available == PA_AVAILABLE_NO)
        return PA_HOOK_OK;

    entry = entry_read(u, card->name);
    if (!entry)
        return PA_HOOK_OK;

    if (!pa_streq(profile->name, entry->profile)) {
        entry_free(entry);
        return PA_HOOK_OK;
    }

    pa_log_info("Card %s profile %s became available, activating.", card->name, profile->name);
    pa_card_set_profile(profile->card, profile, true);

    entry_free(entry);
    return PA_HOOK_OK;
}

static pa_hook_result_t port_offset_change_callback(pa_core *c, pa_device_port *port, struct userdata *u) {
    struct entry *entry;
    pa_card *card;

    pa_assert(port);
    card = port->card;

    if ((entry = entry_read(u, card->name))) {
        struct port_info *p_info;

        if ((p_info = pa_hashmap_get(entry->ports, port->name)))
            p_info->offset = port->latency_offset;
        else {
            p_info = port_info_new(port);
            pa_assert_se(pa_hashmap_put(entry->ports, p_info->name, p_info) >= 0);
        }

        pa_log_info("Storing latency offset for port %s on card %s.", port->name, card->name);

    } else {
        entry = entry_from_card(card);
        show_full_info(card);
    }

    if (entry_write(u, card->name, entry))
        trigger_save(u);

    entry_free(entry);
    return PA_HOOK_OK;
}

static pa_hook_result_t card_new_hook_callback(pa_core *c, pa_card_new_data *new_data, struct userdata *u) {
    struct entry *e;
    void *state;
    pa_device_port *p;
    struct port_info *p_info;

    pa_assert(new_data);

    if (!(e = entry_read(u, new_data->name)))
        return PA_HOOK_OK;

    if (e->profile[0]) {
        if (!new_data->active_profile) {
            pa_card_new_data_set_profile(new_data, e->profile);
            pa_log_info("Restored profile '%s' for card %s.", new_data->active_profile, new_data->name);
            new_data->save_profile = true;

        } else
            pa_log_debug("Not restoring profile for card %s, because already set.", new_data->name);
    }

    /* Always restore the latency offsets because their
     * initial value is always 0 */

    pa_log_info("Restoring port latency offsets for card %s.", new_data->name);

    PA_HASHMAP_FOREACH(p_info, e->ports, state)
        if ((p = pa_hashmap_get(new_data->ports, p_info->name))) {
            p->latency_offset = p_info->offset;
            if (!p->preferred_profile && p_info->profile)
                pa_device_port_set_preferred_profile(p, p_info->profile);
        }
    entry_free(e);

    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    char *fname;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;

    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_CARD_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) card_new_hook_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_CARD_PUT], PA_HOOK_NORMAL, (pa_hook_cb_t) card_put_hook_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_CHANGED], PA_HOOK_NORMAL, (pa_hook_cb_t) card_profile_changed_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_ADDED], PA_HOOK_NORMAL, (pa_hook_cb_t) card_profile_added_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_CARD_PROFILE_AVAILABLE_CHANGED],
                                                             PA_HOOK_NORMAL, profile_available_changed_callback, u);
    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_PORT_LATENCY_OFFSET_CHANGED], PA_HOOK_NORMAL, (pa_hook_cb_t) port_offset_change_callback, u);

    if (!(fname = pa_state_path("card-database", true)))
        goto fail;

    if (!(u->database = pa_database_open(fname, true))) {
        pa_log("Failed to open volume database '%s': %s", fname, pa_cstrerror(errno));
        pa_xfree(fname);
        goto fail;
    }

    pa_log_info("Successfully opened database file '%s'.", fname);
    pa_xfree(fname);

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

    if (u->save_time_event)
        u->core->mainloop->time_free(u->save_time_event);

    if (u->database)
        pa_database_close(u->database);

    pa_xfree(u);
}
