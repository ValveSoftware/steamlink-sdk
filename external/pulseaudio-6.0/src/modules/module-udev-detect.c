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

#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <libudev.h>

#include <pulse/timeval.h>

#include <pulsecore/modargs.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/namereg.h>
#include <pulsecore/ratelimit.h>
#include <pulsecore/strbuf.h>

#include "module-udev-detect-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Detect available audio hardware and load matching drivers");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_USAGE(
        "tsched=<enable system timer based scheduling mode?> "
        "tsched_buffer_size=<buffer size when using timer based scheduling> "
        "fixed_latency_range=<disable latency range changes on underrun?> "
        "ignore_dB=<ignore dB information from the device?> "
        "deferred_volume=<syncronize sw and hw volume changes in IO-thread?> "
        "use_ucm=<use ALSA UCM for card configuration?>");


/*
    The system does not run a udev daemon and we don't support
    /lib/udev/rules.d/78-sound-card.rules! so this detection code needs some
    special workarrounds
*/
#define NO_UDEV_RULES

struct device {
    char *path;
    bool need_verify;
    char *card_name;
    char *args;
    uint32_t module;
    pa_ratelimit ratelimit;
};

struct userdata {
    pa_core *core;
    pa_hashmap *devices;

    bool use_tsched:1;
    bool tsched_buffer_size_valid:1;
    bool fixed_latency_range:1;
    bool ignore_dB:1;
    bool deferred_volume:1;
    bool use_ucm:1;

    uint32_t tsched_buffer_size;

    struct udev* udev;
    struct udev_monitor *monitor;
    pa_io_event *udev_io;

    int inotify_fd;
    pa_io_event *inotify_io;
};

static const char* const valid_modargs[] = {
    "tsched",
    "tsched_buffer_size",
    "fixed_latency_range",
    "ignore_dB",
    "deferred_volume",
    "use_ucm",
    NULL
};

static int setup_inotify(struct userdata *u);

static void device_free(struct device *d) {
    pa_assert(d);

    pa_xfree(d->path);
    pa_xfree(d->card_name);
    pa_xfree(d->args);
    pa_xfree(d);
}

static const char *path_get_card_id(const char *path) {
    const char *e;

    if (!path)
        return NULL;

    if (!(e = strrchr(path, '/')))
        return NULL;

    if (!pa_startswith(e, "/card"))
        return NULL;

    return e + 5;
}

#ifdef NO_UDEV_RULES
bool path_contains_control(const char *path) {
    const char *e;

    if (!path)
        return false;

    if (!(e = strrchr(path, '/')))
        return false;

    if (!pa_startswith(e, "/control"))
        return false;

    return true;
}
#endif

static char *card_get_sysattr(const char *card_idx, const char *name) {
    struct udev *udev;
    struct udev_device *card = NULL;
    char *t, *r = NULL;
    const char *v;

    pa_assert(card_idx);
    pa_assert(name);

    if (!(udev = udev_new())) {
        pa_log_error("Failed to allocate udev context.");
        goto finish;
    }

    t = pa_sprintf_malloc("/sys/class/sound/card%s", card_idx);
    card = udev_device_new_from_syspath(udev, t);
    pa_xfree(t);

    if (!card) {
        pa_log_error("Failed to get card object.");
        goto finish;
    }

    if ((v = udev_device_get_sysattr_value(card, name)) && *v)
        r = pa_xstrdup(v);

finish:

    if (card)
        udev_device_unref(card);

    if (udev)
        udev_unref(udev);

    return r;
}

static bool pcm_is_modem(const char *card_idx, const char *pcm) {
    char *sysfs_path, *pcm_class;
    bool is_modem;

    pa_assert(card_idx);
    pa_assert(pcm);

    /* Check /sys/class/sound/card.../pcmC...../pcm_class. An HDA
     * modem can be used simultaneously with generic
     * playback/record. */

    sysfs_path = pa_sprintf_malloc("pcmC%sD%s/pcm_class", card_idx, pcm);
    pcm_class = card_get_sysattr(card_idx, sysfs_path);
    is_modem = pcm_class && pa_streq(pcm_class, "modem");
    pa_xfree(pcm_class);
    pa_xfree(sysfs_path);

    return is_modem;
}

