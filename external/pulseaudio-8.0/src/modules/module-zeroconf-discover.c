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
#include <string.h>
#include <unistd.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/domain.h>
#include <avahi-common/malloc.h>

#include <pulse/xmalloc.h>

#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/avahi-wrap.h>

#include "module-zeroconf-discover-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("mDNS/DNS-SD Service Discovery");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define SERVICE_TYPE_SINK "_pulse-sink._tcp"
#define SERVICE_TYPE_SOURCE "_non-monitor._sub._pulse-source._tcp"

static const char* const valid_modargs[] = {
    NULL
};

struct tunnel {
    AvahiIfIndex interface;
    AvahiProtocol protocol;
    char *name, *type, *domain;
    uint32_t module_index;
};

struct userdata {
    pa_core *core;
    pa_module *module;
    AvahiPoll *avahi_poll;
    AvahiClient *client;
    AvahiServiceBrowser *source_browser, *sink_browser;

    pa_hashmap *tunnels;
};

static unsigned tunnel_hash(const void *p) {
    const struct tunnel *t = p;

    return
        (unsigned) t->interface +
        (unsigned) t->protocol +
        pa_idxset_string_hash_func(t->name) +
        pa_idxset_string_hash_func(t->type) +
        pa_idxset_string_hash_func(t->domain);
}

static int tunnel_compare(const void *a, const void *b) {
    const struct tunnel *ta = a, *tb = b;
    int r;

    if (ta->interface != tb->interface)
        return 1;
    if (ta->protocol != tb->protocol)
        return 1;
    if ((r = strcmp(ta->name, tb->name)))
        return r;
    if ((r = strcmp(ta->type, tb->type)))
        return r;
    if ((r = strcmp(ta->domain, tb->domain)))
        return r;

    return 0;
}

static struct tunnel *tunnel_new(
        AvahiIfIndex interface, AvahiProtocol protocol,
        const char *name, const char *type, const char *domain) {

    struct tunnel *t;
    t = pa_xnew(struct tunnel, 1);
    t->interface = interface;
    t->protocol = protocol;
    t->name = pa_xstrdup(name);
    t->type = pa_xstrdup(type);
    t->domain = pa_xstrdup(domain);
    t->module_index = PA_IDXSET_INVALID;
    return t;
}

static void tunnel_free(struct tunnel *t) {
    pa_assert(t);
    pa_xfree(t->name);
    pa_xfree(t->type);
    pa_xfree(t->domain);
    pa_xfree(t);
}

static void resolver_cb(
        AvahiServiceResolver *r,
        AvahiIfIndex interface, AvahiProtocol protocol,
        AvahiResolverEvent event,
        const char *name, const char *type, const char *domain,
        const char *host_name, const AvahiAddress *a, uint16_t port,
        AvahiStringList *txt,
        AvahiLookupResultFlags flags,
        void *userdata) {

    struct userdata *u = userdata;
    struct tunnel *tnl;

    pa_assert(u);

    tnl = tunnel_new(interface, protocol, name, type, domain);

    if (event != AVAHI_RESOLVER_FOUND)
        pa_log("Resolving of '%s' failed: %s", name, avahi_strerror(avahi_client_errno(u->client)));
    else {
        char *device = NULL, *dname, *module_name, *args;
        const char *t;
        char *if_suffix = NULL;
        char at[AVAHI_ADDRESS_STR_MAX], cmt[PA_CHANNEL_MAP_SNPRINT_MAX];
        pa_sample_spec ss;
        pa_channel_map cm;
        AvahiStringList *l;
        bool channel_map_set = false;
        pa_module *m;

        ss = u->core->default_sample_spec;
        cm = u->core->default_channel_map;

        for (l = txt; l; l = l->next) {
            char *key, *value;
            pa_assert_se(avahi_string_list_get_pair(l, &key, &value, NULL) == 0);

            if (pa_streq(key, "device")) {
                pa_xfree(device);
                device = value;
                value = NULL;
            } else if (pa_streq(key, "rate"))
                ss.rate = (uint32_t) atoi(value);
            else if (pa_streq(key, "channels"))
                ss.channels = (uint8_t) atoi(value);
            else if (pa_streq(key, "format"))
                ss.format = pa_parse_sample_format(value);
            else if (pa_streq(key, "channel_map")) {
                pa_channel_map_parse(&cm, value);
                channel_map_set = true;
            }

            avahi_free(key);
            avahi_free(value);
        }

        if (!channel_map_set && cm.channels != ss.channels)
            pa_channel_map_init_extend(&cm, ss.channels, PA_CHANNEL_MAP_DEFAULT);

        if (!pa_sample_spec_valid(&ss)) {
            pa_log("Service '%s' contains an invalid sample specification.", name);
            avahi_free(device);
            goto finish;
        }

        if (!pa_channel_map_valid(&cm) || cm.channels != ss.channels) {
            pa_log("Service '%s' contains an invalid channel map.", name);
            avahi_free(device);
            goto finish;
        }

        if (device)
            dname = pa_sprintf_malloc("tunnel.%s.%s", host_name, device);
        else
            dname = pa_sprintf_malloc("tunnel.%s", host_name);

        if (!pa_namereg_is_valid_name(dname)) {
            pa_log("Cannot construct valid device name from credentials of service '%s'.", dname);
            avahi_free(device);
            pa_xfree(dname);
            goto finish;
        }

        t = strstr(type, "sink") ? "sink" : "source";
        if (a->proto == AVAHI_PROTO_INET6 &&
            a->data.ipv6.address[0] == 0xfe &&
            (a->data.ipv6.address[1] & 0xc0) == 0x80)
            if_suffix = pa_sprintf_malloc("%%%d", interface);

        module_name = pa_sprintf_malloc("module-tunnel-%s", t);
        args = pa_sprintf_malloc("server=[%s%s]:%u "
                                 "%s=%s "
                                 "format=%s "
                                 "channels=%u "
                                 "rate=%u "
                                 "%s_name=%s "
                                 "channel_map=%s",
                                 avahi_address_snprint(at, sizeof(at), a),
                                 if_suffix ? if_suffix : "", port,
                                 t, device,
                                 pa_sample_format_to_string(ss.format),
                                 ss.channels,
                                 ss.rate,
                                 t, dname,
                                 pa_channel_map_snprint(cmt, sizeof(cmt), &cm));

        pa_log_debug("Loading %s with arguments '%s'", module_name, args);

        if ((m = pa_module_load(u->core, module_name, args))) {
            tnl->module_index = m->index;
            pa_hashmap_put(u->tunnels, tnl, tnl);
            tnl = NULL;
        }

        pa_xfree(module_name);
        pa_xfree(dname);
        pa_xfree(args);
        pa_xfree(if_suffix);
        avahi_free(device);
    }

finish:

    avahi_service_resolver_free(r);

    if (tnl)
        tunnel_free(tnl);
}

