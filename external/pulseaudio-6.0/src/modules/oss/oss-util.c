/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <pulse/xmalloc.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>

#include "oss-util.h"

int pa_oss_open(const char *device, int *mode, int* pcaps) {
    int fd = -1;
    int caps;
    char *t;

    pa_assert(device);
    pa_assert(mode);
    pa_assert(*mode == O_RDWR || *mode == O_RDONLY || *mode == O_WRONLY);

    if (!pcaps)
        pcaps = &caps;

    if (*mode == O_RDWR) {
        if ((fd = pa_open_cloexec(device, O_RDWR|O_NDELAY, 0)) >= 0) {
            ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0);

            if (ioctl(fd, SNDCTL_DSP_GETCAPS, pcaps) < 0) {
                pa_log("SNDCTL_DSP_GETCAPS: %s", pa_cstrerror(errno));
                goto fail;
            }

            if (*pcaps & DSP_CAP_DUPLEX)
                goto success;

            pa_log_warn("'%s' doesn't support full duplex", device);

            pa_close(fd);
        }

        if ((fd = pa_open_cloexec(device, (*mode = O_WRONLY)|O_NDELAY, 0)) < 0) {
            if ((fd = pa_open_cloexec(device, (*mode = O_RDONLY)|O_NDELAY, 0)) < 0) {
                pa_log("open('%s'): %s", device, pa_cstrerror(errno));
                goto fail;
            }
        }
    } else {
        if ((fd = pa_open_cloexec(device, *mode|O_NDELAY, 0)) < 0) {
            pa_log("open('%s'): %s", device, pa_cstrerror(errno));
            goto fail;
        }
    }

    *pcaps = 0;

    if (ioctl(fd, SNDCTL_DSP_GETCAPS, pcaps) < 0) {
        pa_log("SNDCTL_DSP_GETCAPS: %s", pa_cstrerror(errno));
        goto fail;
    }

success:

    t = pa_sprintf_malloc(
            "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
                 *pcaps & DSP_CAP_BATCH ? " BATCH" : "",
#ifdef DSP_CAP_BIND
                 *pcaps & DSP_CAP_BIND ? " BIND" : "",
#else
                 "",
#endif
                 *pcaps & DSP_CAP_COPROC ? " COPROC" : "",
                 *pcaps & DSP_CAP_DUPLEX ? " DUPLEX" : "",
#ifdef DSP_CAP_FREERATE
                 *pcaps & DSP_CAP_FREERATE ? " FREERATE" : "",
#else
                 "",
#endif
#ifdef DSP_CAP_INPUT
                 *pcaps & DSP_CAP_INPUT ? " INPUT" : "",
#else
                 "",
#endif
                 *pcaps & DSP_CAP_MMAP ? " MMAP" : "",
#ifdef DSP_CAP_MODEM
                 *pcaps & DSP_CAP_MODEM ? " MODEM" : "",
#else
                 "",
#endif
#ifdef DSP_CAP_MULTI
                 *pcaps & DSP_CAP_MULTI ? " MULTI" : "",
#else
                 "",
#endif
#ifdef DSP_CAP_OUTPUT
                 *pcaps & DSP_CAP_OUTPUT ? " OUTPUT" : "",
#else
                 "",
#endif
                 *pcaps & DSP_CAP_REALTIME ? " REALTIME" : "",
#ifdef DSP_CAP_SHADOW
                 *pcaps & DSP_CAP_SHADOW ? " SHADOW" : "",
#else
                 "",
#endif
#ifdef DSP_CAP_VIRTUAL
                 *pcaps & DSP_CAP_VIRTUAL ? " VIRTUAL" : "",
#else
                 "",
#endif
                 *pcaps & DSP_CAP_TRIGGER ? " TRIGGER" : "");

    pa_log_debug("capabilities:%s", t);
    pa_xfree(t);

    return fd;

fail:
    if (fd >= 0)
        pa_close(fd);
    return -1;
}