static bool is_card_busy(const char *id) {
    char *card_path = NULL, *pcm_path = NULL, *sub_status = NULL;
    DIR *card_dir = NULL, *pcm_dir = NULL;
    FILE *status_file = NULL;
    size_t len;
    struct dirent *space = NULL, *de;
    bool busy = false;
    int r;

    pa_assert(id);

    /* This simply uses /proc/asound/card.../pcm.../sub.../status to
     * check whether there is still a process using this audio device. */

    card_path = pa_sprintf_malloc("/proc/asound/card%s", id);

    if (!(card_dir = opendir(card_path))) {
        pa_log_warn("Failed to open %s: %s", card_path, pa_cstrerror(errno));
        goto fail;
    }

    len = offsetof(struct dirent, d_name) + fpathconf(dirfd(card_dir), _PC_NAME_MAX) + 1;
    space = pa_xmalloc(len);

    for (;;) {
        de = NULL;

        if ((r = readdir_r(card_dir, space, &de)) != 0) {
            pa_log_warn("readdir_r() failed: %s", pa_cstrerror(r));
            goto fail;
        }

        if (!de)
            break;

        if (!pa_startswith(de->d_name, "pcm"))
            continue;

        if (pcm_is_modem(id, de->d_name + 3))
            continue;

        pa_xfree(pcm_path);
        pcm_path = pa_sprintf_malloc("%s/%s", card_path, de->d_name);

        if (pcm_dir)
            closedir(pcm_dir);

        if (!(pcm_dir = opendir(pcm_path))) {
            pa_log_warn("Failed to open %s: %s", pcm_path, pa_cstrerror(errno));
            continue;
        }

        for (;;) {
            char line[32];

            if ((r = readdir_r(pcm_dir, space, &de)) != 0) {
                pa_log_warn("readdir_r() failed: %s", pa_cstrerror(r));
                goto fail;
            }

            if (!de)
                break;

            if (!pa_startswith(de->d_name, "sub"))
                continue;

            pa_xfree(sub_status);
            sub_status = pa_sprintf_malloc("%s/%s/status", pcm_path, de->d_name);

            if (status_file)
                fclose(status_file);

            if (!(status_file = pa_fopen_cloexec(sub_status, "r"))) {
                pa_log_warn("Failed to open %s: %s", sub_status, pa_cstrerror(errno));
                continue;
            }

            if (!(fgets(line, sizeof(line)-1, status_file))) {
                pa_log_warn("Failed to read from %s: %s", sub_status, pa_cstrerror(errno));
                continue;
            }

            if (!pa_streq(line, "closed\n")) {
                busy = true;
                break;
            }
        }
    }

fail:

    pa_xfree(card_path);
    pa_xfree(pcm_path);
    pa_xfree(sub_status);
    pa_xfree(space);

    if (card_dir)
        closedir(card_dir);

    if (pcm_dir)
        closedir(pcm_dir);

    if (status_file)
        fclose(status_file);

    return busy;
}

