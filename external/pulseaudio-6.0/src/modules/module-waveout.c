/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2006-2007 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <windows.h>
#include <mmsystem.h>

#include <pulse/xmalloc.h>
#include <pulse/timeval.h>

#include <pulsecore/sink.h>
#include <pulsecore/source.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>

#include "module-waveout-symdef.h"

PA_MODULE_AUTHOR("Pierre Ossman");
PA_MODULE_DESCRIPTION("Windows waveOut Sink/Source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_USAGE(
    "sink_name=<name for the sink> "
    "source_name=<name for the source> "
    "device=<device number> "
    "device_name=<name of the device> "
    "record=<enable source?> "
    "playback=<enable sink?> "
    "format=<sample format> "
    "rate=<sample rate> "
    "channels=<number of channels> "
    "channel_map=<channel map> "
    "fragments=<number of fragments> "
    "fragment_size=<fragment size>");

#define DEFAULT_SINK_NAME "wave_output"
#define DEFAULT_SOURCE_NAME "wave_input"

#define WAVEOUT_MAX_VOLUME 0xFFFF

struct userdata {
    pa_sink *sink;
    pa_source *source;
    pa_core *core;
    pa_usec_t poll_timeout;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    uint32_t fragments, fragment_size;

    uint32_t free_ofrags, free_ifrags;

    DWORD written_bytes;
    int sink_underflow;

    int cur_ohdr, cur_ihdr;
    WAVEHDR *ohdrs, *ihdrs;

    HWAVEOUT hwo;
    HWAVEIN hwi;
    pa_module *module;

    CRITICAL_SECTION crit;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "source_name",
    "device",
    "device_name",
    "record",
    "playback",
    "fragments",
    "fragment_size",
    "format",
    "rate",
    "channels",
    "channel_map",
    NULL
};

