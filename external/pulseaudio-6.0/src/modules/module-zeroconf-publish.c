/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>

#include <pulse/xmalloc.h>
#include <pulse/util.h>
#include <pulse/thread-mainloop.h>

#include <pulsecore/parseaddr.h>
#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/native-common.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/dynarray.h>
#include <pulsecore/modargs.h>
#include <pulsecore/avahi-wrap.h>
#include <pulsecore/protocol-native.h>

#include "module-zeroconf-publish-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("mDNS/DNS-SD Service Publisher");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define SERVICE_TYPE_SINK "_pulse-sink._tcp"
#define SERVICE_TYPE_SOURCE "_pulse-source._tcp"
#define SERVICE_TYPE_SERVER "_pulse-server._tcp"
#define SERVICE_SUBTYPE_SINK_HARDWARE "_hardware._sub."SERVICE_TYPE_SINK
#define SERVICE_SUBTYPE_SINK_VIRTUAL "_virtual._sub."SERVICE_TYPE_SINK
#define SERVICE_SUBTYPE_SOURCE_HARDWARE "_hardware._sub."SERVICE_TYPE_SOURCE
#define SERVICE_SUBTYPE_SOURCE_VIRTUAL "_virtual._sub."SERVICE_TYPE_SOURCE
#define SERVICE_SUBTYPE_SOURCE_MONITOR "_monitor._sub."SERVICE_TYPE_SOURCE
#define SERVICE_SUBTYPE_SOURCE_NON_MONITOR "_non-monitor._sub."SERVICE_TYPE_SOURCE

/*
 * Note: Because the core avahi-client calls result in synchronous D-Bus
 * communication, calling any of those functions in the PA mainloop context
 * could lead to the mainloop being blocked for long periods.
 *
 * To avoid this, we create a threaded-mainloop for Avahi calls, and push all
 * D-Bus communication into that thread. The thumb-rule for the split is:
 *
 * 1. If access to PA data structures is needed, use the PA mainloop context
 *
 * 2. If a (blocking) avahi-client call is needed, use the Avahi mainloop
 *
 * We do have message queue to pass messages from the Avahi mainloop to the PA
 * mainloop.
 */

static const char* const valid_modargs[] = {
    NULL
};

struct avahi_msg {
    pa_msgobject parent;
};

typedef struct avahi_msg avahi_msg;
PA_DEFINE_PRIVATE_CLASS(avahi_msg, pa_msgobject);

enum {
    AVAHI_MESSAGE_PUBLISH_ALL,
    AVAHI_MESSAGE_SHUTDOWN_START,
    AVAHI_MESSAGE_SHUTDOWN_COMPLETE,
};

enum service_subtype {
    SUBTYPE_HARDWARE,
    SUBTYPE_VIRTUAL,
    SUBTYPE_MONITOR
};

struct service {
    void *key;

    struct userdata *userdata;
    AvahiEntryGroup *entry_group;
    char *service_name;
    const char *service_type;
    enum service_subtype subtype;

    char *name;
    bool is_sink;
    pa_sample_spec ss;
    pa_channel_map cm;
    pa_proplist *proplist;
};

struct userdata {
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;
    avahi_msg *msg;

    pa_core *core;
    pa_module *module;
    pa_mainloop_api *api;
    pa_threaded_mainloop *mainloop;

    AvahiPoll *avahi_poll;
    AvahiClient *client;

    pa_hashmap *services; /* protect with mainloop lock */
    char *service_name;

    AvahiEntryGroup *main_entry_group;

    pa_hook_slot *sink_new_slot, *source_new_slot, *sink_unlink_slot, *source_unlink_slot, *sink_changed_slot, *source_changed_slot;

    pa_native_protocol *native;

    bool shutting_down; /* Used in the main thread. */
    bool client_freed; /* Used in the Avahi thread. */
};

