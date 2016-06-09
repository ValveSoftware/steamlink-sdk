/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#include <libudev.h>

#include <pulse/xmalloc.h>
#include <pulse/proplist.h>

#include <pulsecore/log.h>
#include <pulsecore/core-util.h>

#include "udev-util.h"

static int read_id(struct udev_device *d, const char *n) {
    const char *v;
    unsigned u;

    pa_assert(d);
    pa_assert(n);

    if (!(v = udev_device_get_property_value(d, n)))
        return -1;

    if (pa_startswith(v, "0x"))
        v += 2;

    if (!*v)
        return -1;

    if (sscanf(v, "%04x", &u) != 1)
        return -1;

    if (u > 0xFFFFU)
        return -1;

    return u;
}

static int dehex(char x) {
    if (x >= '0' && x <= '9')
        return x - '0';

    if (x >= 'A' && x <= 'F')
        return x - 'A' + 10;

    if (x >= 'a' && x <= 'f')
        return x - 'a' + 10;

    return -1;
}

static void proplist_sets_unescape(pa_proplist *p, const char *prop, const char *s) {
    const char *f;
    char *t, *r;
    int c = 0;

    enum {
        TEXT,
        BACKSLASH,
        EX,
        FIRST
    } state = TEXT;

    /* The resulting string is definitely shorter than the source string */
    r = pa_xnew(char, strlen(s)+1);

    for (f = s, t = r; *f; f++) {

        switch (state) {

            case TEXT:
                if (*f == '\\')
                    state = BACKSLASH;
                else
                    *(t++) = *f;
                break;

            case BACKSLASH:
                if (*f == 'x')
                    state = EX;
                else {
                    *(t++) = '\\';
                    *(t++) = *f;
                    state = TEXT;
                }
                break;

            case EX:
                c = dehex(*f);

                if (c < 0) {
                    *(t++) = '\\';
                    *(t++) = 'x';
                    *(t++) = *f;
                    state = TEXT;
                } else
                    state = FIRST;

                break;

            case FIRST: {
                int d = dehex(*f);

                if (d < 0) {
                    *(t++) = '\\';
                    *(t++) = 'x';
                    *(t++) = *(f-1);
                    *(t++) = *f;
                } else
                    *(t++) = (char) (c << 4) | d;

                state = TEXT;
                break;
            }
        }
    }

    switch (state) {

        case TEXT:
            break;

        case BACKSLASH:
            *(t++) = '\\';
            break;

        case EX:
            *(t++) = '\\';
            *(t++) = 'x';
            break;

        case FIRST:
            *(t++) = '\\';
            *(t++) = 'x';
            *(t++) = *(f-1);
            break;
    }

    *t = 0;

    pa_proplist_sets(p, prop, r);
    pa_xfree(r);
}

