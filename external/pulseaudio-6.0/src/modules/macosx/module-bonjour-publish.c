/***
  This file is part of PulseAudio.

  Copyright 2009 Daniel Mack
  based on module-zeroconf-publish.c

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
#include <dns_sd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <pulse/xmalloc.h>
#include <pulse/util.h>

#include <pulsecore/parseaddr.h>
#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/native-common.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/dynarray.h>
#include <pulsecore/modargs.h>
#include <pulsecore/protocol-native.h>

#include "module-bonjour-publish-symdef.h"

PA_MODULE_AUTHOR("Daniel Mack");
PA_MODULE_DESCRIPTION("Mac OS X Bonjour Service Publisher");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

#define SERVICE_TYPE_SINK "_pulse-sink._tcp"
#define SERVICE_TYPE_SOURCE "_pulse-source._tcp"
#define SERVICE_TYPE_SERVER "_pulse-server._tcp"

static const char* const valid_modargs[] = {
    NULL
};

enum service_subtype {
    SUBTYPE_HARDWARE,
    SUBTYPE_VIRTUAL,
    SUBTYPE_MONITOR
};

struct service {
    struct userdata *userdata;
    DNSServiceRef service;
    DNSRecordRef rec, rec2;
    char *service_name;
    pa_object *device;
    enum service_subtype subtype;
};

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_hashmap *services;
    char *service_name;

    pa_hook_slot *sink_new_slot, *source_new_slot, *sink_unlink_slot, *source_unlink_slot, *sink_changed_slot, *source_changed_slot;

    pa_native_protocol *native;
    DNSServiceRef main_service;
};

static void get_service_data(struct service *s, pa_sample_spec *ret_ss, pa_channel_map *ret_map, const char **ret_name, pa_proplist **ret_proplist, enum service_subtype *ret_subtype) {
    pa_assert(s);
    pa_assert(ret_ss);
    pa_assert(ret_proplist);
    pa_assert(ret_subtype);

    if (pa_sink_isinstance(s->device)) {
        pa_sink *sink = PA_SINK(s->device);

        *ret_ss = sink->sample_spec;
        *ret_map = sink->channel_map;
        *ret_name = sink->name;
        *ret_proplist = sink->proplist;
        *ret_subtype = sink->flags & PA_SINK_HARDWARE ? SUBTYPE_HARDWARE : SUBTYPE_VIRTUAL;

    } else if (pa_source_isinstance(s->device)) {
        pa_source *source = PA_SOURCE(s->device);

        *ret_ss = source->sample_spec;
        *ret_map = source->channel_map;
        *ret_name = source->name;
        *ret_proplist = source->proplist;
        *ret_subtype = source->monitor_of ? SUBTYPE_MONITOR : (source->flags & PA_SOURCE_HARDWARE ? SUBTYPE_HARDWARE : SUBTYPE_VIRTUAL);

    } else
        pa_assert_not_reached();
}

static void txt_record_server_data(pa_core *c, TXTRecordRef *txt) {
    char s[128];
    char *t;

    pa_assert(c);

    TXTRecordSetValue(txt, "server-version", strlen(PACKAGE_NAME" "PACKAGE_VERSION), PACKAGE_NAME" "PACKAGE_VERSION);

    t = pa_get_user_name_malloc();
    TXTRecordSetValue(txt, "user-name", strlen(t), t);
    pa_xfree(t);

    t = pa_machine_id();
    TXTRecordSetValue(txt, "machine-id", strlen(t), t);
    pa_xfree(t);

    t = pa_uname_string();
    TXTRecordSetValue(txt, "uname", strlen(t), t);
    pa_xfree(t);

    t = pa_get_fqdn(s, sizeof(s));
    TXTRecordSetValue(txt, "fqdn", strlen(t), t);

    snprintf(s, sizeof(s), "0x%08x", c->cookie);
    TXTRecordSetValue(txt, "cookie", strlen(s), s);
}

static void service_free(struct service *s);

static void dns_service_register_reply(DNSServiceRef sdRef,
                                       DNSServiceFlags flags,
                                       DNSServiceErrorType errorCode,
                                       const char *name,
                                       const char *regtype,
                                       const char *domain,
                                       void *context) {
    struct service *s = context;

    pa_assert(s);

    switch (errorCode) {
    case kDNSServiceErr_NameConflict:
        pa_log("DNS service reported kDNSServiceErr_NameConflict\n");
        service_free(s);
        break;

    case kDNSServiceErr_NoError:
    default:
        break;
    }
}

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
            return htons(a.port);
        }

        pa_xfree(a.path_or_host);
    }

    return htons(PA_NATIVE_DEFAULT_PORT);
}

static int publish_service(struct service *s) {
    int r = -1;
    TXTRecordRef txt;
    DNSServiceErrorType err;
    const char *name = NULL, *t;
    pa_proplist *proplist = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    char cm[PA_CHANNEL_MAP_SNPRINT_MAX], tmp[64];
    enum service_subtype subtype;

    const char * const subtype_text[] = {
        [SUBTYPE_HARDWARE] = "hardware",
        [SUBTYPE_VIRTUAL] = "virtual",
        [SUBTYPE_MONITOR] = "monitor"
    };

    pa_assert(s);

    if (s->service) {
        DNSServiceRefDeallocate(s->service);
        s->service = NULL;
    }

    TXTRecordCreate(&txt, 0, NULL);

    txt_record_server_data(s->userdata->core, &txt);

    get_service_data(s, &ss, &map, &name, &proplist, &subtype);
    TXTRecordSetValue(&txt, "device", strlen(name), name);

    snprintf(tmp, sizeof(tmp), "%u", ss.rate);
    TXTRecordSetValue(&txt, "rate", strlen(tmp), tmp);

    snprintf(tmp, sizeof(tmp), "%u", ss.channels);
    TXTRecordSetValue(&txt, "channels", strlen(tmp), tmp);

    t = pa_sample_format_to_string(ss.format);
    TXTRecordSetValue(&txt, "format", strlen(t), t);

    t = pa_channel_map_snprint(cm, sizeof(cm), &map);
    TXTRecordSetValue(&txt, "channel_map", strlen(t), t);

    t = subtype_text[subtype];
    TXTRecordSetValue(&txt, "subtype", strlen(t), t);

    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_DESCRIPTION)))
        TXTRecordSetValue(&txt, "description", strlen(t), t);
    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_ICON_NAME)))
        TXTRecordSetValue(&txt, "icon-name", strlen(t), t);
    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_VENDOR_NAME)))
        TXTRecordSetValue(&txt, "vendor-name", strlen(t), t);
    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_PRODUCT_NAME)))
        TXTRecordSetValue(&txt, "product-name", strlen(t), t);
    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_CLASS)))
        TXTRecordSetValue(&txt, "class", strlen(t), t);
    if ((t = pa_proplist_gets(proplist, PA_PROP_DEVICE_FORM_FACTOR)))
        TXTRecordSetValue(&txt, "form-factor", strlen(t), t);

    err = DNSServiceRegister(&s->service,
                             0,         /* flags */
                             kDNSServiceInterfaceIndexAny,
                             s->service_name,
                             pa_sink_isinstance(s->device) ? SERVICE_TYPE_SINK : SERVICE_TYPE_SOURCE,
                             NULL,      /* domain */
                             NULL,      /* host */
                             compute_port(s->userdata),
                             TXTRecordGetLength(&txt),
                             TXTRecordGetBytesPtr(&txt),
                             dns_service_register_reply, s);

    if (err != kDNSServiceErr_NoError) {
        pa_log("DNSServiceRegister() returned err %d", err);
        goto finish;
    }

    pa_log_debug("Successfully registered Bonjour services for >%s<.", s->service_name);
    return 0;