/* Runs in PA mainloop context */
static void get_service_data(struct service *s, pa_object *device) {
    pa_assert(s);

    if (pa_sink_isinstance(device)) {
        pa_sink *sink = PA_SINK(device);

        s->is_sink = true;
        s->service_type = SERVICE_TYPE_SINK;
        s->ss = sink->sample_spec;
        s->cm = sink->channel_map;
        s->name = pa_xstrdup(sink->name);
        s->proplist = pa_proplist_copy(sink->proplist);
        s->subtype = sink->flags & PA_SINK_HARDWARE ? SUBTYPE_HARDWARE : SUBTYPE_VIRTUAL;

    } else if (pa_source_isinstance(device)) {
        pa_source *source = PA_SOURCE(device);

        s->is_sink = false;
        s->service_type = SERVICE_TYPE_SOURCE;
        s->ss = source->sample_spec;
        s->cm = source->channel_map;
        s->name = pa_xstrdup(source->name);
        s->proplist = pa_proplist_copy(source->proplist);
        s->subtype = source->monitor_of ? SUBTYPE_MONITOR : (source->flags & PA_SOURCE_HARDWARE ? SUBTYPE_HARDWARE : SUBTYPE_VIRTUAL);

    } else
        pa_assert_not_reached();
}

/* Can be used in either PA or Avahi mainloop context since the bits of u->core
 * that we access don't change after startup. */
static AvahiStringList* txt_record_server_data(pa_core *c, AvahiStringList *l) {
    char s[128];
    char *t;

    pa_assert(c);

    l = avahi_string_list_add_pair(l, "server-version", PACKAGE_NAME" "PACKAGE_VERSION);

    t = pa_get_user_name_malloc();
    l = avahi_string_list_add_pair(l, "user-name", t);
    pa_xfree(t);

    t = pa_machine_id();
    l = avahi_string_list_add_pair(l, "machine-id", t);
    pa_xfree(t);

    t = pa_uname_string();
    l = avahi_string_list_add_pair(l, "uname", t);
    pa_xfree(t);

    l = avahi_string_list_add_pair(l, "fqdn", pa_get_fqdn(s, sizeof(s)));
    l = avahi_string_list_add_printf(l, "cookie=0x%08x", c->cookie);

    return l;
}

static void publish_service(pa_mainloop_api *api, void *service);