int pa_udev_get_info(int card_idx, pa_proplist *p) {
    int r = -1;
    struct udev *udev;
    struct udev_device *card = NULL;
    char *t;
    const char *v;
    const char *bus = NULL;
    int id;

    pa_assert(p);
    pa_assert(card_idx >= 0);

    if (!(udev = udev_new())) {
        pa_log_error("Failed to allocate udev context.");
        goto finish;
    }

    t = pa_sprintf_malloc("/sys/class/sound/card%i", card_idx);
    card = udev_device_new_from_syspath(udev, t);
    pa_xfree(t);

    if (!card) {
        pa_log_error("Failed to get card object.");
        goto finish;
    }

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_BUS_PATH))
        if (((v = udev_device_get_property_value(card, "ID_PATH")) && *v) ||
            (v = udev_device_get_devpath(card)))
            pa_proplist_sets(p, PA_PROP_DEVICE_BUS_PATH, v);

    if (!pa_proplist_contains(p, "sysfs.path"))
        if ((v = udev_device_get_devpath(card)))
            pa_proplist_sets(p, "sysfs.path", v);

    if (!pa_proplist_contains(p, "udev.id"))
        if ((v = udev_device_get_property_value(card, "ID_ID")) && *v)
            pa_proplist_sets(p, "udev.id", v);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_BUS))
        if ((bus = udev_device_get_property_value(card, "ID_BUS")) && *bus)
            pa_proplist_sets(p, PA_PROP_DEVICE_BUS, bus);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_VENDOR_ID))
        if ((id = read_id(card, "ID_VENDOR_ID")) > 0)
            pa_proplist_setf(p, PA_PROP_DEVICE_VENDOR_ID, "%04x", id);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_VENDOR_NAME)) {
        /* ID_VENDOR_FROM_DATABASE returns the name of IEEE 1394 Phy/Link chipset for FireWire devices */
        if (!pa_safe_streq(bus, "firewire") && (v = udev_device_get_property_value(card, "ID_VENDOR_FROM_DATABASE")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_VENDOR_NAME, v);
        else if ((v = udev_device_get_property_value(card, "ID_VENDOR_ENC")) && *v)
            proplist_sets_unescape(p, PA_PROP_DEVICE_VENDOR_NAME, v);
        else if ((v = udev_device_get_property_value(card, "ID_VENDOR")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_VENDOR_NAME, v);
    }

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_PRODUCT_ID))
        if ((id = read_id(card, "ID_MODEL_ID")) >= 0)
            pa_proplist_setf(p, PA_PROP_DEVICE_PRODUCT_ID, "%04x", id);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_PRODUCT_NAME)) {
        /* ID_MODEL_FROM_DATABASE returns the name of IEEE 1394 Phy/Link chipset for FireWire devices */
        if (!pa_safe_streq(bus, "firewire") && (v = udev_device_get_property_value(card, "ID_MODEL_FROM_DATABASE")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_PRODUCT_NAME, v);
        else if ((v = udev_device_get_property_value(card, "ID_MODEL_ENC")) && *v)
            proplist_sets_unescape(p, PA_PROP_DEVICE_PRODUCT_NAME, v);
        else if ((v = udev_device_get_property_value(card, "ID_MODEL")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_PRODUCT_NAME, v);
    }

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_SERIAL))
        if ((v = udev_device_get_property_value(card, "ID_SERIAL")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_SERIAL, v);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_CLASS))
        if ((v = udev_device_get_property_value(card, "SOUND_CLASS")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_CLASS, v);

    if (!pa_proplist_contains(p, PA_PROP_DEVICE_FORM_FACTOR))
        if ((v = udev_device_get_property_value(card, "SOUND_FORM_FACTOR")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_FORM_FACTOR, v);

    /* This is normally not set by the udev rules but may be useful to
     * allow administrators to overwrite the device description.*/
    if (!pa_proplist_contains(p, PA_PROP_DEVICE_DESCRIPTION))
        if ((v = udev_device_get_property_value(card, "SOUND_DESCRIPTION")) && *v)
            pa_proplist_sets(p, PA_PROP_DEVICE_DESCRIPTION, v);

    r = 0;

finish:

    if (card)
        udev_device_unref(card);

    if (udev)
        udev_unref(udev);

    return r;
}

char* pa_udev_get_property(int card_idx, const char *name) {
    struct udev *udev;
    struct udev_device *card = NULL;
    char *t, *r = NULL;
    const char *v;

    pa_assert(card_idx >= 0);
    pa_assert(name);

    if (!(udev = udev_new())) {
        pa_log_error("Failed to allocate udev context.");
        goto finish;
    }

    t = pa_sprintf_malloc("/sys/class/sound/card%i", card_idx);
    card = udev_device_new_from_syspath(udev, t);
    pa_xfree(t);

    if (!card) {
        pa_log_error("Failed to get card object.");
        goto finish;
    }

    if ((v = udev_device_get_property_value(card, name)) && *v)
        r = pa_xstrdup(v);

finish:

    if (card)
        udev_device_unref(card);

    if (udev)
        udev_unref(udev);

    return r;
}