int pa_oss_auto_format(int fd, pa_sample_spec *ss) {
    int format, channels, speed, reqformat;
    pa_sample_format_t orig_format;

    static const int format_trans[PA_SAMPLE_MAX] = {
        [PA_SAMPLE_U8] = AFMT_U8,
        [PA_SAMPLE_ALAW] = AFMT_A_LAW,
        [PA_SAMPLE_ULAW] = AFMT_MU_LAW,
        [PA_SAMPLE_S16LE] = AFMT_S16_LE,
        [PA_SAMPLE_S16BE] = AFMT_S16_BE,
        [PA_SAMPLE_FLOAT32LE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_FLOAT32BE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S32LE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S32BE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S24LE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S24BE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S24_32LE] = AFMT_QUERY, /* not supported */
        [PA_SAMPLE_S24_32BE] = AFMT_QUERY, /* not supported */
    };

    pa_assert(fd >= 0);
    pa_assert(ss);

    orig_format = ss->format;

    reqformat = format = format_trans[ss->format];
    if (reqformat == AFMT_QUERY || ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != reqformat) {
        format = AFMT_S16_NE;
        if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != AFMT_S16_NE) {
            int f = AFMT_S16_NE == AFMT_S16_LE ? AFMT_S16_BE : AFMT_S16_LE;
            format = f;
            if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != f) {
                format = AFMT_U8;
                if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) < 0 || format != AFMT_U8) {
                    pa_log("SNDCTL_DSP_SETFMT: %s", format != AFMT_U8 ? "No supported sample format" : pa_cstrerror(errno));
                    return -1;
                } else
                    ss->format = PA_SAMPLE_U8;
            } else
                ss->format = f == AFMT_S16_LE ? PA_SAMPLE_S16LE : PA_SAMPLE_S16BE;
        } else
            ss->format = PA_SAMPLE_S16NE;
    }

    if (orig_format != ss->format)
        pa_log_warn("device doesn't support sample format %s, changed to %s.",
               pa_sample_format_to_string(orig_format),
               pa_sample_format_to_string(ss->format));

    channels = ss->channels;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        pa_log("SNDCTL_DSP_CHANNELS: %s", pa_cstrerror(errno));
        return -1;
    }
    pa_assert(channels > 0);

    if (ss->channels != channels) {
        pa_log_warn("device doesn't support %i channels, using %i channels.", ss->channels, channels);
        ss->channels = (uint8_t) channels;
    }

    speed = (int) ss->rate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) < 0) {
        pa_log("SNDCTL_DSP_SPEED: %s", pa_cstrerror(errno));
        return -1;
    }
    pa_assert(speed > 0);

    if (ss->rate != (unsigned) speed) {
        pa_log_warn("device doesn't support %i Hz, changed to %i Hz.", ss->rate, speed);

        /* If the sample rate deviates too much, we need to resample */
        if (speed < ss->rate*.95 || speed > ss->rate*1.05)
            ss->rate = (uint32_t) speed;
    }

    return 0;
}

int pa_oss_set_fragments(int fd, int nfrags, int frag_size) {
    int arg;

    pa_assert(frag_size >= 0);

    arg = ((int) nfrags << 16) | pa_ulog2(frag_size);

    pa_log_debug("Asking for %i fragments of size %i (requested %i)", nfrags, 1 << pa_ulog2(frag_size), frag_size);

    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &arg) < 0) {
        pa_log("SNDCTL_DSP_SETFRAGMENT: %s", pa_cstrerror(errno));
        return -1;
    }

    return 0;
}

int pa_oss_get_volume(int fd, unsigned long mixer, const pa_sample_spec *ss, pa_cvolume *volume) {
    char cv[PA_CVOLUME_SNPRINT_VERBOSE_MAX];
    unsigned vol;

    pa_assert(fd >= 0);
    pa_assert(ss);
    pa_assert(volume);

    if (ioctl(fd, mixer, &vol) < 0)
        return -1;

    pa_cvolume_reset(volume, ss->channels);

    volume->values[0] = PA_CLAMP_VOLUME(((vol & 0xFF) * PA_VOLUME_NORM) / 100);

    if (volume->channels >= 2)
        volume->values[1] = PA_CLAMP_VOLUME((((vol >> 8) & 0xFF) * PA_VOLUME_NORM) / 100);

    pa_log_debug("Read mixer settings: %s", pa_cvolume_snprint_verbose(cv, sizeof(cv), volume, NULL, false));
    return 0;
}

