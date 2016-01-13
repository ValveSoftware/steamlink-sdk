/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB
  Copyright 2006 Diego Pettenò

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/core-util.h>
#include <pulsecore/macro.h>

#include "module-detect-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Detect available audio hardware and load matching drivers");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE("just-one=<boolean>");
PA_MODULE_DEPRECATED("Please use module-udev-detect instead of module-detect!");

static const char* const valid_modargs[] = {
    "just-one",
    NULL
};

#ifdef HAVE_ALSA

static int detect_alsa(pa_core *c, int just_one) {
    FILE *f;
    int n = 0, n_sink = 0, n_source = 0;

    if (!(f = pa_fopen_cloexec("/proc/asound/devices", "r"))) {

        if (errno != ENOENT)
            pa_log_error("open(\"/proc/asound/devices\") failed: %s", pa_cstrerror(errno));

        return -1;
    }

    while (!feof(f)) {
        char line[64], args[64];
        unsigned device, subdevice;
        int is_sink;

        if (!fgets(line, sizeof(line), f))
            break;

        line[strcspn(line, "\r\n")] = 0;

        if (pa_endswith(line, "digital audio playback"))
            is_sink = 1;
        else if (pa_endswith(line, "digital audio capture"))
            is_sink = 0;
        else
            continue;

        if (just_one && is_sink && n_sink >= 1)
            continue;

        if (just_one && !is_sink && n_source >= 1)
            continue;

        if (sscanf(line, " %*i: [%u- %u]: ", &device, &subdevice) != 2)
            continue;

        /* Only one sink per device */
        if (subdevice != 0)
            continue;

        pa_snprintf(args, sizeof(args), "device_id=%u", device);
        if (!pa_module_load(c, is_sink ? "module-alsa-sink" : "module-alsa-source", args))
            continue;

        n++;

        if (is_sink)
            n_sink++;
        else
            n_source++;
    }

    fclose(f);

    return n;
}
#endif

#ifdef HAVE_OSS_OUTPUT
static int detect_oss(pa_core *c, int just_one) {
    FILE *f;
    int n = 0, b = 0;

    if (!(f = pa_fopen_cloexec("/dev/sndstat", "r")) &&
        !(f = pa_fopen_cloexec("/proc/sndstat", "r")) &&
        !(f = pa_fopen_cloexec("/proc/asound/oss/sndstat", "r"))) {

        if (errno != ENOENT)
            pa_log_error("failed to open OSS sndstat device: %s", pa_cstrerror(errno));

        return -1;
    }

    while (!feof(f)) {
        char line[64], args[64];
        unsigned device;

        if (!fgets(line, sizeof(line), f))
            break;

        line[strcspn(line, "\r\n")] = 0;

        if (!b) {
            b = pa_streq(line, "Audio devices:") || pa_streq(line, "Installed devices:");
            continue;
        }

        if (line[0] == 0)
            break;

        if (sscanf(line, "%u: ", &device) == 1) {
            if (device == 0)
                pa_snprintf(args, sizeof(args), "device=/dev/dsp");
            else
                pa_snprintf(args, sizeof(args), "device=/dev/dsp%u", device);

            if (!pa_module_load(c, "module-oss", args))
                continue;

        } else if (sscanf(line, "pcm%u: ", &device) == 1) {
            /* FreeBSD support, the devices are named /dev/dsp0.0, dsp0.1 and so on */
            pa_snprintf(args, sizeof(args), "device=/dev/dsp%u.0", device);

            if (!pa_module_load(c, "module-oss", args))
                continue;
        }

        n++;

        if (just_one)
            break;
    }

    fclose(f);
    return n;
}
#endif

#ifdef HAVE_SOLARIS
static int detect_solaris(pa_core *c, int just_one) {
    struct stat s;
    const char *dev;
    char args[64];

    dev = getenv("AUDIODEV");
    if (!dev)
        dev = "/dev/audio";

    if (stat(dev, &s) < 0) {
        if (errno != ENOENT)
            pa_log_error("failed to open device %s: %s", dev, pa_cstrerror(errno));
        return -1;
    }

    if (!S_ISCHR(s.st_mode))
        return 0;

    pa_snprintf(args, sizeof(args), "device=%s", dev);

    if (!pa_module_load(c, "module-solaris", args))
        return 0;

    return 1;
}
#endif

#ifdef OS_IS_WIN32
static int detect_waveout(pa_core *c, int just_one) {
    /*
     * FIXME: No point in enumerating devices until the plugin supports
     * selecting anything but the first.
     */
    if (!pa_module_load(c, "module-waveout", ""))
        return 0;

    return 1;
}
#endif

int pa__init(pa_module*m) {
    bool just_one = false;
    int n = 0;
    pa_modargs *ma;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "just-one", &just_one) < 0) {
        pa_log("just_one= expects a boolean argument.");
        goto fail;
    }

#ifdef HAVE_ALSA
    if ((n = detect_alsa(m->core, just_one)) <= 0)
#endif
#ifdef HAVE_OSS_OUTPUT
    if ((n = detect_oss(m->core, just_one)) <= 0)
#endif
#ifdef HAVE_SOLARIS
    if ((n = detect_solaris(m->core, just_one)) <= 0)
#endif
#ifdef OS_IS_WIN32
    if ((n = detect_waveout(m->core, just_one)) <= 0)
#endif
    {
        pa_log_warn("failed to detect any sound hardware.");
        goto fail;
    }

    pa_log_info("loaded %i modules.", n);

    /* We were successful and can unload ourselves now. */
    pa_module_unload_request(m, true);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    return -1;
}
