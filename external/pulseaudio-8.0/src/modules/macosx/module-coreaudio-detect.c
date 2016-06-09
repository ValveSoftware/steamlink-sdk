/***
  This file is part of PulseAudio.

  Copyright 2009,2010 Daniel Mack <daniel@caiaq.de>

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

#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/llist.h>

#include <CoreAudio/CoreAudio.h>

#include "module-coreaudio-detect-symdef.h"

#define DEVICE_MODULE_NAME "module-coreaudio-device"

PA_MODULE_AUTHOR("Daniel Mack");
PA_MODULE_DESCRIPTION("CoreAudio device detection");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE("ioproc_frames=<passed on to module-coreaudio-device> ");

static const char* const valid_modargs[] = {
    "ioproc_frames",
    NULL
};

typedef struct ca_device ca_device;

struct ca_device {
    AudioObjectID id;
    unsigned int module_index;
    PA_LLIST_FIELDS(ca_device);
};

struct userdata {
    int detect_fds[2];
    pa_io_event *detect_io;
    unsigned int ioproc_frames;
    PA_LLIST_HEAD(ca_device, devices);
};

static int ca_device_added(struct pa_module *m, AudioObjectID id) {
    AudioObjectPropertyAddress property_address;
    OSStatus err;
    pa_module *mod;
    struct userdata *u;
    struct ca_device *dev;
    char *args, tmp[64];
    UInt32 size;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    /* To prevent generating a black hole that will suck us in,
       don't create sources/sinks for PulseAudio virtual devices */

    property_address.mSelector = kAudioDevicePropertyDeviceManufacturer;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    size = sizeof(tmp);
    err = AudioObjectGetPropertyData(id, &property_address, 0, NULL, &size, tmp);

    if (!err && pa_streq(tmp, "pulseaudio.org"))
        return 0;

    if (u->ioproc_frames)
        args = pa_sprintf_malloc("object_id=%d ioproc_frames=%d", (int) id, u->ioproc_frames);
    else
        args = pa_sprintf_malloc("object_id=%d", (int) id);

    pa_log_debug("Loading %s with arguments '%s'", DEVICE_MODULE_NAME, args);
    mod = pa_module_load(m->core, DEVICE_MODULE_NAME, args);
    pa_xfree(args);

    if (!mod) {
        pa_log_info("Failed to load module %s with arguments '%s'", DEVICE_MODULE_NAME, args);
        return -1;
    }

    dev = pa_xnew0(ca_device, 1);
    dev->module_index = mod->index;
    dev->id = id;

    PA_LLIST_INIT(ca_device, dev);
    PA_LLIST_PREPEND(ca_device, u->devices, dev);

    return 0;
}

static int ca_update_device_list(struct pa_module *m) {
    AudioObjectPropertyAddress property_address;
    OSStatus err;
    UInt32 i, size, num_devices;
    AudioDeviceID *device_id;
    struct ca_device *dev;
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    property_address.mSelector = kAudioHardwarePropertyDevices;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    /* get the number of currently available audio devices */
    err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0, NULL, &size);
    if (err) {
        pa_log("Unable to get data size for kAudioHardwarePropertyDevices.");
        return -1;
    }

    num_devices = size / sizeof(AudioDeviceID);
    device_id = pa_xnew(AudioDeviceID, num_devices);

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, NULL, &size, device_id);
    if (err) {
        pa_log("Unable to get kAudioHardwarePropertyDevices.");
        pa_xfree(device_id);
        return -1;
    }

    /* scan for devices which are reported but not in our cached list */
    for (i = 0; i < num_devices; i++) {
        bool found = false;

        PA_LLIST_FOREACH(dev, u->devices)
            if (dev->id == device_id[i]) {
                found = true;
                break;
            }

        if (!found)
            ca_device_added(m, device_id[i]);
    }

    /* scan for devices which are in our cached list but are not reported */
scan_removed:

    PA_LLIST_FOREACH(dev, u->devices) {
        bool found = false;

        for (i = 0; i < num_devices; i++)
            if (dev->id == device_id[i]) {
                found = true;
                break;
            }

        if (!found) {
            pa_log_debug("object id %d has been removed (module index %d) %p", (unsigned int) dev->id, dev->module_index, dev);
            pa_module_unload_request_by_index(m->core, dev->module_index, true);
            PA_LLIST_REMOVE(ca_device, u->devices, dev);
            pa_xfree(dev);
            /* the current list item pointer is not valid anymore, so start over. */
            goto scan_removed;
        }
    }

    pa_xfree(device_id);
    return 0;
}

static OSStatus property_listener_proc(AudioObjectID objectID, UInt32 numberAddresses,
                                       const AudioObjectPropertyAddress inAddresses[],
                                       void *clientData) {
    struct userdata *u = clientData;
    char dummy = 1;

    pa_assert(u);

    /* dispatch module load/unload operations in main thread */
    write(u->detect_fds[1], &dummy, 1);

    return 0;
}

static void detect_handle(pa_mainloop_api *a, pa_io_event *e, int fd, pa_io_event_flags_t events, void *userdata) {
    pa_module *m = userdata;
    char dummy;

    pa_assert(m);

    read(fd, &dummy, 1);
    ca_update_device_list(m);
}

int pa__init(pa_module *m) {
    struct userdata *u = pa_xnew0(struct userdata, 1);
    AudioObjectPropertyAddress property_address;
    pa_modargs *ma;

    pa_assert(m);

    m->userdata = u;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    pa_modargs_get_value_u32(ma, "ioproc_frames", &u->ioproc_frames);

    property_address.mSelector = kAudioHardwarePropertyDevices;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    if (AudioObjectAddPropertyListener(kAudioObjectSystemObject, &property_address, property_listener_proc, u)) {
        pa_log("AudioObjectAddPropertyListener() failed.");
        goto fail;
    }

    if (ca_update_device_list(m))
       goto fail;

    pa_assert_se(pipe(u->detect_fds) == 0);
    pa_assert_se(u->detect_io = m->core->mainloop->io_new(m->core->mainloop, u->detect_fds[0], PA_IO_EVENT_INPUT, detect_handle, m));

    return 0;

fail:
    pa_xfree(u);
    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;
    struct ca_device *dev;
    AudioObjectPropertyAddress property_address;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    dev = u->devices;

    property_address.mSelector = kAudioHardwarePropertyDevices;
    property_address.mScope = kAudioObjectPropertyScopeGlobal;
    property_address.mElement = kAudioObjectPropertyElementMaster;

    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &property_address, property_listener_proc, u);

    while (dev) {
        struct ca_device *next = dev->next;

        pa_module_unload_request_by_index(m->core, dev->module_index, true);
        pa_xfree(dev);

        dev = next;
    }

    if (u->detect_fds[0] >= 0)
        close(u->detect_fds[0]);

    if (u->detect_fds[1] >= 0)
        close(u->detect_fds[1]);

    if (u->detect_io)
        m->core->mainloop->io_free(u->detect_io);

    pa_xfree(u);
}
