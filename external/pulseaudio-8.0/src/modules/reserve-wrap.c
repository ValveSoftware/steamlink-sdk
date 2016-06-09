/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#include <errno.h>

#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/shared.h>

#ifdef HAVE_DBUS
#include <pulsecore/dbus-shared.h>
#include "reserve.h"
#include "reserve-monitor.h"
#endif

#include "reserve-wrap.h"

struct pa_reserve_wrapper {
    PA_REFCNT_DECLARE;
    pa_core *core;
    pa_hook hook;
    char *shared_name;
#ifdef HAVE_DBUS
    pa_dbus_connection *connection;
    struct rd_device *device;
#endif
};

struct pa_reserve_monitor_wrapper {
    PA_REFCNT_DECLARE;
    pa_core *core;
    pa_hook hook;
    char *shared_name;
#ifdef HAVE_DBUS
    pa_dbus_connection *connection;
    struct rm_monitor *monitor;
#endif
};

static void reserve_wrapper_free(pa_reserve_wrapper *r) {
    pa_assert(r);

#ifdef HAVE_DBUS
    if (r->device)
        rd_release(r->device);

    if (r->connection)
        pa_dbus_connection_unref(r->connection);
#endif

    pa_hook_done(&r->hook);

    if (r->shared_name) {
        pa_assert_se(pa_shared_remove(r->core, r->shared_name) >= 0);
        pa_xfree(r->shared_name);
    }

    pa_xfree(r);
}

#ifdef HAVE_DBUS
static int request_cb(rd_device *d, int forced) {
    pa_reserve_wrapper *r;
    int k;

    pa_assert(d);
    pa_assert_se(r = rd_get_userdata(d));
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    PA_REFCNT_INC(r);

    k = pa_hook_fire(&r->hook, PA_INT_TO_PTR(forced));
    pa_log_debug("Device unlock of %s has been requested and %s.", r->shared_name, k < 0 ? "failed" : "succeeded");

    pa_reserve_wrapper_unref(r);

    return k < 0 ? -1 : 1;
}
#endif

pa_reserve_wrapper* pa_reserve_wrapper_get(pa_core *c, const char *device_name) {
    pa_reserve_wrapper *r;
    char *t;
#ifdef HAVE_DBUS
    int k;
    DBusError error;

    dbus_error_init(&error);
#endif

    pa_assert(c);
    pa_assert(device_name);

    t = pa_sprintf_malloc("reserve-wrapper@%s", device_name);

    if ((r = pa_shared_get(c, t))) {
        pa_xfree(t);

        pa_assert(PA_REFCNT_VALUE(r) >= 1);
        PA_REFCNT_INC(r);

        return r;
    }

    r = pa_xnew0(pa_reserve_wrapper, 1);
    PA_REFCNT_INIT(r);
    r->core = c;
    pa_hook_init(&r->hook, r);
    r->shared_name = t;

    pa_assert_se(pa_shared_set(c, r->shared_name, r) >= 0);

#ifdef HAVE_DBUS
    if (!(r->connection = pa_dbus_bus_get(c, DBUS_BUS_SESSION, &error)) || dbus_error_is_set(&error)) {
        pa_log_debug("Unable to contact D-Bus session bus: %s: %s", error.name, error.message);

        /* We don't treat this as error here because we want allow PA
         * to run even when no session bus is available. */
        return r;
    }

    if ((k = rd_acquire(
                 &r->device,
                 pa_dbus_connection_get(r->connection),
                 device_name,
                 _("PulseAudio Sound Server"),
                 0,
                 request_cb,
                 NULL)) < 0) {

        if (k == -EBUSY) {
            pa_log_debug("Device '%s' already locked.", device_name);
            goto fail;
        } else {
            pa_log_debug("Failed to acquire reservation lock on device '%s': %s", device_name, pa_cstrerror(-k));
            return r;
        }
    }

    pa_log_debug("Successfully acquired reservation lock on device '%s'", device_name);

    rd_set_userdata(r->device, r);

    return r;
fail:
    dbus_error_free(&error);

    reserve_wrapper_free(r);

    return NULL;
#else
    return r;
#endif
}

void pa_reserve_wrapper_unref(pa_reserve_wrapper *r) {
    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    if (PA_REFCNT_DEC(r) > 0)
        return;

    reserve_wrapper_free(r);
}