finish:

    /* Remove this service */
    if (r < 0)
        service_free(s);

    TXTRecordDeallocate(&txt);

    return r;
}

static struct service *get_service(struct userdata *u, pa_object *device) {
    struct service *s;
    char *hn, *un;
    const char *n;

    pa_assert(u);
    pa_object_assert_ref(device);

    if ((s = pa_hashmap_get(u->services, device)))
        return s;

    s = pa_xnew0(struct service, 1);
    s->userdata = u;
    s->device = device;

    if (pa_sink_isinstance(device)) {
        if (!(n = pa_proplist_gets(PA_SINK(device)->proplist, PA_PROP_DEVICE_DESCRIPTION)))
            n = PA_SINK(device)->name;
    } else {
        if (!(n = pa_proplist_gets(PA_SOURCE(device)->proplist, PA_PROP_DEVICE_DESCRIPTION)))
            n = PA_SOURCE(device)->name;
    }

    hn = pa_get_host_name_malloc();
    un = pa_get_user_name_malloc();

    s->service_name = pa_truncate_utf8(pa_sprintf_malloc("%s@%s: %s", un, hn, n), kDNSServiceMaxDomainName-1);

    pa_xfree(un);
    pa_xfree(hn);

    pa_hashmap_put(u->services, s->device, s);

