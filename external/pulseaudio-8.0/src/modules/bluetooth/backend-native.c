/***
  This file is part of PulseAudio.

  Copyright 2014 Wim Taymans <wim.taymans at gmail.com>

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

#include <pulsecore/shared.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/dbus-shared.h>
#include <pulsecore/log.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sco.h>

#include "bluez5-util.h"

struct pa_bluetooth_backend {
  pa_core *core;
  pa_dbus_connection *connection;
  pa_bluetooth_discovery *discovery;

  PA_LLIST_HEAD(pa_dbus_pending, pending);
};

struct transport_rfcomm {
    int rfcomm_fd;
    pa_io_event *rfcomm_io;
    pa_mainloop_api *mainloop;
};

#define BLUEZ_SERVICE "org.bluez"
#define BLUEZ_MEDIA_TRANSPORT_INTERFACE BLUEZ_SERVICE ".MediaTransport1"

#define BLUEZ_ERROR_NOT_SUPPORTED "org.bluez.Error.NotSupported"

#define BLUEZ_PROFILE_MANAGER_INTERFACE BLUEZ_SERVICE ".ProfileManager1"
#define BLUEZ_PROFILE_INTERFACE BLUEZ_SERVICE ".Profile1"

#define HSP_AG_PROFILE "/Profile/HSPAGProfile"

#define PROFILE_INTROSPECT_XML                                          \
    DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE                           \
    "<node>"                                                            \
    " <interface name=\"" BLUEZ_PROFILE_INTERFACE "\">"                 \
    "  <method name=\"Release\">"                                       \
    "  </method>"                                                       \
    "  <method name=\"RequestDisconnection\">"                          \
    "   <arg name=\"device\" direction=\"in\" type=\"o\"/>"             \
    "  </method>"                                                       \
    "  <method name=\"NewConnection\">"                                 \
    "   <arg name=\"device\" direction=\"in\" type=\"o\"/>"             \
    "   <arg name=\"fd\" direction=\"in\" type=\"h\"/>"                 \
    "   <arg name=\"opts\" direction=\"in\" type=\"a{sv}\"/>"           \
    "  </method>"                                                       \
    " </interface>"                                                     \
    " <interface name=\"org.freedesktop.DBus.Introspectable\">"         \
    "  <method name=\"Introspect\">"                                    \
    "   <arg name=\"data\" type=\"s\" direction=\"out\"/>"              \
    "  </method>"                                                       \
    " </interface>"                                                     \
    "</node>"

static pa_dbus_pending* send_and_add_to_pending(pa_bluetooth_backend *backend, DBusMessage *m,
        DBusPendingCallNotifyFunction func, void *call_data) {

    pa_dbus_pending *p;
    DBusPendingCall *call;

    pa_assert(backend);
    pa_assert(m);

    pa_assert_se(dbus_connection_send_with_reply(pa_dbus_connection_get(backend->connection), m, &call, -1));

    p = pa_dbus_pending_new(pa_dbus_connection_get(backend->connection), m, call, backend, call_data);
    PA_LLIST_PREPEND(pa_dbus_pending, backend->pending, p);
    dbus_pending_call_set_notify(call, func, p, NULL);

    return p;
}

static int bluez5_sco_acquire_cb(pa_bluetooth_transport *t, bool optional, size_t *imtu, size_t *omtu) {
    pa_bluetooth_device *d = t->device;
    struct sockaddr_sco addr;
    int err, i;
    int sock;
    bdaddr_t src;
    bdaddr_t dst;
    const char *src_addr, *dst_addr;

    src_addr = d->adapter->address;
    dst_addr = d->address;

    /* don't use ba2str to avoid -lbluetooth */
    for (i = 5; i >= 0; i--, src_addr += 3)
        src.b[i] = strtol(src_addr, NULL, 16);
    for (i = 5; i >= 0; i--, dst_addr += 3)
        dst.b[i] = strtol(dst_addr, NULL, 16);

    sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
    if (sock < 0) {
        pa_log_error("socket(SEQPACKET, SCO) %s", pa_cstrerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sco_family = AF_BLUETOOTH;
    bacpy(&addr.sco_bdaddr, &src);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        pa_log_error("bind(): %s", pa_cstrerror(errno));
        goto fail_close;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sco_family = AF_BLUETOOTH;
    bacpy(&addr.sco_bdaddr, &dst);

    pa_log_info ("doing connect\n");
    err = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
    if (err < 0 && !(errno == EAGAIN || errno == EINPROGRESS)) {
        pa_log_error("connect(): %s", pa_cstrerror(errno));
        goto fail_close;
    }

    /* The "48" below is hardcoded until we get meaningful MTU values exposed
     * by the kernel */

    if (imtu)
        *imtu = 48;

    if (omtu)
        *omtu = 48;

    return sock;

fail_close:
    close(sock);
    return -1;
}

static void bluez5_sco_release_cb(pa_bluetooth_transport *t) {
    pa_log_info("Transport %s released", t->path);
    /* device will close the SCO socket for us */
}

static void register_profile_reply(DBusPendingCall *pending, void *userdata) {
    DBusMessage *r;
    pa_dbus_pending *p;
    pa_bluetooth_backend *b;
    char *profile;

    pa_assert(pending);
    pa_assert_se(p = userdata);
    pa_assert_se(b = p->context_data);
    pa_assert_se(profile = p->call_data);
    pa_assert_se(r = dbus_pending_call_steal_reply(pending));

    if (dbus_message_is_error(r, BLUEZ_ERROR_NOT_SUPPORTED)) {
        pa_log_info("Couldn't register profile %s because it is disabled in BlueZ", profile);
        goto finish;
    }

    if (dbus_message_get_type(r) == DBUS_MESSAGE_TYPE_ERROR) {
        pa_log_error(BLUEZ_PROFILE_MANAGER_INTERFACE ".RegisterProfile() failed: %s: %s", dbus_message_get_error_name(r),
                     pa_dbus_get_error_message(r));
        goto finish;
    }

finish:
    dbus_message_unref(r);

    PA_LLIST_REMOVE(pa_dbus_pending, b->pending, p);
    pa_dbus_pending_free(p);

    pa_xfree(profile);
}

static void register_profile(pa_bluetooth_backend *b, const char *profile, const char *uuid) {
    DBusMessage *m;
    DBusMessageIter i, d;

    pa_log_debug("Registering Profile %s", profile);

    pa_assert_se(m = dbus_message_new_method_call(BLUEZ_SERVICE, "/org/bluez", BLUEZ_PROFILE_MANAGER_INTERFACE, "RegisterProfile"));

    dbus_message_iter_init_append(m, &i);
    dbus_message_iter_append_basic(&i, DBUS_TYPE_OBJECT_PATH, &profile);
    dbus_message_iter_append_basic(&i, DBUS_TYPE_STRING, &uuid);
    dbus_message_iter_open_container(&i, DBUS_TYPE_ARRAY, DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING DBUS_TYPE_STRING_AS_STRING
            DBUS_TYPE_VARIANT_AS_STRING DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &d);
    dbus_message_iter_close_container(&i, &d);

    send_and_add_to_pending(b, m, register_profile_reply, pa_xstrdup(profile));
}

static void rfcomm_io_callback(pa_mainloop_api *io, pa_io_event *e, int fd, pa_io_event_flags_t events, void *userdata) {
    pa_bluetooth_transport *t = userdata;

    pa_assert(io);
    pa_assert(t);

    if (events & (PA_IO_EVENT_HANGUP|PA_IO_EVENT_ERROR)) {
        pa_log_info("Lost RFCOMM connection.");
        goto fail;
    }

    if (events & PA_IO_EVENT_INPUT) {
        char buf[512];
        ssize_t len;
        int gain;

        len = read(fd, buf, 511);
        buf[len] = 0;
        pa_log_debug("RFCOMM << %s", buf);

        if (sscanf(buf, "AT+VGS=%d", &gain) == 1) {
          t->speaker_gain = gain;
          pa_hook_fire(pa_bluetooth_discovery_hook(t->device->discovery, PA_BLUETOOTH_HOOK_TRANSPORT_SPEAKER_GAIN_CHANGED), t);

        } else if (sscanf(buf, "AT+VGM=%d", &gain) == 1) {
          t->microphone_gain = gain;
          pa_hook_fire(pa_bluetooth_discovery_hook(t->device->discovery, PA_BLUETOOTH_HOOK_TRANSPORT_MICROPHONE_GAIN_CHANGED), t);
        }

        pa_log_debug("RFCOMM >> OK");

        len = write(fd, "\r\nOK\r\n", 6);

        /* we ignore any errors, it's not critical and real errors should
         * be caught with the HANGUP and ERROR events handled above */
        if (len < 0)
            pa_log_error("RFCOMM write error: %s", pa_cstrerror(errno));
    }

    return;

fail:
    pa_bluetooth_transport_unlink(t);
    pa_bluetooth_transport_free(t);
    return;
}

static void transport_destroy(pa_bluetooth_transport *t) {
    struct transport_rfcomm *trfc = t->userdata;

    trfc->mainloop->io_free(trfc->rfcomm_io);

    shutdown(trfc->rfcomm_fd, SHUT_RDWR);
    close (trfc->rfcomm_fd);

    pa_xfree(trfc);
}

static void set_speaker_gain(pa_bluetooth_transport *t, uint16_t gain) {
    struct transport_rfcomm *trfc = t->userdata;
    char buf[512];
    ssize_t len, written;

    if (t->speaker_gain == gain)
      return;

    t->speaker_gain = gain;

    len = sprintf(buf, "\r\n+VGS=%d\r\n", gain);
    pa_log_debug("RFCOMM >> +VGS=%d", gain);

    written = write(trfc->rfcomm_fd, buf, len);

    if (written != len)
        pa_log_error("RFCOMM write error: %s", pa_cstrerror(errno));
}

static void set_microphone_gain(pa_bluetooth_transport *t, uint16_t gain) {
    struct transport_rfcomm *trfc = t->userdata;
    char buf[512];
    ssize_t len, written;

    if (t->microphone_gain == gain)
      return;

    t->microphone_gain = gain;

    len = sprintf(buf, "\r\n+VGM=%d\r\n", gain);
    pa_log_debug("RFCOMM >> +VGM=%d", gain);

    written = write (trfc->rfcomm_fd, buf, len);

    if (written != len)
        pa_log_error("RFCOMM write error: %s", pa_cstrerror(errno));
}

static DBusMessage *profile_new_connection(DBusConnection *conn, DBusMessage *m, void *userdata) {
    pa_bluetooth_backend *b = userdata;
    pa_bluetooth_device *d;
    pa_bluetooth_transport *t;
    pa_bluetooth_profile_t p;
    DBusMessage *r;
    int fd;
    const char *sender, *path, PA_UNUSED *handler;
    DBusMessageIter arg_i;
    char *pathfd;
    struct transport_rfcomm *trfc;

    if (!dbus_message_iter_init(m, &arg_i) || !pa_streq(dbus_message_get_signature(m), "oha{sv}")) {
        pa_log_error("Invalid signature found in NewConnection");
        goto fail;
    }

    handler = dbus_message_get_path(m);
    pa_assert(pa_streq(handler, HSP_AG_PROFILE));

    pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_OBJECT_PATH);
    dbus_message_iter_get_basic(&arg_i, &path);

    d = pa_bluetooth_discovery_get_device_by_path(b->discovery, path);
    if (d == NULL) {
        pa_log_error("Device doesnt exist for %s", path);
        goto fail;
    }

    pa_assert_se(dbus_message_iter_next(&arg_i));

    pa_assert(dbus_message_iter_get_arg_type(&arg_i) == DBUS_TYPE_UNIX_FD);
    dbus_message_iter_get_basic(&arg_i, &fd);

    pa_log_debug("dbus: NewConnection path=%s, fd=%d", path, fd);

    sender = dbus_message_get_sender(m);

    p = PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT;
    pathfd = pa_sprintf_malloc ("%s/fd%d", path, fd);
    d->transports[p] = t = pa_bluetooth_transport_new(d, sender, pathfd, p, NULL, 0);
    pa_xfree(pathfd);

    t->acquire = bluez5_sco_acquire_cb;
    t->release = bluez5_sco_release_cb;
    t->destroy = transport_destroy;
    t->set_speaker_gain = set_speaker_gain;
    t->set_microphone_gain = set_microphone_gain;

    trfc = pa_xnew0(struct transport_rfcomm, 1);
    trfc->rfcomm_fd = fd;
    trfc->mainloop = b->core->mainloop;
    trfc->rfcomm_io = trfc->mainloop->io_new(b->core->mainloop, fd, PA_IO_EVENT_INPUT|PA_IO_EVENT_HANGUP,
        rfcomm_io_callback, t);
    t->userdata =  trfc;

    pa_bluetooth_transport_put(t);

    pa_log_debug("Transport %s available for profile %s", t->path, pa_bluetooth_profile_to_string(t->profile));

    pa_assert_se(r = dbus_message_new_method_return(m));

    return r;

fail:
    pa_assert_se(r = dbus_message_new_error(m, "org.bluez.Error.InvalidArguments", "Unable to handle new connection"));
    return r;
}

