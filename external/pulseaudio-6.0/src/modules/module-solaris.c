/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2006-2007 Pierre Ossman <ossman@cendio.se> for Cendio AB
  Copyright 2009 Finn Thain

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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <signal.h>
#include <stropts.h>
#include <sys/conf.h>
#include <sys/audio.h>

#include <pulse/mainloop-signal.h>
#include <pulse/xmalloc.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/rtclock.h>

#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/module.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/core-error.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/thread.h>
#include <pulsecore/time-smoother.h>

#include "module-solaris-symdef.h"

PA_MODULE_AUTHOR("Pierre Ossman");
PA_MODULE_DESCRIPTION("Solaris Sink/Source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_USAGE(
    "sink_name=<name for the sink> "
    "sink_properties=<properties for the sink> "
    "source_name=<name for the source> "
    "source_properties=<properties for the source> "
    "device=<audio device file name> "
    "record=<enable source?> "
    "playback=<enable sink?> "
    "format=<sample format> "
    "channels=<number of channels> "
    "rate=<sample rate> "
    "buffer_length=<milliseconds> "
    "channel_map=<channel map>");
PA_MODULE_LOAD_ONCE(false);

struct userdata {
    pa_core *core;
    pa_sink *sink;
    pa_source *source;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    pa_signal_event *sig;

    pa_memchunk memchunk;

    uint32_t frame_size;
    int32_t buffer_size;
    uint64_t written_bytes, read_bytes;

    char *device_name;
    int mode;
    int fd;
    pa_rtpoll_item *rtpoll_item;
    pa_module *module;

    bool sink_suspended, source_suspended;

    uint32_t play_samples_msw, record_samples_msw;
    uint32_t prev_playback_samples, prev_record_samples;

    int32_t minimum_request;

    pa_smoother *smoother;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "source_name",
    "source_properties",
    "device",
    "record",
    "playback",
    "buffer_length",
    "format",
    "rate",
    "channels",
    "channel_map",
    NULL
};

#define DEFAULT_DEVICE "/dev/audio"

#define MAX_RENDER_HZ   (300)
/* This render rate limit imposes a minimum latency, but without it we waste too much CPU time. */

#define MAX_BUFFER_SIZE (128 * 1024)
/* An attempt to buffer more than 128 KB causes write() to fail with errno == EAGAIN. */

static uint64_t get_playback_buffered_bytes(struct userdata *u) {
    audio_info_t info;
    uint64_t played_bytes;
    int err;

    pa_assert(u->sink);

    err = ioctl(u->fd, AUDIO_GETINFO, &info);
    pa_assert(err >= 0);

    /* Handle wrap-around of the device's sample counter, which is a uint_32. */
    if (u->prev_playback_samples > info.play.samples) {
        /*
         * Unfortunately info.play.samples can sometimes go backwards, even before it wraps!
         * The bug seems to be absent on Solaris x86 nv117 with audio810 driver, at least on this (UP) machine.
         * The bug is present on a different (SMP) machine running Solaris x86 nv103 with audioens driver.
         * An earlier revision of this file mentions the same bug independently (unknown configuration).
         */
        if (u->prev_playback_samples + info.play.samples < 240000) {
            ++u->play_samples_msw;
        } else {
            pa_log_debug("play.samples went backwards %d bytes", u->prev_playback_samples - info.play.samples);
        }
    }
    u->prev_playback_samples = info.play.samples;
    played_bytes = (((uint64_t)u->play_samples_msw << 32) + info.play.samples) * u->frame_size;

    pa_smoother_put(u->smoother, pa_rtclock_now(), pa_bytes_to_usec(played_bytes, &u->sink->sample_spec));

    if (u->written_bytes > played_bytes)
        return u->written_bytes - played_bytes;
    else
        return 0;
}