static void verify_access(struct userdata *u, struct device *d) {
    char *cd;
    pa_card *card;
    bool accessible;

    pa_assert(u);
    pa_assert(d);

    cd = pa_sprintf_malloc("/dev/snd/controlC%s", path_get_card_id(d->path));
    accessible = access(cd, R_OK|W_OK) >= 0;
    pa_log_debug("%s is accessible: %s", cd, pa_yes_no(accessible));

    pa_xfree(cd);

    if (d->module == PA_INVALID_INDEX) {

        /* If we are not loaded, try to load */

        if (accessible) {
            pa_module *m;
            bool busy;

            /* Check if any of the PCM devices that belong to this
             * card are currently busy. If they are, don't try to load
             * right now, to make sure the probing phase can
             * successfully complete. When the current user of the
             * device closes it we will get another notification via
             * inotify and can then recheck. */

            busy = is_card_busy(path_get_card_id(d->path));
            pa_log_debug("%s is busy: %s", d->path, pa_yes_no(busy));

            if (!busy) {

                /* So, why do we rate limit here? It's certainly ugly,
                 * but there seems to be no other way. Problem is
                 * this: if we are unable to configure/probe an audio
                 * device after opening it we will close it again and
                 * the module initialization will fail. This will then
                 * cause an inotify event on the device node which
                 * will be forwarded to us. We then try to reopen the
                 * audio device again, practically entering a busy
                 * loop.
                 *
                 * A clean fix would be if we would be able to ignore
                 * our own inotify close events. However, inotify
                 * lacks such functionality. Also, during probing of
                 * the device we cannot really distinguish between
                 * other processes causing EBUSY or ourselves, which
                 * means we have no way to figure out if the probing
                 * during opening was canceled by a "try again"
                 * failure or a "fatal" failure. */

                if (pa_ratelimit_test(&d->ratelimit, PA_LOG_DEBUG)) {
                    pa_log_debug("Loading module-alsa-card with arguments '%s'", d->args);
                    m = pa_module_load(u->core, "module-alsa-card", d->args);

                    if (m) {
                        d->module = m->index;
                        pa_log_info("Card %s (%s) module loaded.", d->path, d->card_name);
                    } else
                        pa_log_info("Card %s (%s) failed to load module.", d->path, d->card_name);
                } else
                    pa_log_warn("Tried to configure %s (%s) more often than %u times in %llus",
                                d->path,
                                d->card_name,
                                d->ratelimit.burst,
                                (long long unsigned) (d->ratelimit.interval / PA_USEC_PER_SEC));
            }
        }

    } else {

        /* If we are already loaded update suspend status with
         * accessible boolean */

        if ((card = pa_namereg_get(u->core, d->card_name, PA_NAMEREG_CARD))) {
            pa_log_debug("%s all sinks and sources of card %s.", accessible ? "Resuming" : "Suspending", d->card_name);
            pa_card_suspend(card, !accessible, PA_SUSPEND_SESSION);
        }
    }
}

static void card_changed(struct userdata *u, struct udev_device *dev) {
    struct device *d;
    const char *path;
    const char *t;
    char *n;
    pa_strbuf *args_buf;

    pa_assert(u);
    pa_assert(dev);

    /* Maybe /dev/snd is now available? */
    setup_inotify(u);

    path = udev_device_get_devpath(dev);

    if ((d = pa_hashmap_get(u->devices, path))) {
        verify_access(u, d);
        return;
    }

    d = pa_xnew0(struct device, 1);
    d->path = pa_xstrdup(path);
    d->module = PA_INVALID_INDEX;
    PA_INIT_RATELIMIT(d->ratelimit, 10*PA_USEC_PER_SEC, 5);

    if (!(t = udev_device_get_property_value(dev, "PULSE_NAME")))
        if (!(t = udev_device_get_property_value(dev, "ID_ID")))
            if (!(t = udev_device_get_property_value(dev, "ID_PATH")))
                t = path_get_card_id(path);

    n = pa_namereg_make_valid_name(t);
    d->card_name = pa_sprintf_malloc("alsa_card.%s", n);
    args_buf = pa_strbuf_new();
    pa_strbuf_printf(args_buf,
                     "device_id=\"%s\" "
                     "name=\"%s\" "
                     "card_name=\"%s\" "
                     "namereg_fail=false "
                     "tsched=%s "
                     "fixed_latency_range=%s "
                     "ignore_dB=%s "
                     "deferred_volume=%s "
                     "use_ucm=%s "
                     "card_properties=\"module-udev-detect.discovered=1\"",
                     path_get_card_id(path),
                     n,
                     d->card_name,
                     pa_yes_no(u->use_tsched),
                     pa_yes_no(u->fixed_latency_range),
                     pa_yes_no(u->ignore_dB),
                     pa_yes_no(u->deferred_volume),
                     pa_yes_no(u->use_ucm));
    pa_xfree(n);

    if (u->tsched_buffer_size_valid)
        pa_strbuf_printf(args_buf, " tsched_buffer_size=%" PRIu32, u->tsched_buffer_size);

    d->args = pa_strbuf_tostring_free(args_buf);

    pa_hashmap_put(u->devices, d->path, d);

    verify_access(u, d);
}