    return s;
}

static void service_free(struct service *s) {
    pa_assert(s);

    pa_hashmap_remove(s->userdata->services, s->device);

    if (s->service)
        DNSServiceRefDeallocate(s->service);

    pa_xfree(s->service_name);
    pa_xfree(s);
}

static bool shall_ignore(pa_object *o) {
    pa_object_assert_ref(o);

    if (pa_sink_isinstance(o))
        return !!(PA_SINK(o)->flags & PA_SINK_NETWORK);

    if (pa_source_isinstance(o))
        return PA_SOURCE(o)->monitor_of || (PA_SOURCE(o)->flags & PA_SOURCE_NETWORK);

    pa_assert_not_reached();
}

static pa_hook_result_t device_new_or_changed_cb(pa_core *c, pa_object *o, struct userdata *u) {
    pa_assert(c);
    pa_object_assert_ref(o);

    if (!shall_ignore(o))
        publish_service(get_service(u, o));

    return PA_HOOK_OK;
}

static pa_hook_result_t device_unlink_cb(pa_core *c, pa_object *o, struct userdata *u) {
    struct service *s;

    pa_assert(c);
    pa_object_assert_ref(o);

    if ((s = pa_hashmap_get(u->services, o)))
        service_free(s);

    return PA_HOOK_OK;
}

static int publish_main_service(struct userdata *u) {
    DNSServiceErrorType err;
    TXTRecordRef txt;

    pa_assert(u);

    if (u->main_service) {
        DNSServiceRefDeallocate(u->main_service);
        u->main_service = NULL;
    }

    TXTRecordCreate(&txt, 0, NULL);
    txt_record_server_data(u->core, &txt);

    err = DNSServiceRegister(&u->main_service,
                             0, /* flags */
                             kDNSServiceInterfaceIndexAny,
                             u->service_name,
                             SERVICE_TYPE_SERVER,
                             NULL, /* domain */
                             NULL, /* host */
                             compute_port(u),
                             TXTRecordGetLength(&txt),
                             TXTRecordGetBytesPtr(&txt),
                             NULL, NULL);

    if (err != kDNSServiceErr_NoError) {
        pa_log("%s(): DNSServiceRegister() returned err %d", __func__, err);
        return err;
    }

    TXTRecordDeallocate(&txt);

    return 0;
}

static int publish_all_services(struct userdata *u) {
    pa_sink *sink;
    pa_source *source;
    uint32_t idx;

    pa_assert(u);

    pa_log_debug("Publishing services in Bonjour");

    for (sink = PA_SINK(pa_idxset_first(u->core->sinks, &idx)); sink; sink = PA_SINK(pa_idxset_next(u->core->sinks, &idx)))
        if (!shall_ignore(PA_OBJECT(sink)))
            publish_service(get_service(u, PA_OBJECT(sink)));

    for (source = PA_SOURCE(pa_idxset_first(u->core->sources, &idx)); source; source = PA_SOURCE(pa_idxset_next(u->core->sources, &idx)))
        if (!shall_ignore(PA_OBJECT(source)))
            publish_service(get_service(u, PA_OBJECT(source)));

    return publish_main_service(u);
}

static void unpublish_all_services(struct userdata *u) {
    void *state = NULL;
    struct service *s;

    pa_assert(u);

    pa_log_debug("Unpublishing services in Bonjour");

    while ((s = pa_hashmap_iterate(u->services, &state, NULL)))
        service_free(s);

    if (u->main_service)
        DNSServiceRefDeallocate(u->main_service);
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

    u->services = pa_hashmap_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->sink_new_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->sink_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PROPLIST_CHANGED], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->sink_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) device_unlink_cb, u);
    u->source_new_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->source_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PROPLIST_CHANGED], PA_HOOK_LATE, (pa_hook_cb_t) device_new_or_changed_cb, u);
    u->source_unlink_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) device_unlink_cb, u);

    un = pa_get_user_name_malloc();
    hn = pa_get_host_name_malloc();
    u->service_name = pa_truncate_utf8(pa_sprintf_malloc("%s@%s", un, hn), kDNSServiceMaxDomainName-1);
    pa_xfree(un);
    pa_xfree(hn);

    publish_all_services(u);
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

    unpublish_all_services(u);

    if (u->services)
        pa_hashmap_free(u->services);

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

    pa_xfree(u->service_name);
    pa_xfree(u);
}