static pa_usec_t sink_get_latency(struct userdata *u, pa_sample_spec *ss) {
    pa_usec_t r = 0;

    pa_assert(u);
    pa_assert(ss);

    if (u->fd >= 0) {
        r = pa_bytes_to_usec(get_playback_buffered_bytes(u), ss);
        if (u->memchunk.memblock)
            r += pa_bytes_to_usec(u->memchunk.length, ss);
    }
    return r;
}

static uint64_t get_recorded_bytes(struct userdata *u) {
    audio_info_t info;
    uint64_t result;
    int err;

    pa_assert(u->source);

    err = ioctl(u->fd, AUDIO_GETINFO, &info);
    pa_assert(err >= 0);

    if (u->prev_record_samples > info.record.samples)
        ++u->record_samples_msw;
    u->prev_record_samples = info.record.samples;
    result = (((uint64_t)u->record_samples_msw << 32) + info.record.samples) * u->frame_size;

    return result;
}

static pa_usec_t source_get_latency(struct userdata *u, pa_sample_spec *ss) {
    pa_usec_t r = 0;
    audio_info_t info;

    pa_assert(u);
    pa_assert(ss);

    if (u->fd) {
        int err = ioctl(u->fd, AUDIO_GETINFO, &info);
        pa_assert(err >= 0);

        r = pa_bytes_to_usec(get_recorded_bytes(u), ss) - pa_bytes_to_usec(u->read_bytes, ss);
    }
    return r;
}

static void build_pollfd(struct userdata *u) {
    struct pollfd *pollfd;

    pa_assert(u);
    pa_assert(!u->rtpoll_item);
    u->rtpoll_item = pa_rtpoll_item_new(u->rtpoll, PA_RTPOLL_NEVER, 1);

    pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);
    pollfd->fd = u->fd;
    pollfd->events = 0;
    pollfd->revents = 0;
}

static int set_buffer(int fd, int buffer_size) {
    audio_info_t info;

    pa_assert(fd >= 0);

    AUDIO_INITINFO(&info);
    info.play.buffer_size = buffer_size;
    info.record.buffer_size = buffer_size;

    if (ioctl(fd, AUDIO_SETINFO, &info) < 0) {
        if (errno == EINVAL)
            pa_log("AUDIO_SETINFO: Unsupported buffer size.");
        else
            pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        return -1;
    }

    return 0;
}

static int auto_format(int fd, int mode, pa_sample_spec *ss) {
    audio_info_t info;

    pa_assert(fd >= 0);
    pa_assert(ss);

    AUDIO_INITINFO(&info);

    if (mode != O_RDONLY) {
        info.play.sample_rate = ss->rate;
        info.play.channels = ss->channels;
        switch (ss->format) {
        case PA_SAMPLE_U8:
            info.play.precision = 8;
            info.play.encoding = AUDIO_ENCODING_LINEAR;
            break;
        case PA_SAMPLE_ALAW:
            info.play.precision = 8;
            info.play.encoding = AUDIO_ENCODING_ALAW;
            break;
        case PA_SAMPLE_ULAW:
            info.play.precision = 8;
            info.play.encoding = AUDIO_ENCODING_ULAW;
            break;
        case PA_SAMPLE_S16NE:
            info.play.precision = 16;
            info.play.encoding = AUDIO_ENCODING_LINEAR;
            break;
        default:
            pa_log("AUDIO_SETINFO: Unsupported sample format.");
            return -1;
        }
    }

    if (mode != O_WRONLY) {
        info.record.sample_rate = ss->rate;
        info.record.channels = ss->channels;
        switch (ss->format) {
        case PA_SAMPLE_U8:
            info.record.precision = 8;
            info.record.encoding = AUDIO_ENCODING_LINEAR;
            break;
        case PA_SAMPLE_ALAW:
            info.record.precision = 8;
            info.record.encoding = AUDIO_ENCODING_ALAW;
            break;
        case PA_SAMPLE_ULAW:
            info.record.precision = 8;
            info.record.encoding = AUDIO_ENCODING_ULAW;
            break;
        case PA_SAMPLE_S16NE:
            info.record.precision = 16;
            info.record.encoding = AUDIO_ENCODING_LINEAR;
            break;
        default:
            pa_log("AUDIO_SETINFO: Unsupported sample format.");
            return -1;
        }
    }

    if (ioctl(fd, AUDIO_SETINFO, &info) < 0) {
        if (errno == EINVAL)
            pa_log("AUDIO_SETINFO: Failed to set sample format.");
        else
            pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        return -1;
    }

    return 0;
}