static void remove_card(struct userdata *u, struct udev_device *dev) {
    struct device *d;

    pa_assert(u);
    pa_assert(dev);

    if (!(d = pa_hashmap_remove(u->devices, udev_device_get_devpath(dev))))
        return;

    pa_log_info("Card %s removed.", d->path);

    if (d->module != PA_INVALID_INDEX)
        pa_module_unload_request_by_index(u->core, d->module, true);

    device_free(d);
}

static void process_device(struct userdata *u, struct udev_device *dev) {
    const char *action, *ff;

    pa_assert(u);
    pa_assert(dev);

    if (udev_device_get_property_value(dev, "PULSE_IGNORE")) {
        pa_log_debug("Ignoring %s, because marked so.", udev_device_get_devpath(dev));
        return;
    }

    if ((ff = udev_device_get_property_value(dev, "SOUND_CLASS")) &&
        pa_streq(ff, "modem")) {
        pa_log_debug("Ignoring %s, because it is a modem.", udev_device_get_devpath(dev));
        return;
    }

    action = udev_device_get_action(dev);

    if (action && pa_streq(action, "remove"))
        remove_card(u, dev);
#ifdef NO_UDEV_RULES
    else if (!action)
        card_changed(u, dev);
#else
    else if ((!action || pa_streq(action, "change")) && udev_device_get_property_value(dev, "SOUND_INITIALIZED"))
        card_changed(u, dev);
#endif
    /* For an explanation why we don't look for 'add' events here
     * have a look into /lib/udev/rules.d/78-sound-card.rules! */
}

static void process_path(struct userdata *u, const char *path) {
    struct udev_device *dev;

    if (!path_get_card_id(path))
        return;

    if (!(dev = udev_device_new_from_syspath(u->udev, path))) {
        pa_log("Failed to get udev device object from udev.");
        return;
    }

    process_device(u, dev);
    udev_device_unref(dev);
}

static void monitor_cb(
        pa_mainloop_api*a,
        pa_io_event* e,
        int fd,
        pa_io_event_flags_t events,
        void *userdata) {

    struct userdata *u = userdata;
    struct udev_device *dev;

    pa_assert(a);

    if (!(dev = udev_monitor_receive_device(u->monitor))) {
#ifdef NO_UDEV_RULES
        return;
#else
        pa_log("Failed to get udev device object from monitor.");
        goto fail;
#endif
    }

#ifdef NO_UDEV_RULES
    if(path_contains_control(udev_device_get_devpath(dev)))
    {
        struct udev_device *parent = udev_device_get_parent( dev );
        process_device(u, parent);
        udev_device_unref(dev);
        return;
    }
#endif
    if (!path_get_card_id(udev_device_get_devpath(dev))) {
        udev_device_unref(dev);
        return;
    }

    process_device(u, dev);
    udev_device_unref(dev);
    return;

#ifndef NO_UDEV_RULES
fail:
    a->io_free(u->udev_io);
    u->udev_io = NULL;
#endif
}

static bool pcm_node_belongs_to_device(
        struct device *d,
        const char *node) {

    char *cd;
    bool b;

    cd = pa_sprintf_malloc("pcmC%sD", path_get_card_id(d->path));
    b = pa_startswith(node, cd);
    pa_xfree(cd);

    return b;
}

static bool control_node_belongs_to_device(
        struct device *d,
        const char *node) {

    char *cd;
    bool b;

    cd = pa_sprintf_malloc("controlC%s", path_get_card_id(d->path));
    b = pa_streq(node, cd);
    pa_xfree(cd);

    return b;
}