/* Runs in Avahi mainloop context */
static void service_entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    struct service *s = userdata;

    pa_assert(s);

    switch (state) {

        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            pa_log_info("Successfully established service %s.", s->service_name);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION: {
            char *t;

            t = avahi_alternative_service_name(s->service_name);
            pa_log_info("Name collision, renaming %s to %s.", s->service_name, t);
            pa_xfree(s->service_name);
            s->service_name = t;

            publish_service(NULL, s);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE: {
            pa_log("Failed to register service: %s", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

            avahi_entry_group_free(g);
            s->entry_group = NULL;

            break;
        }

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            ;
    }
}

static void service_free(struct service *s);

/* Can run in either context */
static uint16_t compute_port(struct userdata *u) {
    pa_strlist *i;

    pa_assert(u);

    for (i = pa_native_protocol_servers(u->native); i; i = pa_strlist_next(i)) {
        pa_parsed_address a;

        if (pa_parse_address(pa_strlist_data(i), &a) >= 0 &&
            (a.type == PA_PARSED_ADDRESS_TCP4 ||
             a.type == PA_PARSED_ADDRESS_TCP6 ||
             a.type == PA_PARSED_ADDRESS_TCP_AUTO) &&
            a.port > 0) {

            pa_xfree(a.path_or_host);
            return a.port;
        }

        pa_xfree(a.path_or_host);
    }

    return PA_NATIVE_DEFAULT_PORT;
}

/* Runs in Avahi mainloop context */
static void publish_service(pa_mainloop_api *api PA_GCC_UNUSED, void *service) {
    struct service *s = (struct service *) service;
    int r = -1;
    AvahiStringList *txt = NULL;
    char cm[PA_CHANNEL_MAP_SNPRINT_MAX];
    const char *t;

    const char * const subtype_text[] = {
        [SUBTYPE_HARDWARE] = "hardware",
        [SUBTYPE_VIRTUAL] = "virtual",
        [SUBTYPE_MONITOR] = "monitor"
    };

    pa_assert(s);

    if (!s->userdata->client || avahi_client_get_state(s->userdata->client) != AVAHI_CLIENT_S_RUNNING)
        return;

    if (!s->entry_group) {
        if (!(s->entry_group = avahi_entry_group_new(s->userdata->client, service_entry_group_callback, s))) {
            pa_log("avahi_entry_group_new(): %s", avahi_strerror(avahi_client_errno(s->userdata->client)));
            goto finish;
        }
    } else
        avahi_entry_group_reset(s->entry_group);

    txt = txt_record_server_data(s->userdata->core, txt);

    txt = avahi_string_list_add_pair(txt, "device", s->name);
    txt = avahi_string_list_add_printf(txt, "rate=%u", s->ss.rate);
    txt = avahi_string_list_add_printf(txt, "channels=%u", s->ss.channels);
    txt = avahi_string_list_add_pair(txt, "format", pa_sample_format_to_string(s->ss.format));
    txt = avahi_string_list_add_pair(txt, "channel_map", pa_channel_map_snprint(cm, sizeof(cm), &s->cm));
    txt = avahi_string_list_add_pair(txt, "subtype", subtype_text[s->subtype]);

    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_DESCRIPTION)))
        txt = avahi_string_list_add_pair(txt, "description", t);
    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_ICON_NAME)))
        txt = avahi_string_list_add_pair(txt, "icon-name", t);
    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_VENDOR_NAME)))
        txt = avahi_string_list_add_pair(txt, "vendor-name", t);
    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_PRODUCT_NAME)))
        txt = avahi_string_list_add_pair(txt, "product-name", t);
    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_CLASS)))
        txt = avahi_string_list_add_pair(txt, "class", t);
    if ((t = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_FORM_FACTOR)))
        txt = avahi_string_list_add_pair(txt, "form-factor", t);

    if (avahi_entry_group_add_service_strlst(
                s->entry_group,
                AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                0,
                s->service_name,
                s->service_type,
                NULL,
                NULL,
                compute_port(s->userdata),
                txt) < 0) {

        pa_log("avahi_entry_group_add_service_strlst(): %s", avahi_strerror(avahi_client_errno(s->userdata->client)));
        goto finish;
    }

    if (avahi_entry_group_add_service_subtype(
                s->entry_group,
                AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                0,
                s->service_name,
                s->service_type,
                NULL,
                s->is_sink ? (s->subtype == SUBTYPE_HARDWARE ? SERVICE_SUBTYPE_SINK_HARDWARE : SERVICE_SUBTYPE_SINK_VIRTUAL) :
                (s->subtype == SUBTYPE_HARDWARE ? SERVICE_SUBTYPE_SOURCE_HARDWARE : (s->subtype == SUBTYPE_VIRTUAL ? SERVICE_SUBTYPE_SOURCE_VIRTUAL : SERVICE_SUBTYPE_SOURCE_MONITOR))) < 0) {

        pa_log("avahi_entry_group_add_service_subtype(): %s", avahi_strerror(avahi_client_errno(s->userdata->client)));
        goto finish;
    }

    if (!s->is_sink && s->subtype != SUBTYPE_MONITOR) {
        if (avahi_entry_group_add_service_subtype(
                    s->entry_group,
                    AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                    0,
                    s->service_name,
                    SERVICE_TYPE_SOURCE,
                    NULL,
                    SERVICE_SUBTYPE_SOURCE_NON_MONITOR) < 0) {

            pa_log("avahi_entry_group_add_service_subtype(): %s", avahi_strerror(avahi_client_errno(s->userdata->client)));
            goto finish;
        }
    }

    if (avahi_entry_group_commit(s->entry_group) < 0) {
        pa_log("avahi_entry_group_commit(): %s", avahi_strerror(avahi_client_errno(s->userdata->client)));
        goto finish;
    }

    r = 0;
    pa_log_debug("Successfully created entry group for %s.", s->service_name);

finish:

    /* Remove this service */
    if (r < 0)
        pa_hashmap_remove_and_free(s->userdata->services, s->key);

    avahi_string_list_free(txt);
}

/* Runs in PA mainloop context */
static struct service *get_service(struct userdata *u, pa_object *device) {
    struct service *s;
    char *hn, *un;
    const char *n;

    pa_assert(u);
    pa_object_assert_ref(device);

    pa_threaded_mainloop_lock(u->mainloop);

    if ((s = pa_hashmap_get(u->services, device)))
        goto out;

    s = pa_xnew(struct service, 1);
    s->key = device;
    s->userdata = u;
    s->entry_group = NULL;

    get_service_data(s, device);

    if (!(n = pa_proplist_gets(s->proplist, PA_PROP_DEVICE_DESCRIPTION)))
        n = s->name;