static int open_audio_device(struct userdata *u, pa_sample_spec *ss) {
    pa_assert(u);
    pa_assert(ss);

    if ((u->fd = pa_open_cloexec(u->device_name, u->mode | O_NONBLOCK, 0)) < 0) {
        pa_log_warn("open %s failed (%s)", u->device_name, pa_cstrerror(errno));
        return -1;
    }

    pa_log_info("device opened in %s mode.", u->mode == O_WRONLY ? "O_WRONLY" : (u->mode == O_RDONLY ? "O_RDONLY" : "O_RDWR"));

    if (auto_format(u->fd, u->mode, ss) < 0)
        return -1;

    if (set_buffer(u->fd, u->buffer_size) < 0)
        return -1;

    u->written_bytes = u->read_bytes = 0;
    u->play_samples_msw = u->record_samples_msw = 0;
    u->prev_playback_samples = u->prev_record_samples = 0;

    return u->fd;
}

static int suspend(struct userdata *u) {
    pa_assert(u);
    pa_assert(u->fd >= 0);

    pa_log_info("Suspending...");

    ioctl(u->fd, I_FLUSH, FLUSHRW);
    pa_close(u->fd);
    u->fd = -1;

    if (u->rtpoll_item) {
        pa_rtpoll_item_free(u->rtpoll_item);
        u->rtpoll_item = NULL;
    }

    pa_log_info("Device suspended.");

    return 0;
}

static int unsuspend(struct userdata *u) {
    pa_assert(u);
    pa_assert(u->fd < 0);

    pa_log_info("Resuming...");

    if (open_audio_device(u, u->sink ? &u->sink->sample_spec : &u->source->sample_spec) < 0)
        return -1;

    build_pollfd(u);

    pa_log_info("Device resumed.");

    return 0;
}