static DBusMessage *profile_request_disconnection(DBusConnection *conn, DBusMessage *m, void *userdata) {
    DBusMessage *r;

    pa_assert_se(r = dbus_message_new_method_return(m));

    return r;
}

static DBusHandlerResult profile_handler(DBusConnection *c, DBusMessage *m, void *userdata) {
    pa_bluetooth_backend *b = userdata;
    DBusMessage *r = NULL;
    const char *path, *interface, *member;

    pa_assert(b);

    path = dbus_message_get_path(m);
    interface = dbus_message_get_interface(m);
    member = dbus_message_get_member(m);

    pa_log_debug("dbus: path=%s, interface=%s, member=%s", path, interface, member);

    if (!pa_streq(path, HSP_AG_PROFILE))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_method_call(m, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        const char *xml = PROFILE_INTROSPECT_XML;

        pa_assert_se(r = dbus_message_new_method_return(m));
        pa_assert_se(dbus_message_append_args(r, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID));

    } else if (dbus_message_is_method_call(m, BLUEZ_PROFILE_INTERFACE, "Release")) {
    } else if (dbus_message_is_method_call(m, BLUEZ_PROFILE_INTERFACE, "RequestDisconnection")) {
        r = profile_request_disconnection(c, m, userdata);
    } else if (dbus_message_is_method_call(m, BLUEZ_PROFILE_INTERFACE, "NewConnection"))
        r = profile_new_connection(c, m, userdata);
    else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (r) {
        pa_assert_se(dbus_connection_send(pa_dbus_connection_get(b->connection), r, NULL));
        dbus_message_unref(r);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static void profile_init(pa_bluetooth_backend *b, pa_bluetooth_profile_t profile) {
    static const DBusObjectPathVTable vtable_profile = {
        .message_function = profile_handler,
    };
    const char *object_name;
    const char *uuid;

    pa_assert(b);

    switch (profile) {
        case PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT:
            object_name = HSP_AG_PROFILE;
            uuid = PA_BLUETOOTH_UUID_HSP_AG;
            break;
        default:
            pa_assert_not_reached();
            break;
    }

    pa_assert_se(dbus_connection_register_object_path(pa_dbus_connection_get(b->connection), object_name, &vtable_profile, b));
    register_profile(b, object_name, uuid);
}

static void profile_done(pa_bluetooth_backend *b, pa_bluetooth_profile_t profile) {
    pa_assert(b);

    switch (profile) {
        case PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT:
            dbus_connection_unregister_object_path(pa_dbus_connection_get(b->connection), HSP_AG_PROFILE);
            break;
        default:
            pa_assert_not_reached();
            break;
    }
}

pa_bluetooth_backend *pa_bluetooth_native_backend_new(pa_core *c, pa_bluetooth_discovery *y) {
    pa_bluetooth_backend *backend;
    DBusError err;

    pa_log_debug("Bluetooth Headset Backend API support using the native backend");

    backend = pa_xnew0(pa_bluetooth_backend, 1);
    backend->core = c;

    dbus_error_init(&err);
    if (!(backend->connection = pa_dbus_bus_get(c, DBUS_BUS_SYSTEM, &err))) {
        pa_log("Failed to get D-Bus connection: %s", err.message);
        dbus_error_free(&err);
        pa_xfree(backend);
        return NULL;
    }

    backend->discovery = y;

    profile_init(backend, PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT);

    return backend;
}

void pa_bluetooth_native_backend_free(pa_bluetooth_backend *backend) {
    pa_assert(backend);

    pa_dbus_free_pending_list(&backend->pending);

    profile_done(backend, PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT);

    pa_dbus_connection_unref(backend->connection);

    pa_xfree(backend);
}