int pa_oss_set_volume(int fd, unsigned long mixer, const pa_sample_spec *ss, const pa_cvolume *volume) {
    char cv[PA_CVOLUME_SNPRINT_MAX];
    unsigned vol;
    pa_volume_t l, r;

    l = volume->values[0] > PA_VOLUME_NORM ? PA_VOLUME_NORM : volume->values[0];

    vol = (l*100)/PA_VOLUME_NORM;

    if (ss->channels >= 2) {
        r = volume->values[1] > PA_VOLUME_NORM ? PA_VOLUME_NORM : volume->values[1];
        vol |= ((r*100)/PA_VOLUME_NORM) << 8;
    }

    if (ioctl(fd, mixer, &vol) < 0)
        return -1;

    pa_log_debug("Wrote mixer settings: %s", pa_cvolume_snprint(cv, sizeof(cv), volume));
    return 0;
}

static int get_device_number(const char *dev) {
    const char *p, *e;
    char *rp = NULL;
    int r;

    if (!(p = rp = pa_readlink(dev))) {
#ifdef ENOLINK
        if (errno != EINVAL && errno != ENOLINK) {
#else
        if (errno != EINVAL) {
#endif
            r = -1;
            goto finish;
        }

        p = dev;
    }

    if ((e = strrchr(p, '/')))
        p = e+1;

    if (p == 0) {
        r = 0;
        goto finish;
    }

    p = strchr(p, 0) -1;

    if (*p >= '0' && *p <= '9') {
        r = *p - '0';
        goto finish;
    }

    r = -1;

finish:
    pa_xfree(rp);
    return r;
}

int pa_oss_get_hw_description(const char *dev, char *name, size_t l) {
    FILE *f;
    int n, r = -1;
    int b = 0;

    if ((n = get_device_number(dev)) < 0)
        return -1;

    if (!(f = pa_fopen_cloexec("/dev/sndstat", "r")) &&
        !(f = pa_fopen_cloexec("/proc/sndstat", "r")) &&
        !(f = pa_fopen_cloexec("/proc/asound/oss/sndstat", "r"))) {

        if (errno != ENOENT)
            pa_log_warn("failed to open OSS sndstat device: %s", pa_cstrerror(errno));

        return -1;
    }

    while (!feof(f)) {
        char line[64];
        int device;

        if (!fgets(line, sizeof(line), f))
            break;

        line[strcspn(line, "\r\n")] = 0;

        if (!b) {
            b = pa_streq(line, "Audio devices:");
            continue;
        }

        if (line[0] == 0)
            break;

        if (sscanf(line, "%i: ", &device) != 1)
            continue;

        if (device == n) {
            char *k = strchr(line, ':');
            pa_assert(k);
            k++;
            k += strspn(k, " ");

            if (pa_endswith(k, " (DUPLEX)"))
                k[strlen(k)-9] = 0;

            pa_strlcpy(name, k, l);
            r = 0;
            break;
        }
    }

    fclose(f);
    return r;
}

static int open_mixer(const char *mixer) {
    int fd;

    if ((fd = pa_open_cloexec(mixer, O_RDWR|O_NDELAY, 0)) >= 0)
        return fd;

    return -1;
}

int pa_oss_open_mixer_for_device(const char *device) {
    int n;
    char *fn;
    int fd;

    if ((n = get_device_number(device)) < 0)
        return -1;

    if (n == 0)
        if ((fd = open_mixer("/dev/mixer")) >= 0)
            return fd;

    fn = pa_sprintf_malloc("/dev/mixer%i", n);
    fd = open_mixer(fn);
    pa_xfree(fn);

    if (fd < 0)
        pa_log_warn("Failed to open mixer '%s': %s", device, pa_cstrerror(errno));

    return fd;
}