static void inotify_cb(
        pa_mainloop_api*a,
        pa_io_event* e,
        int fd,
        pa_io_event_flags_t events,
        void *userdata) {

    struct {
        struct inotify_event e;
        char name[NAME_MAX];
    } buf;
    struct userdata *u = userdata;
    static int type = 0;
    bool deleted = false;
    struct device *d;
    void *state;

    for (;;) {
        ssize_t r;
        struct inotify_event *event;

        pa_zero(buf);
        if ((r = pa_read(fd, &buf, sizeof(buf), &type)) <= 0) {

            if (r < 0 && errno == EAGAIN)
                break;

            pa_log("read() from inotify failed: %s", r < 0 ? pa_cstrerror(errno) : "EOF");
            goto fail;
        }

        event = &buf.e;
        while (r > 0) {
            size_t len;

            if ((size_t) r < sizeof(struct inotify_event)) {
                pa_log("read() too short.");
                goto fail;
            }

            len = sizeof(struct inotify_event) + event->len;

            if ((size_t) r < len) {
                pa_log("Payload missing.");
                goto fail;
            }

            /* From udev we get the guarantee that the control
             * device's ACL is changed last. To avoid races when ACLs
             * are changed we hence watch only the control device */
            if (((event->mask & IN_ATTRIB) && pa_startswith(event->name, "controlC")))
                PA_HASHMAP_FOREACH(d, u->devices, state)
                    if (control_node_belongs_to_device(d, event->name))
                        d->need_verify = true;

            /* ALSA doesn't really give us any guarantee on the closing
             * order, so let's simply hope */
            if (((event->mask & IN_CLOSE_WRITE) && pa_startswith(event->name, "pcmC")))
                PA_HASHMAP_FOREACH(d, u->devices, state)
                    if (pcm_node_belongs_to_device(d, event->name))
                        d->need_verify = true;

            /* /dev/snd/ might have been removed */
            if ((event->mask & (IN_DELETE_SELF|IN_MOVE_SELF)))
                deleted = true;

            event = (struct inotify_event*) ((uint8_t*) event + len);
            r -= len;
        }
    }

    PA_HASHMAP_FOREACH(d, u->devices, state)
        if (d->need_verify) {
            d->need_verify = false;
            verify_access(u, d);
        }

    if (!deleted)
        return;

fail:
    if (u->inotify_io) {
        a->io_free(u->inotify_io);
        u->inotify_io = NULL;
    }

    if (u->inotify_fd >= 0) {
        pa_close(u->inotify_fd);
        u->inotify_fd = -1;
    }
}