pa_hook* pa_reserve_wrapper_hook(pa_reserve_wrapper *r) {
    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    return &r->hook;
}

void pa_reserve_wrapper_set_application_device_name(pa_reserve_wrapper *r, const char *name) {
    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

#ifdef HAVE_DBUS
    rd_set_application_device_name(r->device, name);
#endif
}

static void reserve_monitor_wrapper_free(pa_reserve_monitor_wrapper *w) {
    pa_assert(w);

#ifdef HAVE_DBUS
    if (w->monitor)
        rm_release(w->monitor);

    if (w->connection)
        pa_dbus_connection_unref(w->connection);
#endif

    pa_hook_done(&w->hook);

    if (w->shared_name) {
        pa_assert_se(pa_shared_remove(w->core, w->shared_name) >= 0);
        pa_xfree(w->shared_name);
    }

    pa_xfree(w);
}

#ifdef HAVE_DBUS
static void change_cb(rm_monitor *m) {
    pa_reserve_monitor_wrapper *w;
    int k;

    pa_assert(m);
    pa_assert_se(w = rm_get_userdata(m));
    pa_assert(PA_REFCNT_VALUE(w) >= 1);

    PA_REFCNT_INC(w);

    if ((k = rm_busy(w->monitor)) < 0)
        return;

    pa_hook_fire(&w->hook, PA_INT_TO_PTR(!!k));
    pa_log_debug("Device lock status of %s changed: %s", w->shared_name, k ? "busy" : "not busy");

    pa_reserve_monitor_wrapper_unref(w);
}
#endif

pa_reserve_monitor_wrapper* pa_reserve_monitor_wrapper_get(pa_core *c, const char *device_name) {
    pa_reserve_monitor_wrapper *w;
    char *t;
#ifdef HAVE_DBUS
    int k;
    DBusError error;

    dbus_error_init(&error);
#endif

    pa_assert(c);
    pa_assert(device_name);

    t = pa_sprintf_malloc("reserve-monitor-wrapper@%s", device_name);

    if ((w = pa_shared_get(c, t))) {
        pa_xfree(t);

        pa_assert(PA_REFCNT_VALUE(w) >= 1);
        PA_REFCNT_INC(w);

        return w;
    }

    w = pa_xnew0(pa_reserve_monitor_wrapper, 1);
    PA_REFCNT_INIT(w);
    w->core = c;
    pa_hook_init(&w->hook, w);
    w->shared_name = t;

    pa_assert_se(pa_shared_set(c, w->shared_name, w) >= 0);

#ifdef HAVE_DBUS
    if (!(w->connection = pa_dbus_bus_get(c, DBUS_BUS_SESSION, &error)) || dbus_error_is_set(&error)) {
        pa_log_debug("Unable to contact D-Bus session bus: %s: %s", error.name, error.message);

        /* We don't treat this as error here because we want allow PA
         * to run even when no session bus is available. */
        return w;
    }

    if ((k = rm_watch(
                 &w->monitor,
                 pa_dbus_connection_get(w->connection),
                 device_name,
                 change_cb,
                 NULL)) < 0) {

        pa_log_debug("Failed to create watch on device '%s': %s", device_name, pa_cstrerror(-k));
        goto fail;
    }

    pa_log_debug("Successfully create reservation lock monitor for device '%s'", device_name);

    rm_set_userdata(w->monitor, w);
    return w;

fail:
    dbus_error_free(&error);

    reserve_monitor_wrapper_free(w);

    return NULL;
#else
    return w;
#endif
}

void pa_reserve_monitor_wrapper_unref(pa_reserve_monitor_wrapper *w) {
    pa_assert(w);
    pa_assert(PA_REFCNT_VALUE(w) >= 1);

    if (PA_REFCNT_DEC(w) > 0)
        return;

    reserve_monitor_wrapper_free(w);
}

pa_hook* pa_reserve_monitor_wrapper_hook(pa_reserve_monitor_wrapper *w) {
    pa_assert(w);
    pa_assert(PA_REFCNT_VALUE(w) >= 1);

    return &w->hook;
}

bool pa_reserve_monitor_wrapper_busy(pa_reserve_monitor_wrapper *w) {
    pa_assert(w);

    pa_assert(PA_REFCNT_VALUE(w) >= 1);

#ifdef HAVE_DBUS
    return rm_busy(w->monitor) > 0;
#else
    return false;
#endif
}