static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY:
            *((pa_usec_t*) data) = sink_get_latency(u, &PA_SINK(o)->sample_spec);
            return 0;

        case PA_SINK_MESSAGE_SET_STATE:

            switch ((pa_sink_state_t) PA_PTR_TO_UINT(data)) {

                case PA_SINK_SUSPENDED:

                    pa_assert(PA_SINK_IS_OPENED(u->sink->thread_info.state));

                    pa_smoother_pause(u->smoother, pa_rtclock_now());

                    if (!u->source || u->source_suspended) {
                        if (suspend(u) < 0)
                            return -1;
                    }
                    u->sink_suspended = true;
                    break;

                case PA_SINK_IDLE:
                case PA_SINK_RUNNING:

                    if (u->sink->thread_info.state == PA_SINK_SUSPENDED) {
                        pa_smoother_resume(u->smoother, pa_rtclock_now(), true);

                        if (!u->source || u->source_suspended) {
                            if (unsuspend(u) < 0)
                                return -1;
                            u->sink->get_volume(u->sink);
                            u->sink->get_mute(u->sink);
                        }
                        u->sink_suspended = false;
                    }
                    break;

                case PA_SINK_INVALID_STATE:
                case PA_SINK_UNLINKED:
                case PA_SINK_INIT:
                    ;
            }

            break;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {

        case PA_SOURCE_MESSAGE_GET_LATENCY:
            *((pa_usec_t*) data) = source_get_latency(u, &PA_SOURCE(o)->sample_spec);
            return 0;

        case PA_SOURCE_MESSAGE_SET_STATE:

            switch ((pa_source_state_t) PA_PTR_TO_UINT(data)) {

                case PA_SOURCE_SUSPENDED:

                    pa_assert(PA_SOURCE_IS_OPENED(u->source->thread_info.state));

                    if (!u->sink || u->sink_suspended) {
                        if (suspend(u) < 0)
                            return -1;
                    }
                    u->source_suspended = true;
                    break;

                case PA_SOURCE_IDLE:
                case PA_SOURCE_RUNNING:

                    if (u->source->thread_info.state == PA_SOURCE_SUSPENDED) {
                        if (!u->sink || u->sink_suspended) {
                            if (unsuspend(u) < 0)
                                return -1;
                            u->source->get_volume(u->source);
                        }
                        u->source_suspended = false;
                    }
                    break;

                case PA_SOURCE_UNLINKED:
                case PA_SOURCE_INIT:
                case PA_SOURCE_INVALID_STATE:
                    ;

            }
            break;

    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

static void sink_set_volume(pa_sink *s) {
    struct userdata *u;
    audio_info_t info;

    pa_assert_se(u = s->userdata);

    if (u->fd >= 0) {
        AUDIO_INITINFO(&info);

        info.play.gain = pa_cvolume_max(&s->real_volume) * AUDIO_MAX_GAIN / PA_VOLUME_NORM;
        pa_assert(info.play.gain <= AUDIO_MAX_GAIN);

        if (ioctl(u->fd, AUDIO_SETINFO, &info) < 0) {
            if (errno == EINVAL)
                pa_log("AUDIO_SETINFO: Unsupported volume.");
            else
                pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        }
    }
}

static void sink_get_volume(pa_sink *s) {
    struct userdata *u;
    audio_info_t info;

    pa_assert_se(u = s->userdata);

    if (u->fd >= 0) {
        if (ioctl(u->fd, AUDIO_GETINFO, &info) < 0)
            pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        else
            pa_cvolume_set(&s->real_volume, s->sample_spec.channels, info.play.gain * PA_VOLUME_NORM / AUDIO_MAX_GAIN);
    }
}

static void source_set_volume(pa_source *s) {
    struct userdata *u;
    audio_info_t info;

    pa_assert_se(u = s->userdata);

    if (u->fd >= 0) {
        AUDIO_INITINFO(&info);

        info.play.gain = pa_cvolume_max(&s->real_volume) * AUDIO_MAX_GAIN / PA_VOLUME_NORM;
        pa_assert(info.play.gain <= AUDIO_MAX_GAIN);

        if (ioctl(u->fd, AUDIO_SETINFO, &info) < 0) {
            if (errno == EINVAL)
                pa_log("AUDIO_SETINFO: Unsupported volume.");
            else
                pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        }
    }
}

static void source_get_volume(pa_source *s) {
    struct userdata *u;
    audio_info_t info;

    pa_assert_se(u = s->userdata);

    if (u->fd >= 0) {
        if (ioctl(u->fd, AUDIO_GETINFO, &info) < 0)
            pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
        else
            pa_cvolume_set(&s->real_volume, s->sample_spec.channels, info.play.gain * PA_VOLUME_NORM / AUDIO_MAX_GAIN);
    }
}

static void sink_set_mute(pa_sink *s) {
    struct userdata *u = s->userdata;
    audio_info_t info;

    pa_assert(u);

    if (u->fd >= 0) {
        AUDIO_INITINFO(&info);

        info.output_muted = s->muted;

        if (ioctl(u->fd, AUDIO_SETINFO, &info) < 0)
            pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
    }
}

static int sink_get_mute(pa_sink *s, bool *mute) {
    struct userdata *u = s->userdata;
    audio_info_t info;

    pa_assert(u);

    if (u->fd < 0)
        return -1;

    if (ioctl(u->fd, AUDIO_GETINFO, &info) < 0) {
        pa_log("AUDIO_GETINFO: %s", pa_cstrerror(errno));
        return -1;
    }

    *mute = info.output_muted;

    return 0;
}

static void process_rewind(struct userdata *u) {
    size_t rewind_nbytes;

    pa_assert(u);

    if (!PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
        pa_sink_process_rewind(u->sink, 0);
        return;
    }

    rewind_nbytes = u->sink->thread_info.rewind_nbytes;

    if (rewind_nbytes > 0) {
        pa_log_debug("Requested to rewind %lu bytes.", (unsigned long) rewind_nbytes);
        rewind_nbytes = PA_MIN(u->memchunk.length, rewind_nbytes);
        u->memchunk.length -= rewind_nbytes;
        if (u->memchunk.length <= 0 && u->memchunk.memblock) {
            pa_memblock_unref(u->memchunk.memblock);
            pa_memchunk_reset(&u->memchunk);
        }
        pa_log_debug("Rewound %lu bytes.", (unsigned long) rewind_nbytes);
    }

    pa_sink_process_rewind(u->sink, rewind_nbytes);
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    unsigned short revents = 0;
    int ret, err;
    audio_info_t info;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    pa_smoother_set_time_offset(u->smoother, pa_rtclock_now());

    for (;;) {
        /* Render some data and write it to the dsp */

        if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
            process_rewind(u);

        if (u->sink && PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
            pa_usec_t xtime0, ysleep_interval, xsleep_interval;
            uint64_t buffered_bytes;

            err = ioctl(u->fd, AUDIO_GETINFO, &info);
            if (err < 0) {
                pa_log("AUDIO_GETINFO ioctl failed: %s", pa_cstrerror(errno));
                goto fail;
            }

            if (info.play.error) {
                pa_log_debug("buffer under-run!");

                AUDIO_INITINFO(&info);
                info.play.error = 0;
                if (ioctl(u->fd, AUDIO_SETINFO, &info) < 0)
                    pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));

                pa_smoother_reset(u->smoother, pa_rtclock_now(), true);
            }

            for (;;) {
                void *p;
                ssize_t w;
                size_t len;
                int write_type = 1;

                /*
                 * Since we cannot modify the size of the output buffer we fake it
                 * by not filling it more than u->buffer_size.
                 */
                xtime0 = pa_rtclock_now();
                buffered_bytes = get_playback_buffered_bytes(u);
                if (buffered_bytes >= (uint64_t)u->buffer_size)
                    break;

                len = u->buffer_size - buffered_bytes;
                len -= len % u->frame_size;

                if (len < (size_t) u->minimum_request)
                    break;

                if (!u->memchunk.length)
                    pa_sink_render(u->sink, u->sink->thread_info.max_request, &u->memchunk);

                len = PA_MIN(u->memchunk.length, len);

                p = pa_memblock_acquire(u->memchunk.memblock);
                w = pa_write(u->fd, (uint8_t*) p + u->memchunk.index, len, &write_type);
                pa_memblock_release(u->memchunk.memblock);

                if (w <= 0) {
                    if (errno == EINTR) {
                        continue;
                    } else if (errno == EAGAIN) {
                        /* We may have realtime priority so yield the CPU to ensure that fd can become writable again. */
                        pa_log_debug("EAGAIN with %llu bytes buffered.", buffered_bytes);
                        break;
                    } else {
                        pa_log("Failed to write data to DSP: %s", pa_cstrerror(errno));
                        goto fail;
                    }
                } else {
                    pa_assert(w % u->frame_size == 0);

                    u->written_bytes += w;
                    u->memchunk.index += w;
                    u->memchunk.length -= w;
                    if (u->memchunk.length <= 0) {
                        pa_memblock_unref(u->memchunk.memblock);
                        pa_memchunk_reset(&u->memchunk);
                    }
                }
            }

            ysleep_interval = pa_bytes_to_usec(buffered_bytes / 2, &u->sink->sample_spec);
            xsleep_interval = pa_smoother_translate(u->smoother, xtime0, ysleep_interval);
            pa_rtpoll_set_timer_absolute(u->rtpoll, xtime0 + PA_MIN(xsleep_interval, ysleep_interval));
        } else
            pa_rtpoll_set_timer_disabled(u->rtpoll);

        /* Try to read some data and pass it on to the source driver */

        if (u->source && PA_SOURCE_IS_OPENED(u->source->thread_info.state) && (revents & POLLIN)) {
            pa_memchunk memchunk;
            void *p;
            ssize_t r;
            size_t len;

            err = ioctl(u->fd, AUDIO_GETINFO, &info);
            pa_assert(err >= 0);

            if (info.record.error) {
                pa_log_debug("buffer overflow!");

                AUDIO_INITINFO(&info);
                info.record.error = 0;
                if (ioctl(u->fd, AUDIO_SETINFO, &info) < 0)
                    pa_log("AUDIO_SETINFO: %s", pa_cstrerror(errno));
            }

            err = ioctl(u->fd, I_NREAD, &len);
            pa_assert(err >= 0);

            if (len > 0) {
                memchunk.memblock = pa_memblock_new(u->core->mempool, len);
                pa_assert(memchunk.memblock);

                p = pa_memblock_acquire(memchunk.memblock);
                r = pa_read(u->fd, p, len, NULL);
                pa_memblock_release(memchunk.memblock);

                if (r < 0) {
                    pa_memblock_unref(memchunk.memblock);
                    if (errno == EAGAIN)
                        break;
                    else {
                        pa_log("Failed to read data from DSP: %s", pa_cstrerror(errno));
                        goto fail;
                    }
                } else {
                    u->read_bytes += r;

                    memchunk.index = 0;
                    memchunk.length = r;

                    pa_source_post(u->source, &memchunk);
                    pa_memblock_unref(memchunk.memblock);

                    revents &= ~POLLIN;
                }
            }
        }

        if (u->rtpoll_item) {
            struct pollfd *pollfd;

            pa_assert(u->fd >= 0);

            pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);
            pollfd->events = (u->source && PA_SOURCE_IS_OPENED(u->source->thread_info.state)) ? POLLIN : 0;
        }

        /* Hmm, nothing to do. Let's sleep */
        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (ret == 0)
            goto finish;

        if (u->rtpoll_item) {
            struct pollfd *pollfd;

            pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);

            if (pollfd->revents & ~(POLLOUT|POLLIN)) {
                pa_log("DSP shutdown.");
                goto fail;
            }

            revents = pollfd->revents;
        } else
            revents = 0;
    }