    hn = pa_get_host_name_malloc();
    un = pa_get_user_name_malloc();

    s->service_name = pa_truncate_utf8(pa_sprintf_malloc("%s@%s: %s", un, hn, n), AVAHI_LABEL_MAX-1);

    pa_xfree(un);
    pa_xfree(hn);

    pa_hashmap_put(u->services, device, s);

out:
    pa_threaded_mainloop_unlock(u->mainloop);

    return s;
}

/* Run from Avahi mainloop context */
static void service_free(struct service *s) {
    pa_assert(s);

    if (s->entry_group) {
        pa_log_debug("Removing entry group for %s.", s->service_name);
        avahi_entry_group_free(s->entry_group);
    }

    pa_xfree(s->service_name);

    pa_xfree(s->name);
    pa_proplist_free(s->proplist);

    pa_xfree(s);
}

/* Runs in PA mainloop context */
static bool shall_ignore(pa_object *o) {
    pa_object_assert_ref(o);

    if (pa_sink_isinstance(o))
        return !!(PA_SINK(o)->flags & PA_SINK_NETWORK);

    if (pa_source_isinstance(o))
        return PA_SOURCE(o)->monitor_of || (PA_SOURCE(o)->flags & PA_SOURCE_NETWORK);

    pa_assert_not_reached();
}

/* Runs in PA mainloop context */
static pa_hook_result_t device_new_or_changed_cb(pa_core *c, pa_object *o, struct userdata *u) {
    pa_assert(c);
    pa_object_assert_ref(o);

    if (!shall_ignore(o)) {
        pa_threaded_mainloop_lock(u->mainloop);
        pa_mainloop_api_once(u->api, publish_service, get_service(u, o));
        pa_threaded_mainloop_unlock(u->mainloop);
    }

    return PA_HOOK_OK;
}

/* Runs in PA mainloop context */
static pa_hook_result_t device_unlink_cb(pa_core *c, pa_object *o, struct userdata *u) {
    pa_assert(c);
    pa_object_assert_ref(o);

    pa_threaded_mainloop_lock(u->mainloop);
    pa_hashmap_remove_and_free(u->services, o);
    pa_threaded_mainloop_unlock(u->mainloop);

    return PA_HOOK_OK;
}

static int publish_main_service(struct userdata *u);