static void browser_cb(
        AvahiServiceBrowser *b,
        AvahiIfIndex interface, AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name, const char *type, const char *domain,
        AvahiLookupResultFlags flags,
        void *userdata) {

    struct userdata *u = userdata;
    struct tunnel *t;

    pa_assert(u);

    if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
        return;

    t = tunnel_new(interface, protocol, name, type, domain);

    if (event == AVAHI_BROWSER_NEW) {

        if (!pa_hashmap_get(u->tunnels, t))
            if (!(avahi_service_resolver_new(u->client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolver_cb, u)))
                pa_log("avahi_service_resolver_new() failed: %s", avahi_strerror(avahi_client_errno(u->client)));

        /* We ignore the returned resolver object here, since the we don't
         * need to attach any special data to it, and we can still destroy
         * it from the callback */

    } else if (event == AVAHI_BROWSER_REMOVE) {
        struct tunnel *t2;

        if ((t2 = pa_hashmap_get(u->tunnels, t))) {
            pa_module_unload_request_by_index(u->core, t2->module_index, true);
            pa_hashmap_remove(u->tunnels, t2);
            tunnel_free(t2);
        }
    }

    tunnel_free(t);
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(c);
    pa_assert(u);

    u->client = c;

    switch (state) {
        case AVAHI_CLIENT_S_REGISTERING:
        case AVAHI_CLIENT_S_RUNNING:
        case AVAHI_CLIENT_S_COLLISION:

            if (!u->sink_browser) {

                if (!(u->sink_browser = avahi_service_browser_new(
                              c,
                              AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                              SERVICE_TYPE_SINK,
                              NULL,
                              0,
                              browser_cb, u))) {

                    pa_log("avahi_service_browser_new() failed: %s", avahi_strerror(avahi_client_errno(c)));
                    pa_module_unload_request(u->module, true);
                }
            }

            if (!u->source_browser) {

                if (!(u->source_browser = avahi_service_browser_new(
                              c,
                              AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                              SERVICE_TYPE_SOURCE,
                              NULL,
                              0,
                              browser_cb, u))) {

                    pa_log("avahi_service_browser_new() failed: %s", avahi_strerror(avahi_client_errno(c)));
                    pa_module_unload_request(u->module, true);
                }
            }

            break;

        case AVAHI_CLIENT_FAILURE:
            if (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) {
                int error;

                pa_log_debug("Avahi daemon disconnected.");

                if (!(u->client = avahi_client_new(u->avahi_poll, AVAHI_CLIENT_NO_FAIL, client_callback, u, &error))) {
                    pa_log("avahi_client_new() failed: %s", avahi_strerror(error));
                    pa_module_unload_request(u->module, true);
                }
            }

            /* Fall through */

        case AVAHI_CLIENT_CONNECTING:

            if (u->sink_browser) {
                avahi_service_browser_free(u->sink_browser);
                u->sink_browser = NULL;
            }

            if (u->source_browser) {
                avahi_service_browser_free(u->source_browser);
                u->source_browser = NULL;
            }

            break;

        default: ;
    }
}

int pa__init(pa_module*m) {

    struct userdata *u;
    pa_modargs *ma = NULL;
    int error;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->sink_browser = u->source_browser = NULL;

    u->tunnels = pa_hashmap_new(tunnel_hash, tunnel_compare);

    u->avahi_poll = pa_avahi_poll_new(m->core->mainloop);

    if (!(u->client = avahi_client_new(u->avahi_poll, AVAHI_CLIENT_NO_FAIL, client_callback, u, &error))) {
        pa_log("pa_avahi_client_new() failed: %s", avahi_strerror(error));
        goto fail;
    }

    pa_modargs_free(ma);

    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata*u;
    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->client)
        avahi_client_free(u->client);

    if (u->avahi_poll)
        pa_avahi_poll_free(u->avahi_poll);

    if (u->tunnels) {
        struct tunnel *t;

        while ((t = pa_hashmap_steal_first(u->tunnels))) {
            pa_module_unload_request_by_index(u->core, t->module_index, true);
            tunnel_free(t);
        }

        pa_hashmap_free(u->tunnels);
    }

    pa_xfree(u);
}