fail:
    /* We have to continue processing messages until we receive the
     * SHUTDOWN message */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

static void sig_callback(pa_mainloop_api *api, pa_signal_event*e, int sig, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    pa_log_debug("caught signal");

    if (u->sink) {
        pa_sink_get_volume(u->sink, true);
        pa_sink_get_mute(u->sink, true);
    }

    if (u->source)
        pa_source_get_volume(u->source, true);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    bool record = true, playback = true;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    uint32_t buffer_length_msec;
    int fd = -1;
    pa_sink_new_data sink_new_data;
    pa_source_new_data source_new_data;
    char const *name;
    char *name_buf;
    bool namereg_fail;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "record", &record) < 0 || pa_modargs_get_value_boolean(ma, "playback", &playback) < 0) {
        pa_log("record= and playback= expect a boolean argument.");
        goto fail;
    }

    if (!playback && !record) {
        pa_log("neither playback nor record enabled for device.");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);

    if (!(u->smoother = pa_smoother_new(PA_USEC_PER_SEC, PA_USEC_PER_SEC * 2, true, true, 10, pa_rtclock_now(), true)))
        goto fail;

    /*
     * For a process (or several processes) to use the same audio device for both
     * record and playback at the same time, the device's mixer must be enabled.
     * See mixerctl(1). It may be turned off for playback only or record only.
     */
    u->mode = (playback && record) ? O_RDWR : (playback ? O_WRONLY : (record ? O_RDONLY : 0));

    ss = m->core->default_sample_spec;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("failed to parse sample specification");
        goto fail;
    }
    u->frame_size = pa_frame_size(&ss);

    u->minimum_request = pa_usec_to_bytes(PA_USEC_PER_SEC / MAX_RENDER_HZ, &ss);

    buffer_length_msec = 100;
    if (pa_modargs_get_value_u32(ma, "buffer_length", &buffer_length_msec) < 0) {
        pa_log("failed to parse buffer_length argument");
        goto fail;
    }
    u->buffer_size = pa_usec_to_bytes(1000 * buffer_length_msec, &ss);
    if (u->buffer_size < 2 * u->minimum_request) {
        pa_log("buffer_length argument cannot be smaller than %u",
               (unsigned)(pa_bytes_to_usec(2 * u->minimum_request, &ss) / 1000));
        goto fail;
    }
    if (u->buffer_size > MAX_BUFFER_SIZE) {
        pa_log("buffer_length argument cannot be greater than %u",
               (unsigned)(pa_bytes_to_usec(MAX_BUFFER_SIZE, &ss) / 1000));
        goto fail;
    }

    u->device_name = pa_xstrdup(pa_modargs_get_value(ma, "device", DEFAULT_DEVICE));

    if ((fd = open_audio_device(u, &ss)) < 0)
        goto fail;

    u->core = m->core;
    u->module = m;
    m->userdata = u;

    pa_memchunk_reset(&u->memchunk);

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    u->rtpoll_item = NULL;
    build_pollfd(u);

    if (u->mode != O_WRONLY) {
        name_buf = NULL;
        namereg_fail = true;

        if (!(name = pa_modargs_get_value(ma, "source_name", NULL))) {
            name = name_buf = pa_sprintf_malloc("solaris_input.%s", pa_path_get_filename(u->device_name));
            namereg_fail = false;
        }

        pa_source_new_data_init(&source_new_data);
        source_new_data.driver = __FILE__;
        source_new_data.module = m;
        pa_source_new_data_set_name(&source_new_data, name);
        source_new_data.namereg_fail = namereg_fail;
        pa_source_new_data_set_sample_spec(&source_new_data, &ss);
        pa_source_new_data_set_channel_map(&source_new_data, &map);
        pa_proplist_sets(source_new_data.proplist, PA_PROP_DEVICE_STRING, u->device_name);
        pa_proplist_sets(source_new_data.proplist, PA_PROP_DEVICE_API, "solaris");
        pa_proplist_sets(source_new_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Solaris PCM source");
        pa_proplist_sets(source_new_data.proplist, PA_PROP_DEVICE_ACCESS_MODE, "serial");
        pa_proplist_setf(source_new_data.proplist, PA_PROP_DEVICE_BUFFERING_BUFFER_SIZE, "%lu", (unsigned long) u->buffer_size);

        if (pa_modargs_get_proplist(ma, "source_properties", source_new_data.proplist, PA_UPDATE_REPLACE) < 0) {
            pa_log("Invalid properties");
            pa_source_new_data_done(&source_new_data);
            goto fail;
        }

        u->source = pa_source_new(m->core, &source_new_data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY);
        pa_source_new_data_done(&source_new_data);
        pa_xfree(name_buf);

        if (!u->source) {
            pa_log("Failed to create source object");
            goto fail;
        }

        u->source->userdata = u;
        u->source->parent.process_msg = source_process_msg;

        pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
        pa_source_set_rtpoll(u->source, u->rtpoll);
        pa_source_set_fixed_latency(u->source, pa_bytes_to_usec(u->buffer_size, &u->source->sample_spec));

        pa_source_set_get_volume_callback(u->source, source_get_volume);
        pa_source_set_set_volume_callback(u->source, source_set_volume);
        u->source->refresh_volume = true;
    } else
        u->source = NULL;

    if (u->mode != O_RDONLY) {
        name_buf = NULL;
        namereg_fail = true;
        if (!(name = pa_modargs_get_value(ma, "sink_name", NULL))) {
            name = name_buf = pa_sprintf_malloc("solaris_output.%s", pa_path_get_filename(u->device_name));
            namereg_fail = false;
        }

        pa_sink_new_data_init(&sink_new_data);
        sink_new_data.driver = __FILE__;
        sink_new_data.module = m;
        pa_sink_new_data_set_name(&sink_new_data, name);
        sink_new_data.namereg_fail = namereg_fail;
        pa_sink_new_data_set_sample_spec(&sink_new_data, &ss);
        pa_sink_new_data_set_channel_map(&sink_new_data, &map);
        pa_proplist_sets(sink_new_data.proplist, PA_PROP_DEVICE_STRING, u->device_name);
        pa_proplist_sets(sink_new_data.proplist, PA_PROP_DEVICE_API, "solaris");
        pa_proplist_sets(sink_new_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Solaris PCM sink");
        pa_proplist_sets(sink_new_data.proplist, PA_PROP_DEVICE_ACCESS_MODE, "serial");

        if (pa_modargs_get_proplist(ma, "sink_properties", sink_new_data.proplist, PA_UPDATE_REPLACE) < 0) {
            pa_log("Invalid properties");
            pa_sink_new_data_done(&sink_new_data);
            goto fail;
        }

        u->sink = pa_sink_new(m->core, &sink_new_data, PA_SINK_HARDWARE|PA_SINK_LATENCY);
        pa_sink_new_data_done(&sink_new_data);

        pa_assert(u->sink);
        u->sink->userdata = u;
        u->sink->parent.process_msg = sink_process_msg;

        pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
        pa_sink_set_rtpoll(u->sink, u->rtpoll);
        pa_sink_set_fixed_latency(u->sink, pa_bytes_to_usec(u->buffer_size, &u->sink->sample_spec));
        pa_sink_set_max_request(u->sink, u->buffer_size);
        pa_sink_set_max_rewind(u->sink, u->buffer_size);

        pa_sink_set_get_volume_callback(u->sink, sink_get_volume);
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume);
        pa_sink_set_get_mute_callback(u->sink, sink_get_mute);
        pa_sink_set_set_mute_callback(u->sink, sink_set_mute);
        u->sink->refresh_volume = u->sink->refresh_muted = true;
    } else
        u->sink = NULL;

    pa_assert(u->source || u->sink);

    u->sig = pa_signal_new(SIGPOLL, sig_callback, u);
    if (u->sig)
        ioctl(u->fd, I_SETSIG, S_MSG);
    else
        pa_log_warn("Could not register SIGPOLL handler");

    if (!(u->thread = pa_thread_new("solaris", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    /* Read mixer settings */
    if (u->sink) {
        if (sink_new_data.volume_is_set)
            u->sink->set_volume(u->sink);
        else
            u->sink->get_volume(u->sink);

        if (sink_new_data.muted_is_set)
            u->sink->set_mute(u->sink);
        else
            u->sink->get_mute(u->sink);

        pa_sink_put(u->sink);
    }

    if (u->source) {
        if (source_new_data.volume_is_set)
            u->source->set_volume(u->source);
        else
            u->source->get_volume(u->source);

        pa_source_put(u->source);
    }

    pa_modargs_free(ma);

    return 0;

fail:
    if (u)
        pa__done(m);
    else if (fd >= 0)
        close(fd);

    if (ma)
        pa_modargs_free(ma);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sig) {
        ioctl(u->fd, I_SETSIG, 0);
        pa_signal_free(u->sig);
    }

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->source)
        pa_source_unlink(u->source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->source)
        pa_source_unref(u->source);

    if (u->memchunk.memblock)
        pa_memblock_unref(u->memchunk.memblock);

    if (u->rtpoll_item)
        pa_rtpoll_item_free(u->rtpoll_item);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    if (u->fd >= 0)
        close(u->fd);

    if (u->smoother)
        pa_smoother_free(u->smoother);

    pa_xfree(u->device_name);

    pa_xfree(u);
}