/* Runs in Avahi mainloop context */
static void main_entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    struct userdata *u = userdata;
    pa_assert(u);

    switch (state) {

        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            pa_log_info("Successfully established main service.");
            break;

        case AVAHI_ENTRY_GROUP_COLLISION: {
            char *t;

            t = avahi_alternative_service_name(u->service_name);
            pa_log_info("Name collision: renaming main service %s to %s.", u->service_name, t);
            pa_xfree(u->service_name);
            u->service_name = t;

            publish_main_service(u);
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE: {
            pa_log("Failed to register main service: %s", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

            avahi_entry_group_free(g);
            u->main_entry_group = NULL;
            break;
        }

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
    }
}

/* Runs in Avahi mainloop context */
static int publish_main_service(struct userdata *u) {
    AvahiStringList *txt = NULL;
    int r = -1;

    pa_assert(u);

    if (!u->main_entry_group) {
        if (!(u->main_entry_group = avahi_entry_group_new(u->client, main_entry_group_callback, u))) {
            pa_log("avahi_entry_group_new() failed: %s", avahi_strerror(avahi_client_errno(u->client)));
            goto fail;
        }
    } else
        avahi_entry_group_reset(u->main_entry_group);

    txt = txt_record_server_data(u->core, txt);

    if (avahi_entry_group_add_service_strlst(
                u->main_entry_group,
                AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                0,
                u->service_name,
                SERVICE_TYPE_SERVER,
                NULL,
                NULL,
                compute_port(u),
                txt) < 0) {

        pa_log("avahi_entry_group_add_service_strlst() failed: %s", avahi_strerror(avahi_client_errno(u->client)));
        goto fail;
    }

    if (avahi_entry_group_commit(u->main_entry_group) < 0) {
        pa_log("avahi_entry_group_commit() failed: %s", avahi_strerror(avahi_client_errno(u->client)));
        goto fail;
    }

    r = 0;

fail:
    avahi_string_list_free(txt);

    return r;
}

/* Runs in PA mainloop context */
static int publish_all_services(struct userdata *u) {
    pa_sink *sink;
    pa_source *source;
    int r = -1;
    uint32_t idx;

    pa_assert(u);

    pa_log_debug("Publishing services in Zeroconf");

    for (sink = PA_SINK(pa_idxset_first(u->core->sinks, &idx)); sink; sink = PA_SINK(pa_idxset_next(u->core->sinks, &idx)))
        if (!shall_ignore(PA_OBJECT(sink))) {
            pa_threaded_mainloop_lock(u->mainloop);
            pa_mainloop_api_once(u->api, publish_service, get_service(u, PA_OBJECT(sink)));
            pa_threaded_mainloop_unlock(u->mainloop);
        }

    for (source = PA_SOURCE(pa_idxset_first(u->core->sources, &idx)); source; source = PA_SOURCE(pa_idxset_next(u->core->sources, &idx)))
        if (!shall_ignore(PA_OBJECT(source))) {
            pa_threaded_mainloop_lock(u->mainloop);
            pa_mainloop_api_once(u->api, publish_service, get_service(u, PA_OBJECT(source)));
            pa_threaded_mainloop_unlock(u->mainloop);
        }

    if (publish_main_service(u) < 0)
        goto fail;

    r = 0;

fail:
    return r;
}

/* Runs in Avahi mainloop context */
static void unpublish_all_services(struct userdata *u, bool rem) {
    void *state = NULL;
    struct service *s;

    pa_assert(u);

    pa_log_debug("Unpublishing services in Zeroconf");

    while ((s = pa_hashmap_iterate(u->services, &state, NULL))) {
        if (s->entry_group) {
            if (rem) {
                pa_log_debug("Removing entry group for %s.", s->service_name);
                avahi_entry_group_free(s->entry_group);
                s->entry_group = NULL;
            } else {
                avahi_entry_group_reset(s->entry_group);
                pa_log_debug("Resetting entry group for %s.", s->service_name);
            }
        }
    }

    if (u->main_entry_group) {
        if (rem) {
            pa_log_debug("Removing main entry group.");
            avahi_entry_group_free(u->main_entry_group);
            u->main_entry_group = NULL;
        } else {
            avahi_entry_group_reset(u->main_entry_group);
            pa_log_debug("Resetting main entry group.");
        }
    }
}

/* Runs in PA mainloop context */
static int avahi_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = (struct userdata *) data;

    pa_assert(u);

    if (u->shutting_down)
        return 0;

    switch (code) {
        case AVAHI_MESSAGE_PUBLISH_ALL:
            publish_all_services(u);
            break;

        case AVAHI_MESSAGE_SHUTDOWN_START:
            pa_module_unload(u->core, u->module, true);
            break;

        default:
            pa_assert_not_reached();
    }

    return 0;
}

/* Runs in Avahi mainloop context */
static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(u);

    u->client = c;

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            /* Collect all sinks/sources, and publish them */
            pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->msg), AVAHI_MESSAGE_PUBLISH_ALL, u, 0, NULL, NULL);
            break;

        case AVAHI_CLIENT_S_COLLISION:
            pa_log_debug("Host name collision");
            unpublish_all_services(u, false);
            break;

        case AVAHI_CLIENT_FAILURE:
            if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) {
                int error;

                pa_log_debug("Avahi daemon disconnected.");

                unpublish_all_services(u, true);
                avahi_client_free(u->client);

                if (!(u->client = avahi_client_new(u->avahi_poll, AVAHI_CLIENT_NO_FAIL, client_callback, u, &error))) {
                    pa_log("avahi_client_new() failed: %s", avahi_strerror(error));
                    pa_module_unload_request(u->module, true);
                }
            }

            break;

        default: ;
    }
}