static int setup_inotify(struct userdata *u) {
    int r;

    if (u->inotify_fd >= 0)
        return 0;

    if ((u->inotify_fd = inotify_init1(IN_CLOEXEC|IN_NONBLOCK)) < 0) {
        pa_log("inotify_init1() failed: %s", pa_cstrerror(errno));
        return -1;
    }

    r = inotify_add_watch(u->inotify_fd, "/dev/snd", IN_ATTRIB|IN_CLOSE_WRITE|IN_DELETE_SELF|IN_MOVE_SELF);

    if (r < 0) {
        int saved_errno = errno;

        pa_close(u->inotify_fd);
        u->inotify_fd = -1;

        if (saved_errno == ENOENT) {
            pa_log_debug("/dev/snd/ is apparently not existing yet, retrying to create inotify watch later.");
            return 0;
        }

        if (saved_errno == ENOSPC) {
            pa_log("You apparently ran out of inotify watches, probably because Tracker/Beagle took them all away. "
                   "I wished people would do their homework first and fix inotify before using it for watching whole "
                   "directory trees which is something the current inotify is certainly not useful for. "
                   "Please make sure to drop the Tracker/Beagle guys a line complaining about their broken use of inotify.");
            return 0;
        }

        pa_log("inotify_add_watch() failed: %s", pa_cstrerror(saved_errno));
        return -1;
    }

    pa_assert_se(u->inotify_io = u->core->mainloop->io_new(u->core->mainloop, u->inotify_fd, PA_IO_EVENT_INPUT, inotify_cb, u));

    return 0;
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma;
    struct udev_enumerate *enumerate = NULL;
    struct udev_list_entry *item = NULL, *first = NULL;
    int fd;
    bool use_tsched = true, fixed_latency_range = false, ignore_dB = false, deferred_volume = m->core->deferred_volume;
    bool use_ucm = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->devices = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) device_free);
    u->inotify_fd = -1;

    if (pa_modargs_get_value_boolean(ma, "tsched", &use_tsched) < 0) {
        pa_log("Failed to parse tsched= argument.");
        goto fail;
    }
    u->use_tsched = use_tsched;

    if (pa_modargs_get_value(ma, "tsched_buffer_size", NULL)) {
        if (pa_modargs_get_value_u32(ma, "tsched_buffer_size", &u->tsched_buffer_size) < 0) {
            pa_log("Failed to parse tsched_buffer_size= argument.");
            goto fail;
        }

        u->tsched_buffer_size_valid = true;
    }

    if (pa_modargs_get_value_boolean(ma, "fixed_latency_range", &fixed_latency_range) < 0) {
        pa_log("Failed to parse fixed_latency_range= argument.");
        goto fail;
    }
    u->fixed_latency_range = fixed_latency_range;

    if (pa_modargs_get_value_boolean(ma, "ignore_dB", &ignore_dB) < 0) {
        pa_log("Failed to parse ignore_dB= argument.");
        goto fail;
    }
    u->ignore_dB = ignore_dB;

    if (pa_modargs_get_value_boolean(ma, "deferred_volume", &deferred_volume) < 0) {
        pa_log("Failed to parse deferred_volume= argument.");
        goto fail;
    }
    u->deferred_volume = deferred_volume;

    if (pa_modargs_get_value_boolean(ma, "use_ucm", &use_ucm) < 0) {
        pa_log("Failed to parse use_ucm= argument.");
        goto fail;
    }
    u->use_ucm = use_ucm;

    if (!(u->udev = udev_new())) {
        pa_log("Failed to initialize udev library.");
        goto fail;
    }

    if (setup_inotify(u) < 0)
        goto fail;

    if (!(u->monitor = udev_monitor_new_from_netlink(u->udev, "udev"))) {
        pa_log("Failed to initialize monitor.");
        goto fail;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(u->monitor, "sound", NULL) < 0) {
        pa_log("Failed to subscribe to sound devices.");
        goto fail;
    }

    errno = 0;
    if (udev_monitor_enable_receiving(u->monitor) < 0) {
        pa_log("Failed to enable monitor: %s", pa_cstrerror(errno));
        if (errno == EPERM)
            pa_log_info("Most likely your kernel is simply too old and "
                        "allows only privileged processes to listen to device events. "
                        "Please upgrade your kernel to at least 2.6.30.");
        goto fail;
    }

    if ((fd = udev_monitor_get_fd(u->monitor)) < 0) {
        pa_log("Failed to get udev monitor fd.");
        goto fail;
    }

    pa_assert_se(u->udev_io = u->core->mainloop->io_new(u->core->mainloop, fd, PA_IO_EVENT_INPUT, monitor_cb, u));

    if (!(enumerate = udev_enumerate_new(u->udev))) {
        pa_log("Failed to initialize udev enumerator.");
        goto fail;
    }

    if (udev_enumerate_add_match_subsystem(enumerate, "sound") < 0) {
        pa_log("Failed to match to subsystem.");
        goto fail;
    }

    if (udev_enumerate_scan_devices(enumerate) < 0) {
        pa_log("Failed to scan for devices.");
        goto fail;
    }

    first = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(item, first)
        process_path(u, udev_list_entry_get_name(item));

    udev_enumerate_unref(enumerate);

    pa_log_info("Found %u cards.", pa_hashmap_size(u->devices));

    pa_modargs_free(ma);

    return 0;

fail:

    if (enumerate)
        udev_enumerate_unref(enumerate);

    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->udev_io)
        m->core->mainloop->io_free(u->udev_io);

    if (u->monitor)
        udev_monitor_unref(u->monitor);

    if (u->udev)
        udev_unref(u->udev);

    if (u->inotify_io)
        m->core->mainloop->io_free(u->inotify_io);

    if (u->inotify_fd >= 0)
        pa_close(u->inotify_fd);

    if (u->devices)
        pa_hashmap_free(u->devices);

    pa_xfree(u);
}