static void do_write(struct userdata *u) {
    uint32_t free_frags;
    pa_memchunk memchunk;
    WAVEHDR *hdr;
    MMRESULT res;
    void *p;

    if (!u->sink)
        return;

    if (!PA_SINK_IS_LINKED(u->sink->state))
        return;

    EnterCriticalSection(&u->crit);
    free_frags = u->free_ofrags;
    LeaveCriticalSection(&u->crit);

    if (!u->sink_underflow && (free_frags == u->fragments))
        pa_log_debug("WaveOut underflow!");

    while (free_frags) {
        hdr = &u->ohdrs[u->cur_ohdr];
        if (hdr->dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(u->hwo, hdr, sizeof(WAVEHDR));

        hdr->dwBufferLength = 0;
        while (hdr->dwBufferLength < u->fragment_size) {
            size_t len;

            len = u->fragment_size - hdr->dwBufferLength;

            pa_sink_render(u->sink, len, &memchunk);

            pa_assert(memchunk.memblock);
            pa_assert(memchunk.length);

            if (memchunk.length < len)
                len = memchunk.length;

            p = pa_memblock_acquire(memchunk.memblock);
            memcpy(hdr->lpData + hdr->dwBufferLength, (char*) p + memchunk.index, len);
            pa_memblock_release(memchunk.memblock);

            hdr->dwBufferLength += len;

            pa_memblock_unref(memchunk.memblock);
            memchunk.memblock = NULL;
        }

        /* Underflow detection */
        if (hdr->dwBufferLength == 0) {
            u->sink_underflow = 1;
            break;
        }
        u->sink_underflow = 0;

        res = waveOutPrepareHeader(u->hwo, hdr, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
            pa_log_error("Unable to prepare waveOut block: %d", res);

        res = waveOutWrite(u->hwo, hdr, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
            pa_log_error("Unable to write waveOut block: %d", res);

        u->written_bytes += hdr->dwBufferLength;

        EnterCriticalSection(&u->crit);
        u->free_ofrags--;
        LeaveCriticalSection(&u->crit);

        free_frags--;
        u->cur_ohdr++;
        u->cur_ohdr %= u->fragments;
    }
}

static void do_read(struct userdata *u) {
    uint32_t free_frags;
    pa_memchunk memchunk;
    WAVEHDR *hdr;
    MMRESULT res;
    void *p;

    if (!u->source)
        return;

    if (!PA_SOURCE_IS_LINKED(u->source->state))
        return;

    EnterCriticalSection(&u->crit);
    free_frags = u->free_ifrags;
    u->free_ifrags = 0;
    LeaveCriticalSection(&u->crit);

    if (free_frags == u->fragments)
        pa_log_debug("WaveIn overflow!");

    while (free_frags) {
        hdr = &u->ihdrs[u->cur_ihdr];
        if (hdr->dwFlags & WHDR_PREPARED)
            waveInUnprepareHeader(u->hwi, hdr, sizeof(WAVEHDR));

        if (hdr->dwBytesRecorded) {
            memchunk.memblock = pa_memblock_new(u->core->mempool, hdr->dwBytesRecorded);
            pa_assert(memchunk.memblock);

            p = pa_memblock_acquire(memchunk.memblock);
            memcpy((char*) p, hdr->lpData, hdr->dwBytesRecorded);
            pa_memblock_release(memchunk.memblock);

            memchunk.length = hdr->dwBytesRecorded;
            memchunk.index = 0;

            pa_source_post(u->source, &memchunk);
            pa_memblock_unref(memchunk.memblock);
        }

        res = waveInPrepareHeader(u->hwi, hdr, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
            pa_log_error("Unable to prepare waveIn block: %d", res);

        res = waveInAddBuffer(u->hwi, hdr, sizeof(WAVEHDR));
        if (res != MMSYSERR_NOERROR)
            pa_log_error("Unable to add waveIn block: %d", res);

        free_frags--;
        u->cur_ihdr++;
        u->cur_ihdr %= u->fragments;
    }
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);
    pa_assert(u->sink || u->source);

    pa_log_debug("Thread starting up");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    for (;;) {
        int ret;
        bool need_timer = false;

        if (u->sink) {
            if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
                pa_sink_process_rewind(u->sink, 0);

            if (PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
                do_write(u);
                need_timer = true;
            }
        }

        if (u->source && PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
            do_read(u);
            need_timer = true;
        }

        if (need_timer)
            pa_rtpoll_set_timer_relative(u->rtpoll, u->poll_timeout);
        else
            pa_rtpoll_set_timer_disabled(u->rtpoll);

        /* Hmm, nothing to do. Let's sleep */
        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (ret == 0)
            goto finish;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

static void CALLBACK chunk_done_cb(HWAVEOUT hwo, UINT msg, DWORD_PTR inst, DWORD param1, DWORD param2) {
    struct userdata *u = (struct userdata*) inst;

    if (msg == WOM_OPEN)
        pa_log_debug("WaveOut subsystem opened.");
    if (msg == WOM_CLOSE)
        pa_log_debug("WaveOut subsystem closed.");
    if (msg != WOM_DONE)
        return;

    EnterCriticalSection(&u->crit);
    u->free_ofrags++;
    pa_assert(u->free_ofrags <= u->fragments);
    LeaveCriticalSection(&u->crit);
}

static void CALLBACK chunk_ready_cb(HWAVEIN hwi, UINT msg, DWORD_PTR inst, DWORD param1, DWORD param2) {
    struct userdata *u = (struct userdata*) inst;

    if (msg == WIM_OPEN)
        pa_log_debug("WaveIn subsystem opened.");
    if (msg == WIM_CLOSE)
        pa_log_debug("WaveIn subsystem closed.");
    if (msg != WIM_DATA)
        return;

    EnterCriticalSection(&u->crit);
    u->free_ifrags++;
    pa_assert(u->free_ifrags <= u->fragments);
    LeaveCriticalSection(&u->crit);
}

static pa_usec_t sink_get_latency(struct userdata *u) {
    uint32_t free_frags;
    MMTIME mmt;
    pa_assert(u);
    pa_assert(u->sink);

    memset(&mmt, 0, sizeof(mmt));
    mmt.wType = TIME_BYTES;
    if (waveOutGetPosition(u->hwo, &mmt, sizeof(mmt)) == MMSYSERR_NOERROR)
        return pa_bytes_to_usec(u->written_bytes - mmt.u.cb, &u->sink->sample_spec);
    else {
        EnterCriticalSection(&u->crit);
        free_frags = u->free_ofrags;
        LeaveCriticalSection(&u->crit);

        return pa_bytes_to_usec((u->fragments - free_frags) * u->fragment_size, &u->sink->sample_spec);
    }
}

static pa_usec_t source_get_latency(struct userdata *u) {
    pa_usec_t r = 0;
    uint32_t free_frags;
    pa_assert(u);
    pa_assert(u->source);

    EnterCriticalSection(&u->crit);
    free_frags = u->free_ifrags;
    LeaveCriticalSection(&u->crit);

    r += pa_bytes_to_usec((free_frags + 1) * u->fragment_size, &u->source->sample_spec);

    return r;
}

static int process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u;

    if (pa_sink_isinstance(o)) {
        u = PA_SINK(o)->userdata;

        switch (code) {

            case PA_SINK_MESSAGE_GET_LATENCY: {
                pa_usec_t r = 0;
                if (u->hwo)
                    r = sink_get_latency(u);
                *((pa_usec_t*) data) = r;
                return 0;
            }

        }

        return pa_sink_process_msg(o, code, data, offset, chunk);
    }

    if (pa_source_isinstance(o)) {
        u = PA_SOURCE(o)->userdata;

        switch (code) {

            case PA_SOURCE_MESSAGE_GET_LATENCY: {
                pa_usec_t r = 0;
                if (u->hwi)
                    r = source_get_latency(u);
                *((pa_usec_t*) data) = r;
                return 0;
            }

        }

        return pa_source_process_msg(o, code, data, offset, chunk);
    }

    return -1;
}

static void sink_get_volume_cb(pa_sink *s) {
    struct userdata *u = s->userdata;
    WAVEOUTCAPS caps;
    DWORD vol;
    pa_volume_t left, right;

    if (waveOutGetDevCaps(u->hwo, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        return;
    if (!(caps.dwSupport & WAVECAPS_VOLUME))
        return;

    if (waveOutGetVolume(u->hwo, &vol) != MMSYSERR_NOERROR)
        return;

    left = PA_CLAMP_VOLUME((vol & 0xFFFF) * PA_VOLUME_NORM / WAVEOUT_MAX_VOLUME);
    if (caps.dwSupport & WAVECAPS_LRVOLUME)
        right = PA_CLAMP_VOLUME(((vol >> 16) & 0xFFFF) * PA_VOLUME_NORM / WAVEOUT_MAX_VOLUME);
    else
        right = left;

    /* Windows supports > 2 channels, except for volume control */
    if (s->real_volume.channels > 2)
        pa_cvolume_set(&s->real_volume, s->real_volume.channels, (left + right)/2);

    s->real_volume.values[0] = left;
    if (s->real_volume.channels > 1)
        s->real_volume.values[1] = right;
}

static void sink_set_volume_cb(pa_sink *s) {
    struct userdata *u = s->userdata;
    WAVEOUTCAPS caps;
    DWORD vol;

    if (waveOutGetDevCaps(u->hwo, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        return;
    if (!(caps.dwSupport & WAVECAPS_VOLUME))
        return;

    if (s->real_volume.channels == 2 && caps.dwSupport & WAVECAPS_LRVOLUME) {
        vol = (s->real_volume.values[0] * WAVEOUT_MAX_VOLUME / PA_VOLUME_NORM)
            | (s->real_volume.values[1] * WAVEOUT_MAX_VOLUME / PA_VOLUME_NORM) << 16;
    } else {
        vol = (pa_cvolume_avg(&(s->real_volume)) * WAVEOUT_MAX_VOLUME / PA_VOLUME_NORM)
            | (pa_cvolume_avg(&(s->real_volume)) * WAVEOUT_MAX_VOLUME / PA_VOLUME_NORM) << 16;
    }

    if (waveOutSetVolume(u->hwo, vol) != MMSYSERR_NOERROR)
        return;
}

static int ss_to_waveformat(pa_sample_spec *ss, LPWAVEFORMATEX wf) {
    wf->wFormatTag = WAVE_FORMAT_PCM;

    if (ss->channels > 2) {
        pa_log_error("More than two channels not supported.");
        return -1;
    }

    wf->nChannels = ss->channels;

    wf->nSamplesPerSec = ss->rate;

    if (ss->format == PA_SAMPLE_U8)
        wf->wBitsPerSample = 8;
    else if (ss->format == PA_SAMPLE_S16NE)
        wf->wBitsPerSample = 16;
    else {
        pa_log_error("Unsupported sample format, only u8 and s16 are supported.");
        return -1;
    }

    wf->nBlockAlign = wf->nChannels * wf->wBitsPerSample/8;
    wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->nBlockAlign;

    wf->cbSize = 0;

    return 0;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;
    pa_assert(m);
    pa_assert(m->userdata);
    u = (struct userdata*) m->userdata;

    return (u->sink ? pa_sink_used_by(u->sink) : 0) +
           (u->source ? pa_source_used_by(u->source) : 0);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    HWAVEOUT hwo = INVALID_HANDLE_VALUE;
    HWAVEIN hwi = INVALID_HANDLE_VALUE;
    WAVEFORMATEX wf;
    WAVEOUTCAPS pwoc;
    MMRESULT result;
    int nfrags, frag_size;
    bool record = true, playback = true;
    unsigned int device;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    const char *device_name = NULL;
    unsigned int i;

    pa_assert(m);
    pa_assert(m->core);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "record", &record) < 0 || pa_modargs_get_value_boolean(ma, "playback", &playback) < 0) {
        pa_log("record= and playback= expect boolean argument.");
        goto fail;
    }

    if (!playback && !record) {
        pa_log("neither playback nor record enabled for device.");
        goto fail;
    }

    /* Set the device to be opened.  If set device_name is used,
     * else device if set and lastly WAVE_MAPPER is the default */
    device = WAVE_MAPPER;
    if (pa_modargs_get_value_u32(ma, "device", &device) < 0) {
        pa_log("failed to parse device argument");
        goto fail;
    }
    if ((device_name = pa_modargs_get_value(ma, "device_name", NULL)) != NULL) {
        unsigned int num_devices = waveOutGetNumDevs();
        for (i = 0; i < num_devices; i++) {
            if (waveOutGetDevCaps(i, &pwoc, sizeof(pwoc)) == MMSYSERR_NOERROR)
                if (_stricmp(device_name, pwoc.szPname) == 0)
                    break;
        }
        if (i < num_devices)
            device = i;
        else {
            pa_log("device not found: %s", device_name);
            goto fail;
        }
    }
    if (waveOutGetDevCaps(device, &pwoc, sizeof(pwoc)) == MMSYSERR_NOERROR)
        device_name = pwoc.szPname;
    else
        device_name = "unknown";

    nfrags = 5;
    frag_size = 8192;
    if (pa_modargs_get_value_s32(ma, "fragments", &nfrags) < 0 || pa_modargs_get_value_s32(ma, "fragment_size", &frag_size) < 0) {
        pa_log("failed to parse fragments arguments");
        goto fail;
    }

    ss = m->core->default_sample_spec;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_WAVEEX) < 0) {
        pa_log("failed to parse sample specification");
        goto fail;
    }

    if (ss_to_waveformat(&ss, &wf) < 0)
        goto fail;

    u = pa_xmalloc(sizeof(struct userdata));

    if (record) {
        result = waveInOpen(&hwi, device, &wf, 0, 0, WAVE_FORMAT_DIRECT | WAVE_FORMAT_QUERY);
        if (result != MMSYSERR_NOERROR) {
            pa_log_warn("Sample spec not supported by WaveIn, falling back to default sample rate.");
            ss.rate = wf.nSamplesPerSec = m->core->default_sample_spec.rate;
        }
        result = waveInOpen(&hwi, device, &wf, (DWORD_PTR) chunk_ready_cb, (DWORD_PTR) u, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            char errortext[MAXERRORLENGTH];
            pa_log("Failed to open WaveIn.");
            if (waveInGetErrorText(result, errortext, sizeof(errortext)) == MMSYSERR_NOERROR)
                pa_log("Error: %s", errortext);
            goto fail;
        }
        if (waveInStart(hwi) != MMSYSERR_NOERROR) {
            pa_log("failed to start waveIn");
            goto fail;
        }
    }

    if (playback) {
        result = waveOutOpen(&hwo, device, &wf, 0, 0, WAVE_FORMAT_DIRECT | WAVE_FORMAT_QUERY);
        if (result != MMSYSERR_NOERROR) {
            pa_log_warn("Sample spec not supported by WaveOut, falling back to default sample rate.");
            ss.rate = wf.nSamplesPerSec = m->core->default_sample_spec.rate;
        }
        result = waveOutOpen(&hwo, device, &wf, (DWORD_PTR) chunk_done_cb, (DWORD_PTR) u, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            char errortext[MAXERRORLENGTH];
            pa_log("Failed to open WaveOut.");
            if (waveOutGetErrorText(result, errortext, sizeof(errortext)) == MMSYSERR_NOERROR)
                pa_log("Error: %s", errortext);
            goto fail;
        }
    }

    InitializeCriticalSection(&u->crit);

    if (hwi != INVALID_HANDLE_VALUE) {
        pa_source_new_data data;
        pa_source_new_data_init(&data);
        data.driver = __FILE__;
        data.module = m;
        pa_source_new_data_set_sample_spec(&data, &ss);
        pa_source_new_data_set_channel_map(&data, &map);
        pa_source_new_data_set_name(&data, pa_modargs_get_value(ma, "source_name", DEFAULT_SOURCE_NAME));
        pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "WaveIn on %s", device_name);
        u->source = pa_source_new(m->core, &data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY);
        pa_source_new_data_done(&data);

        pa_assert(u->source);
        u->source->userdata = u;
        u->source->parent.process_msg = process_msg;
    } else
        u->source = NULL;

    if (hwo != INVALID_HANDLE_VALUE) {
        pa_sink_new_data data;
        pa_sink_new_data_init(&data);
        data.driver = __FILE__;
        data.module = m;
        pa_sink_new_data_set_sample_spec(&data, &ss);
        pa_sink_new_data_set_channel_map(&data, &map);
        pa_sink_new_data_set_name(&data, pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
        pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "WaveOut on %s", device_name);
        u->sink = pa_sink_new(m->core, &data, PA_SINK_HARDWARE|PA_SINK_LATENCY);
        pa_sink_new_data_done(&data);

        pa_assert(u->sink);
        pa_sink_set_get_volume_callback(u->sink, sink_get_volume_cb);
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        u->sink->userdata = u;
        u->sink->parent.process_msg = process_msg;
    } else
        u->sink = NULL;

    pa_assert(u->source || u->sink);
    pa_modargs_free(ma);

    u->core = m->core;
    u->hwi = hwi;
    u->hwo = hwo;

    u->fragments = nfrags;
    u->free_ifrags = u->fragments;
    u->free_ofrags = u->fragments;
    u->fragment_size = frag_size - (frag_size % pa_frame_size(&ss));

    u->written_bytes = 0;
    u->sink_underflow = 1;

    u->poll_timeout = pa_bytes_to_usec(u->fragments * u->fragment_size / 10, &ss);
    pa_log_debug("Poll timeout = %.1f ms", (double) u->poll_timeout / PA_USEC_PER_MSEC);

    u->cur_ihdr = 0;
    u->cur_ohdr = 0;
    u->ihdrs = pa_xmalloc0(sizeof(WAVEHDR) * u->fragments);
    pa_assert(u->ihdrs);
    u->ohdrs = pa_xmalloc0(sizeof(WAVEHDR) * u->fragments);
    pa_assert(u->ohdrs);
    for (i = 0; i < u->fragments; i++) {
        u->ihdrs[i].dwBufferLength = u->fragment_size;
        u->ohdrs[i].dwBufferLength = u->fragment_size;
        u->ihdrs[i].lpData = pa_xmalloc(u->fragment_size);
        pa_assert(u->ihdrs);
        u->ohdrs[i].lpData = pa_xmalloc(u->fragment_size);
        pa_assert(u->ohdrs);
    }

    u->module = m;
    m->userdata = u;

    /* Read mixer settings */
    if (u->sink)
        sink_get_volume_cb(u->sink);

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    if (u->sink) {
        pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
        pa_sink_set_rtpoll(u->sink, u->rtpoll);
    }
    if (u->source) {
        pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
        pa_source_set_rtpoll(u->source, u->rtpoll);
    }

    if (!(u->thread = pa_thread_new("waveout", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    if (u->sink)
        pa_sink_put(u->sink);
    if (u->source)
        pa_source_put(u->source);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;
    unsigned int i;

    pa_assert(m);
    pa_assert(m->core);

    if (!(u = m->userdata))
        return;

    if (u->sink)
        pa_sink_unlink(u->sink);
    if (u->source)
        pa_source_unlink(u->source);

    pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
    if (u->thread)
        pa_thread_free(u->thread);
    pa_thread_mq_done(&u->thread_mq);

    if (u->sink)
        pa_sink_unref(u->sink);
    if (u->source)
        pa_source_unref(u->source);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    if (u->hwi != INVALID_HANDLE_VALUE) {
        waveInReset(u->hwi);
        waveInClose(u->hwi);
    }

    if (u->hwo != INVALID_HANDLE_VALUE) {
        waveOutReset(u->hwo);
        waveOutClose(u->hwo);
    }

    for (i = 0; i < u->fragments; i++) {
        pa_xfree(u->ihdrs[i].lpData);
        pa_xfree(u->ohdrs[i].lpData);
    }

    pa_xfree(u->ihdrs);
    pa_xfree(u->ohdrs);

    DeleteCriticalSection(&u->crit);

    pa_xfree(u);
}