/* Runs in Avahi mainloop context */
static void create_client(pa_mainloop_api *api PA_GCC_UNUSED, void *userdata) {
    struct userdata *u = (struct userdata *) userdata;
    int error;

    /* create_client() and client_free() are called via defer events. If the
     * two defer events are created very quickly one after another, we can't
     * assume that the defer event that runs create_client() will be dispatched
     * before the defer event that runs client_free() (at the time of writing,
     * pa_mainloop actually always dispatches queued defer events in reverse
     * creation order). For that reason we must be prepared for the case where
     * client_free() has already been called. */
    if (u->client_freed)
        return;

    pa_thread_mq_install(&u->thread_mq);

    if (!(u->client = avahi_client_new(u->avahi_poll, AVAHI_CLIENT_NO_FAIL, client_callback, u, &error))) {
        pa_log("avahi_client_new() failed: %s", avahi_strerror(error));
        goto fail;
    }

    pa_log_debug("Started Avahi threaded mainloop");

    return;

fail:
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->msg), AVAHI_MESSAGE_SHUTDOWN_START, u, 0, NULL, NULL);
}

int pa__init(pa_module*m) {

    struct userdata *u;
    pa_modargs *ma = NULL;
    char *hn, *un;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->native = pa_native_protocol_get(u->core);

    u->rtpoll = pa_rtpoll_new();
    u->mainloop = pa_threaded_mainloop_new();
    u->api = pa_threaded_mainloop_get_api(u->mainloop);

    pa_thread_mq_init(&u->thread_mq, u->core->mainloop, u->rtpoll);
    u->msg = pa_msgobject_new(avahi_msg);
    u->msg->parent.process_msg = avahi_process_msg;

    u->avahi_poll = pa_avahi_poll_new(u->api);

    u->services = pa_hashmap_new_full(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func, NULL, (pa_free_cb_t) service_free);

    u->sink_new_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->sink_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PROPLIST_CHANGED], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->sink_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) device_unlink_cb, u);
    u->source_new_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->source_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PROPLIST_CHANGED], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->source_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) device_unlink_cb, u);

    un = pa_get_user_name_malloc();
    hn = pa_get_host_name_malloc();
    u->service_name = pa_truncate_utf8(pa_sprintf_malloc("%s@%s", un, hn), AVAHI_LABEL_MAX-1);
    pa_xfree(un);
    pa_xfree(hn);

    pa_threaded_mainloop_set_name(u->mainloop, "avahi-ml");
    pa_threaded_mainloop_start(u->mainloop);

    pa_threaded_mainloop_lock(u->mainloop);
    pa_mainloop_api_once(u->api, create_client, u);
    pa_threaded_mainloop_unlock(u->mainloop);

    pa_modargs_free(ma);

    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

/* Runs in Avahi mainloop context */
static void client_free(pa_mainloop_api *api PA_GCC_UNUSED, void *userdata) {
    struct userdata *u = (struct userdata *) userdata;

    pa_hashmap_free(u->services);

    if (u->main_entry_group)
        avahi_entry_group_free(u->main_entry_group);

    if (u->client)
        avahi_client_free(u->client);

    if (u->avahi_poll)
        pa_avahi_poll_free(u->avahi_poll);

    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->msg), AVAHI_MESSAGE_SHUTDOWN_COMPLETE, u, 0, NULL, NULL);

    u->client_freed = true;
}

void pa__done(pa_module*m) {
    struct userdata*u;
    pa_assert(m);

    if (!(u = m->userdata))
        return;

    u->shutting_down = true;

    pa_threaded_mainloop_lock(u->mainloop);
    pa_mainloop_api_once(u->api, client_free, u);
    pa_threaded_mainloop_unlock(u->mainloop);
    pa_asyncmsgq_wait_for(u->thread_mq.outq, AVAHI_MESSAGE_SHUTDOWN_COMPLETE);

    pa_threaded_mainloop_stop(u->mainloop);
    pa_threaded_mainloop_free(u->mainloop);

    pa_thread_mq_done(&u->thread_mq);
    pa_rtpoll_free(u->rtpoll);

    if (u->sink_new_slot)
        pa_hook_slot_free(u->sink_new_slot);
    if (u->source_new_slot)
        pa_hook_slot_free(u->source_new_slot);
    if (u->sink_changed_slot)
        pa_hook_slot_free(u->sink_changed_slot);
    if (u->source_changed_slot)
        pa_hook_slot_free(u->source_changed_slot);
    if (u->sink_unlink_slot)
        pa_hook_slot_free(u->sink_unlink_slot);
    if (u->source_unlink_slot)
        pa_hook_slot_free(u->source_unlink_slot);

    if (u->native)
        pa_native_protocol_unref(u->native);

    pa_xfree(u->msg);
    pa_xfree(u->service_name);
    pa_xfree(u);
}
