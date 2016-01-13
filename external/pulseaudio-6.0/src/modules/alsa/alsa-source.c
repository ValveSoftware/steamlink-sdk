/***
  This file is part of PulseAudio.

  Copyright 2004-2008 Lennart Poettering
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

#include <signal.h>
#include <stdio.h>

#include <asoundlib.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/volume.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core.h>
#include <pulsecore/i18n.h>
#include <pulsecore/module.h>
#include <pulsecore/memchunk.h>
#include <pulsecore/sink.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/time-smoother.h>

#include <modules/reserve-wrap.h>

#include "alsa-util.h"
#include "alsa-source.h"

/* #define DEBUG_TIMING */

#define DEFAULT_DEVICE "default"

#define DEFAULT_TSCHED_BUFFER_USEC (2*PA_USEC_PER_SEC)             /* 2s */
#define DEFAULT_TSCHED_WATERMARK_USEC (20*PA_USEC_PER_MSEC)        /* 20ms */

#define TSCHED_WATERMARK_INC_STEP_USEC (10*PA_USEC_PER_MSEC)       /* 10ms  */
#define TSCHED_WATERMARK_DEC_STEP_USEC (5*PA_USEC_PER_MSEC)        /* 5ms */
#define TSCHED_WATERMARK_VERIFY_AFTER_USEC (20*PA_USEC_PER_SEC)    /* 20s */
#define TSCHED_WATERMARK_INC_THRESHOLD_USEC (0*PA_USEC_PER_MSEC)   /* 0ms */
#define TSCHED_WATERMARK_DEC_THRESHOLD_USEC (100*PA_USEC_PER_MSEC) /* 100ms */
#define TSCHED_WATERMARK_STEP_USEC (10*PA_USEC_PER_MSEC)           /* 10ms */

#define TSCHED_MIN_SLEEP_USEC (10*PA_USEC_PER_MSEC)                /* 10ms */
#define TSCHED_MIN_WAKEUP_USEC (4*PA_USEC_PER_MSEC)                /* 4ms */

#define SMOOTHER_WINDOW_USEC  (10*PA_USEC_PER_SEC)                 /* 10s */
#define SMOOTHER_ADJUST_USEC  (1*PA_USEC_PER_SEC)                  /* 1s */

#define SMOOTHER_MIN_INTERVAL (2*PA_USEC_PER_MSEC)                 /* 2ms */
#define SMOOTHER_MAX_INTERVAL (200*PA_USEC_PER_MSEC)               /* 200ms */

#define VOLUME_ACCURACY (PA_VOLUME_NORM/100)

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_source *source;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    snd_pcm_t *pcm_handle;

    char *paths_dir;
    pa_alsa_fdlist *mixer_fdl;
    pa_alsa_mixer_pdata *mixer_pd;
    snd_mixer_t *mixer_handle;
    pa_alsa_path_set *mixer_path_set;
    pa_alsa_path *mixer_path;

    pa_cvolume hardware_volume;

    unsigned int *rates;

    size_t
        frame_size,
        fragment_size,
        hwbuf_size,
        tsched_watermark,
        tsched_watermark_ref,
        hwbuf_unused,
        min_sleep,
        min_wakeup,
        watermark_inc_step,
        watermark_dec_step,
        watermark_inc_threshold,
        watermark_dec_threshold;

    snd_pcm_uframes_t frames_per_block;

    pa_usec_t watermark_dec_not_before;
    pa_usec_t min_latency_ref;
    pa_usec_t tsched_watermark_usec;

    char *device_name;  /* name of the PCM device */
    char *control_device; /* name of the control device */

    bool use_mmap:1, use_tsched:1, deferred_volume:1, fixed_latency_range:1;

    bool first;

    pa_rtpoll_item *alsa_rtpoll_item;

    pa_smoother *smoother;
    uint64_t read_count;
    pa_usec_t smoother_interval;
    pa_usec_t last_smoother_update;

    pa_reserve_wrapper *reserve;
    pa_hook_slot *reserve_slot;
    pa_reserve_monitor_wrapper *monitor;
    pa_hook_slot *monitor_slot;

    /* ucm context */
    pa_alsa_ucm_mapping_context *ucm_context;
};

static void userdata_free(struct userdata *u);

static pa_hook_result_t reserve_cb(pa_reserve_wrapper *r, void *forced, struct userdata *u) {
    pa_assert(r);
    pa_assert(u);

    pa_log_debug("Suspending source %s, because another application requested us to release the device.", u->source->name);

    if (pa_source_suspend(u->source, true, PA_SUSPEND_APPLICATION) < 0)
        return PA_HOOK_CANCEL;

    return PA_HOOK_OK;
}

static void reserve_done(struct userdata *u) {
    pa_assert(u);

    if (u->reserve_slot) {
        pa_hook_slot_free(u->reserve_slot);
        u->reserve_slot = NULL;
    }

    if (u->reserve) {
        pa_reserve_wrapper_unref(u->reserve);
        u->reserve = NULL;
    }
}

static void reserve_update(struct userdata *u) {
    const char *description;
    pa_assert(u);

    if (!u->source || !u->reserve)
        return;

    if ((description = pa_proplist_gets(u->source->proplist, PA_PROP_DEVICE_DESCRIPTION)))
        pa_reserve_wrapper_set_application_device_name(u->reserve, description);
}

static int reserve_init(struct userdata *u, const char *dname) {
    char *rname;

    pa_assert(u);
    pa_assert(dname);

    if (u->reserve)
        return 0;

    if (pa_in_system_mode())
        return 0;

    if (!(rname = pa_alsa_get_reserve_name(dname)))
        return 0;

    /* We are resuming, try to lock the device */
    u->reserve = pa_reserve_wrapper_get(u->core, rname);
    pa_xfree(rname);

    if (!(u->reserve))
        return -1;

    reserve_update(u);

    pa_assert(!u->reserve_slot);
    u->reserve_slot = pa_hook_connect(pa_reserve_wrapper_hook(u->reserve), PA_HOOK_NORMAL, (pa_hook_cb_t) reserve_cb, u);

    return 0;
}

static pa_hook_result_t monitor_cb(pa_reserve_monitor_wrapper *w, void* busy, struct userdata *u) {
    pa_assert(w);
    pa_assert(u);

    if (PA_PTR_TO_UINT(busy) && !u->reserve) {
        pa_log_debug("Suspending source %s, because another application is blocking the access to the device.", u->source->name);
        pa_source_suspend(u->source, true, PA_SUSPEND_APPLICATION);
    } else {
        pa_log_debug("Resuming source %s, because other applications aren't blocking access to the device any more.", u->source->name);
        pa_source_suspend(u->source, false, PA_SUSPEND_APPLICATION);
    }

    return PA_HOOK_OK;
}

static void monitor_done(struct userdata *u) {
    pa_assert(u);

    if (u->monitor_slot) {
        pa_hook_slot_free(u->monitor_slot);
        u->monitor_slot = NULL;
    }

    if (u->monitor) {
        pa_reserve_monitor_wrapper_unref(u->monitor);
        u->monitor = NULL;
    }
}

static int reserve_monitor_init(struct userdata *u, const char *dname) {
    char *rname;

    pa_assert(u);
    pa_assert(dname);

    if (pa_in_system_mode())
        return 0;

    if (!(rname = pa_alsa_get_reserve_name(dname)))
        return 0;

    /* We are resuming, try to lock the device */
    u->monitor = pa_reserve_monitor_wrapper_get(u->core, rname);
    pa_xfree(rname);

    if (!(u->monitor))
        return -1;

    pa_assert(!u->monitor_slot);
    u->monitor_slot = pa_hook_connect(pa_reserve_monitor_wrapper_hook(u->monitor), PA_HOOK_NORMAL, (pa_hook_cb_t) monitor_cb, u);

    return 0;
}

static void fix_min_sleep_wakeup(struct userdata *u) {
    size_t max_use, max_use_2;

    pa_assert(u);
    pa_assert(u->use_tsched);

    max_use = u->hwbuf_size - u->hwbuf_unused;
    max_use_2 = pa_frame_align(max_use/2, &u->source->sample_spec);

    u->min_sleep = pa_usec_to_bytes(TSCHED_MIN_SLEEP_USEC, &u->source->sample_spec);
    u->min_sleep = PA_CLAMP(u->min_sleep, u->frame_size, max_use_2);

    u->min_wakeup = pa_usec_to_bytes(TSCHED_MIN_WAKEUP_USEC, &u->source->sample_spec);
    u->min_wakeup = PA_CLAMP(u->min_wakeup, u->frame_size, max_use_2);
}

static void fix_tsched_watermark(struct userdata *u) {
    size_t max_use;
    pa_assert(u);
    pa_assert(u->use_tsched);

    max_use = u->hwbuf_size - u->hwbuf_unused;

    if (u->tsched_watermark > max_use - u->min_sleep)
        u->tsched_watermark = max_use - u->min_sleep;

    if (u->tsched_watermark < u->min_wakeup)
        u->tsched_watermark = u->min_wakeup;

   u->tsched_watermark_usec = pa_bytes_to_usec(u->tsched_watermark, &u->source->sample_spec);
}

static void increase_watermark(struct userdata *u) {
    size_t old_watermark;
    pa_usec_t old_min_latency, new_min_latency;

    pa_assert(u);
    pa_assert(u->use_tsched);

    /* First, just try to increase the watermark */
    old_watermark = u->tsched_watermark;
    u->tsched_watermark = PA_MIN(u->tsched_watermark * 2, u->tsched_watermark + u->watermark_inc_step);
    fix_tsched_watermark(u);

    if (old_watermark != u->tsched_watermark) {
        pa_log_info("Increasing wakeup watermark to %0.2f ms",
                    (double) u->tsched_watermark_usec / PA_USEC_PER_MSEC);
        return;
    }

    /* Hmm, we cannot increase the watermark any further, hence let's
     raise the latency unless doing so was disabled in
     configuration */
    if (u->fixed_latency_range)
        return;

    old_min_latency = u->source->thread_info.min_latency;
    new_min_latency = PA_MIN(old_min_latency * 2, old_min_latency + TSCHED_WATERMARK_INC_STEP_USEC);
    new_min_latency = PA_MIN(new_min_latency, u->source->thread_info.max_latency);

    if (old_min_latency != new_min_latency) {
        pa_log_info("Increasing minimal latency to %0.2f ms",
                    (double) new_min_latency / PA_USEC_PER_MSEC);

        pa_source_set_latency_range_within_thread(u->source, new_min_latency, u->source->thread_info.max_latency);
    }

    /* When we reach this we're officialy fucked! */
}

static void decrease_watermark(struct userdata *u) {
    size_t old_watermark;
    pa_usec_t now;

    pa_assert(u);
    pa_assert(u->use_tsched);

    now = pa_rtclock_now();

    if (u->watermark_dec_not_before <= 0)
        goto restart;

    if (u->watermark_dec_not_before > now)
        return;

    old_watermark = u->tsched_watermark;

    if (u->tsched_watermark < u->watermark_dec_step)
        u->tsched_watermark = u->tsched_watermark / 2;
    else
        u->tsched_watermark = PA_MAX(u->tsched_watermark / 2, u->tsched_watermark - u->watermark_dec_step);

    fix_tsched_watermark(u);

    if (old_watermark != u->tsched_watermark)
        pa_log_info("Decreasing wakeup watermark to %0.2f ms",
                    (double) u->tsched_watermark_usec / PA_USEC_PER_MSEC);

    /* We don't change the latency range*/

restart:
    u->watermark_dec_not_before = now + TSCHED_WATERMARK_VERIFY_AFTER_USEC;
}

static void hw_sleep_time(struct userdata *u, pa_usec_t *sleep_usec, pa_usec_t*process_usec) {
    pa_usec_t wm, usec;

    pa_assert(sleep_usec);
    pa_assert(process_usec);

    pa_assert(u);
    pa_assert(u->use_tsched);

    usec = pa_source_get_requested_latency_within_thread(u->source);

    if (usec == (pa_usec_t) -1)
        usec = pa_bytes_to_usec(u->hwbuf_size, &u->source->sample_spec);

    wm = u->tsched_watermark_usec;

    if (wm > usec)
        wm = usec/2;

    *sleep_usec = usec - wm;
    *process_usec = wm;

#ifdef DEBUG_TIMING
    pa_log_debug("Buffer time: %lu ms; Sleep time: %lu ms; Process time: %lu ms",
                 (unsigned long) (usec / PA_USEC_PER_MSEC),
                 (unsigned long) (*sleep_usec / PA_USEC_PER_MSEC),
                 (unsigned long) (*process_usec / PA_USEC_PER_MSEC));
#endif
}

static int try_recover(struct userdata *u, const char *call, int err) {
    pa_assert(u);
    pa_assert(call);
    pa_assert(err < 0);

    pa_log_debug("%s: %s", call, pa_alsa_strerror(err));

    pa_assert(err != -EAGAIN);

    if (err == -EPIPE)
        pa_log_debug("%s: Buffer overrun!", call);

    if (err == -ESTRPIPE)
        pa_log_debug("%s: System suspended!", call);

    if ((err = snd_pcm_recover(u->pcm_handle, err, 1)) < 0) {
        pa_log("%s: %s", call, pa_alsa_strerror(err));
        return -1;
    }

    u->first = true;
    return 0;
}

static size_t check_left_to_record(struct userdata *u, size_t n_bytes, bool on_timeout) {
    size_t left_to_record;
    size_t rec_space = u->hwbuf_size - u->hwbuf_unused;
    bool overrun = false;

    /* We use <= instead of < for this check here because an overrun
     * only happens after the last sample was processed, not already when
     * it is removed from the buffer. This is particularly important
     * when block transfer is used. */

    if (n_bytes <= rec_space)
        left_to_record = rec_space - n_bytes;
    else {

        /* We got a dropout. What a mess! */
        left_to_record = 0;
        overrun = true;

#ifdef DEBUG_TIMING
        PA_DEBUG_TRAP;
#endif

        if (pa_log_ratelimit(PA_LOG_INFO))
            pa_log_info("Overrun!");
    }

#ifdef DEBUG_TIMING
    pa_log_debug("%0.2f ms left to record", (double) pa_bytes_to_usec(left_to_record, &u->source->sample_spec) / PA_USEC_PER_MSEC);
#endif

    if (u->use_tsched) {
        bool reset_not_before = true;

        if (overrun || left_to_record < u->watermark_inc_threshold)
            increase_watermark(u);
        else if (left_to_record > u->watermark_dec_threshold) {
            reset_not_before = false;

            /* We decrease the watermark only if have actually
             * been woken up by a timeout. If something else woke
             * us up it's too easy to fulfill the deadlines... */

            if (on_timeout)
                decrease_watermark(u);
        }

        if (reset_not_before)
            u->watermark_dec_not_before = 0;
    }

    return left_to_record;
}

static int mmap_read(struct userdata *u, pa_usec_t *sleep_usec, bool polled, bool on_timeout) {
    bool work_done = false;
    pa_usec_t max_sleep_usec = 0, process_usec = 0;
    size_t left_to_record;
    unsigned j = 0;

    pa_assert(u);
    pa_source_assert_ref(u->source);

    if (u->use_tsched)
        hw_sleep_time(u, &max_sleep_usec, &process_usec);

    for (;;) {
        snd_pcm_sframes_t n;
        size_t n_bytes;
        int r;
        bool after_avail = true;

        if (PA_UNLIKELY((n = pa_alsa_safe_avail(u->pcm_handle, u->hwbuf_size, &u->source->sample_spec)) < 0)) {

            if ((r = try_recover(u, "snd_pcm_avail", (int) n)) == 0)
                continue;

            return r;
        }

        n_bytes = (size_t) n * u->frame_size;

#ifdef DEBUG_TIMING
        pa_log_debug("avail: %lu", (unsigned long) n_bytes);
#endif

        left_to_record = check_left_to_record(u, n_bytes, on_timeout);
        on_timeout = false;

        if (u->use_tsched)
            if (!polled &&
                pa_bytes_to_usec(left_to_record, &u->source->sample_spec) > process_usec+max_sleep_usec/2) {
#ifdef DEBUG_TIMING
                pa_log_debug("Not reading, because too early.");
#endif
                break;
            }

        if (PA_UNLIKELY(n_bytes <= 0)) {

            if (polled)
                PA_ONCE_BEGIN {
                    char *dn = pa_alsa_get_driver_name_by_pcm(u->pcm_handle);
                    pa_log(_("ALSA woke us up to read new data from the device, but there was actually nothing to read!\n"
                             "Most likely this is a bug in the ALSA driver '%s'. Please report this issue to the ALSA developers.\n"
                             "We were woken up with POLLIN set -- however a subsequent snd_pcm_avail() returned 0 or another value < min_avail."),
                           pa_strnull(dn));
                    pa_xfree(dn);
                } PA_ONCE_END;

#ifdef DEBUG_TIMING
            pa_log_debug("Not reading, because not necessary.");
#endif
            break;
        }

        if (++j > 10) {
#ifdef DEBUG_TIMING
            pa_log_debug("Not filling up, because already too many iterations.");
#endif

            break;
        }

        polled = false;

#ifdef DEBUG_TIMING
        pa_log_debug("Reading");
#endif

        for (;;) {
            pa_memchunk chunk;
            void *p;
            int err;
            const snd_pcm_channel_area_t *areas;
            snd_pcm_uframes_t offset, frames;
            snd_pcm_sframes_t sframes;

            frames = (snd_pcm_uframes_t) (n_bytes / u->frame_size);
/*             pa_log_debug("%lu frames to read", (unsigned long) frames); */

            if (PA_UNLIKELY((err = pa_alsa_safe_mmap_begin(u->pcm_handle, &areas, &offset, &frames, u->hwbuf_size, &u->source->sample_spec)) < 0)) {

                if (!after_avail && err == -EAGAIN)
                    break;

                if ((r = try_recover(u, "snd_pcm_mmap_begin", err)) == 0)
                    continue;

                return r;
            }

            /* Make sure that if these memblocks need to be copied they will fit into one slot */
            frames = PA_MIN(frames, u->frames_per_block);

            if (!after_avail && frames == 0)
                break;

            pa_assert(frames > 0);
            after_avail = false;

            /* Check these are multiples of 8 bit */
            pa_assert((areas[0].first & 7) == 0);
            pa_assert((areas[0].step & 7) == 0);

            /* We assume a single interleaved memory buffer */
            pa_assert((areas[0].first >> 3) == 0);
            pa_assert((areas[0].step >> 3) == u->frame_size);

            p = (uint8_t*) areas[0].addr + (offset * u->frame_size);

            chunk.memblock = pa_memblock_new_fixed(u->core->mempool, p, frames * u->frame_size, true);
            chunk.length = pa_memblock_get_length(chunk.memblock);
            chunk.index = 0;

            pa_source_post(u->source, &chunk);
            pa_memblock_unref_fixed(chunk.memblock);

            if (PA_UNLIKELY((sframes = snd_pcm_mmap_commit(u->pcm_handle, offset, frames)) < 0)) {

                if ((r = try_recover(u, "snd_pcm_mmap_commit", (int) sframes)) == 0)
                    continue;

                return r;
            }

            work_done = true;

            u->read_count += frames * u->frame_size;

#ifdef DEBUG_TIMING
            pa_log_debug("Read %lu bytes (of possible %lu bytes)", (unsigned long) (frames * u->frame_size), (unsigned long) n_bytes);
#endif

            if ((size_t) frames * u->frame_size >= n_bytes)
                break;

            n_bytes -= (size_t) frames * u->frame_size;
        }
    }

    if (u->use_tsched) {
        *sleep_usec = pa_bytes_to_usec(left_to_record, &u->source->sample_spec);
        process_usec = u->tsched_watermark_usec;

        if (*sleep_usec > process_usec)
            *sleep_usec -= process_usec;
        else
            *sleep_usec = 0;
    }

    return work_done ? 1 : 0;
}

static int unix_read(struct userdata *u, pa_usec_t *sleep_usec, bool polled, bool on_timeout) {
    int work_done = false;
    pa_usec_t max_sleep_usec = 0, process_usec = 0;
    size_t left_to_record;
    unsigned j = 0;

    pa_assert(u);
    pa_source_assert_ref(u->source);

    if (u->use_tsched)
        hw_sleep_time(u, &max_sleep_usec, &process_usec);

    for (;;) {
        snd_pcm_sframes_t n;
        size_t n_bytes;
        int r;
        bool after_avail = true;

        if (PA_UNLIKELY((n = pa_alsa_safe_avail(u->pcm_handle, u->hwbuf_size, &u->source->sample_spec)) < 0)) {

            if ((r = try_recover(u, "snd_pcm_avail", (int) n)) == 0)
                continue;

            return r;
        }

        n_bytes = (size_t) n * u->frame_size;
        left_to_record = check_left_to_record(u, n_bytes, on_timeout);
        on_timeout = false;

        if (u->use_tsched)
            if (!polled &&
                pa_bytes_to_usec(left_to_record, &u->source->sample_spec) > process_usec+max_sleep_usec/2)
                break;

        if (PA_UNLIKELY(n_bytes <= 0)) {

            if (polled)
                PA_ONCE_BEGIN {
                    char *dn = pa_alsa_get_driver_name_by_pcm(u->pcm_handle);
                    pa_log(_("ALSA woke us up to read new data from the device, but there was actually nothing to read!\n"
                             "Most likely this is a bug in the ALSA driver '%s'. Please report this issue to the ALSA developers.\n"
                             "We were woken up with POLLIN set -- however a subsequent snd_pcm_avail() returned 0 or another value < min_avail."),
                           pa_strnull(dn));
                    pa_xfree(dn);
                } PA_ONCE_END;

            break;
        }

        if (++j > 10) {
#ifdef DEBUG_TIMING
            pa_log_debug("Not filling up, because already too many iterations.");
#endif

            break;
        }

        polled = false;

        for (;;) {
            void *p;
            snd_pcm_sframes_t frames;
            pa_memchunk chunk;

            chunk.memblock = pa_memblock_new(u->core->mempool, (size_t) -1);

            frames = (snd_pcm_sframes_t) (pa_memblock_get_length(chunk.memblock) / u->frame_size);

            if (frames > (snd_pcm_sframes_t) (n_bytes/u->frame_size))
                frames = (snd_pcm_sframes_t) (n_bytes/u->frame_size);

/*             pa_log_debug("%lu frames to read", (unsigned long) n); */

            p = pa_memblock_acquire(chunk.memblock);
            frames = snd_pcm_readi(u->pcm_handle, (uint8_t*) p, (snd_pcm_uframes_t) frames);
            pa_memblock_release(chunk.memblock);

            if (PA_UNLIKELY(frames < 0)) {
                pa_memblock_unref(chunk.memblock);

                if (!after_avail && (int) frames == -EAGAIN)
                    break;

                if ((r = try_recover(u, "snd_pcm_readi", (int) frames)) == 0)
                    continue;

                return r;
            }

            if (!after_avail && frames == 0) {
                pa_memblock_unref(chunk.memblock);
                break;
            }

            pa_assert(frames > 0);
            after_avail = false;

            chunk.index = 0;
            chunk.length = (size_t) frames * u->frame_size;

            pa_source_post(u->source, &chunk);
            pa_memblock_unref(chunk.memblock);

            work_done = true;

            u->read_count += frames * u->frame_size;

/*             pa_log_debug("read %lu frames", (unsigned long) frames); */

            if ((size_t) frames * u->frame_size >= n_bytes)
                break;

            n_bytes -= (size_t) frames * u->frame_size;
        }
    }

    if (u->use_tsched) {
        *sleep_usec = pa_bytes_to_usec(left_to_record, &u->source->sample_spec);
        process_usec = u->tsched_watermark_usec;

        if (*sleep_usec > process_usec)
            *sleep_usec -= process_usec;
        else
            *sleep_usec = 0;
    }

    return work_done ? 1 : 0;
}

static void update_smoother(struct userdata *u) {
    snd_pcm_sframes_t delay = 0;
    uint64_t position;
    int err;
    pa_usec_t now1 = 0, now2;
    snd_pcm_status_t *status;
    snd_htimestamp_t htstamp = { 0, 0 };

    snd_pcm_status_alloca(&status);

    pa_assert(u);
    pa_assert(u->pcm_handle);

    /* Let's update the time smoother */

    if (PA_UNLIKELY((err = pa_alsa_safe_delay(u->pcm_handle, status, &delay, u->hwbuf_size, &u->source->sample_spec, true)) < 0)) {
        pa_log_warn("Failed to get delay: %s", pa_alsa_strerror(err));
        return;
    }

    snd_pcm_status_get_htstamp(status, &htstamp);
    now1 = pa_timespec_load(&htstamp);

    /* Hmm, if the timestamp is 0, then it wasn't set and we take the current time */
    if (now1 <= 0)
        now1 = pa_rtclock_now();

    /* check if the time since the last update is bigger than the interval */
    if (u->last_smoother_update > 0)
        if (u->last_smoother_update + u->smoother_interval > now1)
            return;

    position = u->read_count + ((uint64_t) delay * (uint64_t) u->frame_size);
    now2 = pa_bytes_to_usec(position, &u->source->sample_spec);

    pa_smoother_put(u->smoother, now1, now2);

    u->last_smoother_update = now1;
    /* exponentially increase the update interval up to the MAX limit */
    u->smoother_interval = PA_MIN (u->smoother_interval * 2, SMOOTHER_MAX_INTERVAL);
}

static pa_usec_t source_get_latency(struct userdata *u) {
    int64_t delay;
    pa_usec_t now1, now2;

    pa_assert(u);

    now1 = pa_rtclock_now();
    now2 = pa_smoother_get(u->smoother, now1);

    delay = (int64_t) now2 - (int64_t) pa_bytes_to_usec(u->read_count, &u->source->sample_spec);

    return delay >= 0 ? (pa_usec_t) delay : 0;
}

static int build_pollfd(struct userdata *u) {
    pa_assert(u);
    pa_assert(u->pcm_handle);

    if (u->alsa_rtpoll_item)
        pa_rtpoll_item_free(u->alsa_rtpoll_item);

    if (!(u->alsa_rtpoll_item = pa_alsa_build_pollfd(u->pcm_handle, u->rtpoll)))
        return -1;

    return 0;
}

/* Called from IO context */
static int suspend(struct userdata *u) {
    pa_assert(u);
    pa_assert(u->pcm_handle);

    pa_smoother_pause(u->smoother, pa_rtclock_now());

    /* Let's suspend */
    snd_pcm_close(u->pcm_handle);
    u->pcm_handle = NULL;

    if (u->alsa_rtpoll_item) {
        pa_rtpoll_item_free(u->alsa_rtpoll_item);
        u->alsa_rtpoll_item = NULL;
    }

    pa_log_info("Device suspended...");

    return 0;
}

/* Called from IO context */
static int update_sw_params(struct userdata *u) {
    snd_pcm_uframes_t avail_min;
    int err;

    pa_assert(u);

    /* Use the full buffer if no one asked us for anything specific */
    u->hwbuf_unused = 0;

    if (u->use_tsched) {
        pa_usec_t latency;

        if ((latency = pa_source_get_requested_latency_within_thread(u->source)) != (pa_usec_t) -1) {
            size_t b;

            pa_log_debug("latency set to %0.2fms", (double) latency / PA_USEC_PER_MSEC);

            b = pa_usec_to_bytes(latency, &u->source->sample_spec);

            /* We need at least one sample in our buffer */

            if (PA_UNLIKELY(b < u->frame_size))
                b = u->frame_size;

            u->hwbuf_unused = PA_LIKELY(b < u->hwbuf_size) ? (u->hwbuf_size - b) : 0;
        }

        fix_min_sleep_wakeup(u);
        fix_tsched_watermark(u);
    }

    pa_log_debug("hwbuf_unused=%lu", (unsigned long) u->hwbuf_unused);

    avail_min = 1;

    if (u->use_tsched) {
        pa_usec_t sleep_usec, process_usec;

        hw_sleep_time(u, &sleep_usec, &process_usec);
        avail_min += pa_usec_to_bytes(sleep_usec, &u->source->sample_spec) / u->frame_size;
    }

    pa_log_debug("setting avail_min=%lu", (unsigned long) avail_min);

    if ((err = pa_alsa_set_sw_params(u->pcm_handle, avail_min, !u->use_tsched)) < 0) {
        pa_log("Failed to set software parameters: %s", pa_alsa_strerror(err));
        return err;
    }

    return 0;
}

/* Called from IO Context on unsuspend or from main thread when creating source */
static void reset_watermark(struct userdata *u, size_t tsched_watermark, pa_sample_spec *ss,
                            bool in_thread) {
    u->tsched_watermark = pa_usec_to_bytes_round_up(pa_bytes_to_usec_round_up(tsched_watermark, ss),
                                                    &u->source->sample_spec);

    u->watermark_inc_step = pa_usec_to_bytes(TSCHED_WATERMARK_INC_STEP_USEC, &u->source->sample_spec);
    u->watermark_dec_step = pa_usec_to_bytes(TSCHED_WATERMARK_DEC_STEP_USEC, &u->source->sample_spec);

    u->watermark_inc_threshold = pa_usec_to_bytes_round_up(TSCHED_WATERMARK_INC_THRESHOLD_USEC, &u->source->sample_spec);
    u->watermark_dec_threshold = pa_usec_to_bytes_round_up(TSCHED_WATERMARK_DEC_THRESHOLD_USEC, &u->source->sample_spec);

    fix_min_sleep_wakeup(u);
    fix_tsched_watermark(u);

    if (in_thread)
        pa_source_set_latency_range_within_thread(u->source,
                                                  u->min_latency_ref,
                                                  pa_bytes_to_usec(u->hwbuf_size, ss));
    else {
        pa_source_set_latency_range(u->source,
                                    0,
                                    pa_bytes_to_usec(u->hwbuf_size, ss));

        /* work-around assert in pa_source_set_latency_within_thead,
           keep track of min_latency and reuse it when
           this routine is called from IO context */
        u->min_latency_ref = u->source->thread_info.min_latency;
    }

    pa_log_info("Time scheduling watermark is %0.2fms",
                (double) u->tsched_watermark_usec / PA_USEC_PER_MSEC);
}

/* Called from IO context */
static int unsuspend(struct userdata *u) {
    pa_sample_spec ss;
    int err;
    bool b, d;
    snd_pcm_uframes_t period_size, buffer_size;

    pa_assert(u);
    pa_assert(!u->pcm_handle);

    pa_log_info("Trying resume...");

    if ((err = snd_pcm_open(&u->pcm_handle, u->device_name, SND_PCM_STREAM_CAPTURE,
                            SND_PCM_NONBLOCK|
                            SND_PCM_NO_AUTO_RESAMPLE|
                            SND_PCM_NO_AUTO_CHANNELS|
                            SND_PCM_NO_AUTO_FORMAT)) < 0) {
        pa_log("Error opening PCM device %s: %s", u->device_name, pa_alsa_strerror(err));
        goto fail;
    }

    ss = u->source->sample_spec;
    period_size = u->fragment_size / u->frame_size;
    buffer_size = u->hwbuf_size / u->frame_size;
    b = u->use_mmap;
    d = u->use_tsched;

    if ((err = pa_alsa_set_hw_params(u->pcm_handle, &ss, &period_size, &buffer_size, 0, &b, &d, true)) < 0) {
        pa_log("Failed to set hardware parameters: %s", pa_alsa_strerror(err));
        goto fail;
    }

    if (b != u->use_mmap || d != u->use_tsched) {
        pa_log_warn("Resume failed, couldn't get original access mode.");
        goto fail;
    }

    if (!pa_sample_spec_equal(&ss, &u->source->sample_spec)) {
        pa_log_warn("Resume failed, couldn't restore original sample settings.");
        goto fail;
    }

    if (period_size*u->frame_size != u->fragment_size ||
        buffer_size*u->frame_size != u->hwbuf_size) {
        pa_log_warn("Resume failed, couldn't restore original fragment settings. (Old: %lu/%lu, New %lu/%lu)",
                    (unsigned long) u->hwbuf_size, (unsigned long) u->fragment_size,
                    (unsigned long) (buffer_size*u->frame_size), (unsigned long) (period_size*u->frame_size));
        goto fail;
    }

    if (update_sw_params(u) < 0)
        goto fail;

    if (build_pollfd(u) < 0)
        goto fail;

    /* FIXME: We need to reload the volume somehow */

    u->read_count = 0;
    pa_smoother_reset(u->smoother, pa_rtclock_now(), true);
    u->smoother_interval = SMOOTHER_MIN_INTERVAL;
    u->last_smoother_update = 0;

    u->first = true;

    /* reset the watermark to the value defined when source was created */
    if (u->use_tsched)
        reset_watermark(u, u->tsched_watermark_ref, &u->source->sample_spec, true);

    pa_log_info("Resumed successfully...");

    return 0;

fail:
    if (u->pcm_handle) {
        snd_pcm_close(u->pcm_handle);
        u->pcm_handle = NULL;
    }

    return -PA_ERR_IO;
}

/* Called from IO context */
static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            pa_usec_t r = 0;

            if (u->pcm_handle)
                r = source_get_latency(u);

            *((pa_usec_t*) data) = r;

            return 0;
        }

        case PA_SOURCE_MESSAGE_SET_STATE:

            switch ((pa_source_state_t) PA_PTR_TO_UINT(data)) {

                case PA_SOURCE_SUSPENDED: {
                    int r;

                    pa_assert(PA_SOURCE_IS_OPENED(u->source->thread_info.state));

                    if ((r = suspend(u)) < 0)
                        return r;

                    break;
                }

                case PA_SOURCE_IDLE:
                case PA_SOURCE_RUNNING: {
                    int r;

                    if (u->source->thread_info.state == PA_SOURCE_INIT) {
                        if (build_pollfd(u) < 0)
                            return -PA_ERR_IO;
                    }

                    if (u->source->thread_info.state == PA_SOURCE_SUSPENDED) {
                        if ((r = unsuspend(u)) < 0)
                            return r;
                    }

                    break;
                }

                case PA_SOURCE_UNLINKED:
                case PA_SOURCE_INIT:
                case PA_SOURCE_INVALID_STATE:
                    ;
            }

            break;
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int source_set_state_cb(pa_source *s, pa_source_state_t new_state) {
    pa_source_state_t old_state;
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    old_state = pa_source_get_state(u->source);

    if (PA_SOURCE_IS_OPENED(old_state) && new_state == PA_SOURCE_SUSPENDED)
        reserve_done(u);
    else if (old_state == PA_SOURCE_SUSPENDED && PA_SOURCE_IS_OPENED(new_state))
        if (reserve_init(u, u->device_name) < 0)
            return -PA_ERR_BUSY;

    return 0;
}

static int ctl_mixer_callback(snd_mixer_elem_t *elem, unsigned int mask) {
    struct userdata *u = snd_mixer_elem_get_callback_private(elem);

    pa_assert(u);
    pa_assert(u->mixer_handle);

    if (mask == SND_CTL_EVENT_MASK_REMOVE)
        return 0;

    if (!PA_SOURCE_IS_LINKED(u->source->state))
        return 0;

    if (u->source->suspend_cause & PA_SUSPEND_SESSION) {
        pa_source_set_mixer_dirty(u->source, true);
        return 0;
    }

    if (mask & SND_CTL_EVENT_MASK_VALUE) {
        pa_source_get_volume(u->source, true);
        pa_source_get_mute(u->source, true);
    }

    return 0;
}

static int io_mixer_callback(snd_mixer_elem_t *elem, unsigned int mask) {
    struct userdata *u = snd_mixer_elem_get_callback_private(elem);

    pa_assert(u);
    pa_assert(u->mixer_handle);

    if (mask == SND_CTL_EVENT_MASK_REMOVE)
        return 0;

    if (u->source->suspend_cause & PA_SUSPEND_SESSION) {
        pa_source_set_mixer_dirty(u->source, true);
        return 0;
    }

    if (mask & SND_CTL_EVENT_MASK_VALUE)
        pa_source_update_volume_and_mute(u->source);

    return 0;
}

static void source_get_volume_cb(pa_source *s) {
    struct userdata *u = s->userdata;
    pa_cvolume r;
    char volume_buf[PA_CVOLUME_SNPRINT_VERBOSE_MAX];

    pa_assert(u);
    pa_assert(u->mixer_path);
    pa_assert(u->mixer_handle);

    if (pa_alsa_path_get_volume(u->mixer_path, u->mixer_handle, &s->channel_map, &r) < 0)
        return;

    /* Shift down by the base volume, so that 0dB becomes maximum volume */
    pa_sw_cvolume_multiply_scalar(&r, &r, s->base_volume);

    pa_log_debug("Read hardware volume: %s",
                 pa_cvolume_snprint_verbose(volume_buf, sizeof(volume_buf), &r, &s->channel_map, u->mixer_path->has_dB));

    if (pa_cvolume_equal(&u->hardware_volume, &r))
        return;

    s->real_volume = u->hardware_volume = r;

    /* Hmm, so the hardware volume changed, let's reset our software volume */
    if (u->mixer_path->has_dB)
        pa_source_set_soft_volume(s, NULL);
}

static void source_set_volume_cb(pa_source *s) {
    struct userdata *u = s->userdata;
    pa_cvolume r;
    char volume_buf[PA_CVOLUME_SNPRINT_VERBOSE_MAX];
    bool deferred_volume = !!(s->flags & PA_SOURCE_DEFERRED_VOLUME);

    pa_assert(u);
    pa_assert(u->mixer_path);
    pa_assert(u->mixer_handle);

    /* Shift up by the base volume */
    pa_sw_cvolume_divide_scalar(&r, &s->real_volume, s->base_volume);

    if (pa_alsa_path_set_volume(u->mixer_path, u->mixer_handle, &s->channel_map, &r, deferred_volume, !deferred_volume) < 0)
        return;

    /* Shift down by the base volume, so that 0dB becomes maximum volume */
    pa_sw_cvolume_multiply_scalar(&r, &r, s->base_volume);

    u->hardware_volume = r;

    if (u->mixer_path->has_dB) {
        pa_cvolume new_soft_volume;
        bool accurate_enough;

        /* Match exactly what the user requested by software */
        pa_sw_cvolume_divide(&new_soft_volume, &s->real_volume, &u->hardware_volume);

        /* If the adjustment to do in software is only minimal we
         * can skip it. That saves us CPU at the expense of a bit of
         * accuracy */
        accurate_enough =
            (pa_cvolume_min(&new_soft_volume) >= (PA_VOLUME_NORM - VOLUME_ACCURACY)) &&
            (pa_cvolume_max(&new_soft_volume) <= (PA_VOLUME_NORM + VOLUME_ACCURACY));

        pa_log_debug("Requested volume: %s",
                     pa_cvolume_snprint_verbose(volume_buf, sizeof(volume_buf), &s->real_volume, &s->channel_map, true));
        pa_log_debug("Got hardware volume: %s",
                     pa_cvolume_snprint_verbose(volume_buf, sizeof(volume_buf), &u->hardware_volume, &s->channel_map, true));
        pa_log_debug("Calculated software volume: %s (accurate-enough=%s)",
                     pa_cvolume_snprint_verbose(volume_buf, sizeof(volume_buf), &new_soft_volume, &s->channel_map, true),
                     pa_yes_no(accurate_enough));

        if (!accurate_enough)
            s->soft_volume = new_soft_volume;

    } else {
        pa_log_debug("Wrote hardware volume: %s",
                     pa_cvolume_snprint_verbose(volume_buf, sizeof(volume_buf), &r, &s->channel_map, false));

        /* We can't match exactly what the user requested, hence let's
         * at least tell the user about it */

        s->real_volume = r;
    }
}

static void source_write_volume_cb(pa_source *s) {
    struct userdata *u = s->userdata;
    pa_cvolume hw_vol = s->thread_info.current_hw_volume;

    pa_assert(u);
    pa_assert(u->mixer_path);
    pa_assert(u->mixer_handle);
    pa_assert(s->flags & PA_SOURCE_DEFERRED_VOLUME);

    /* Shift up by the base volume */
    pa_sw_cvolume_divide_scalar(&hw_vol, &hw_vol, s->base_volume);

    if (pa_alsa_path_set_volume(u->mixer_path, u->mixer_handle, &s->channel_map, &hw_vol, true, true) < 0)
        pa_log_error("Writing HW volume failed");
    else {
        pa_cvolume tmp_vol;
        bool accurate_enough;

        /* Shift down by the base volume, so that 0dB becomes maximum volume */
        pa_sw_cvolume_multiply_scalar(&hw_vol, &hw_vol, s->base_volume);

        pa_sw_cvolume_divide(&tmp_vol, &hw_vol, &s->thread_info.current_hw_volume);
        accurate_enough =
            (pa_cvolume_min(&tmp_vol) >= (PA_VOLUME_NORM - VOLUME_ACCURACY)) &&
            (pa_cvolume_max(&tmp_vol) <= (PA_VOLUME_NORM + VOLUME_ACCURACY));

        if (!accurate_enough) {
            char volume_buf[2][PA_CVOLUME_SNPRINT_VERBOSE_MAX];

            pa_log_debug("Written HW volume did not match with the request: %s (request) != %s",
                         pa_cvolume_snprint_verbose(volume_buf[0],
                                                    sizeof(volume_buf[0]),
                                                    &s->thread_info.current_hw_volume,
                                                    &s->channel_map,
                                                    true),
                         pa_cvolume_snprint_verbose(volume_buf[1], sizeof(volume_buf[1]), &hw_vol, &s->channel_map, true));
        }
    }
}

static int source_get_mute_cb(pa_source *s, bool *mute) {
    struct userdata *u = s->userdata;

    pa_assert(u);
    pa_assert(u->mixer_path);
    pa_assert(u->mixer_handle);

    if (pa_alsa_path_get_mute(u->mixer_path, u->mixer_handle, mute) < 0)
        return -1;

    return 0;
}

static void source_set_mute_cb(pa_source *s) {
    struct userdata *u = s->userdata;

    pa_assert(u);
    pa_assert(u->mixer_path);
    pa_assert(u->mixer_handle);

    pa_alsa_path_set_mute(u->mixer_path, u->mixer_handle, s->muted);
}

static void mixer_volume_init(struct userdata *u) {
    pa_assert(u);

    if (!u->mixer_path->has_volume) {
        pa_source_set_write_volume_callback(u->source, NULL);
        pa_source_set_get_volume_callback(u->source, NULL);
        pa_source_set_set_volume_callback(u->source, NULL);

        pa_log_info("Driver does not support hardware volume control, falling back to software volume control.");
    } else {
        pa_source_set_get_volume_callback(u->source, source_get_volume_cb);
        pa_source_set_set_volume_callback(u->source, source_set_volume_cb);

        if (u->mixer_path->has_dB && u->deferred_volume) {
            pa_source_set_write_volume_callback(u->source, source_write_volume_cb);
            pa_log_info("Successfully enabled deferred volume.");
        } else
            pa_source_set_write_volume_callback(u->source, NULL);

        if (u->mixer_path->has_dB) {
            pa_source_enable_decibel_volume(u->source, true);
            pa_log_info("Hardware volume ranges from %0.2f dB to %0.2f dB.", u->mixer_path->min_dB, u->mixer_path->max_dB);

            u->source->base_volume = pa_sw_volume_from_dB(-u->mixer_path->max_dB);
            u->source->n_volume_steps = PA_VOLUME_NORM+1;

            pa_log_info("Fixing base volume to %0.2f dB", pa_sw_volume_to_dB(u->source->base_volume));
        } else {
            pa_source_enable_decibel_volume(u->source, false);
            pa_log_info("Hardware volume ranges from %li to %li.", u->mixer_path->min_volume, u->mixer_path->max_volume);

            u->source->base_volume = PA_VOLUME_NORM;
            u->source->n_volume_steps = u->mixer_path->max_volume - u->mixer_path->min_volume + 1;
        }

        pa_log_info("Using hardware volume control. Hardware dB scale %s.", u->mixer_path->has_dB ? "supported" : "not supported");
    }

    if (!u->mixer_path->has_mute) {
        pa_source_set_get_mute_callback(u->source, NULL);
        pa_source_set_set_mute_callback(u->source, NULL);
        pa_log_info("Driver does not support hardware mute control, falling back to software mute control.");
    } else {
        pa_source_set_get_mute_callback(u->source, source_get_mute_cb);
        pa_source_set_set_mute_callback(u->source, source_set_mute_cb);
        pa_log_info("Using hardware mute control.");
    }
}

static int source_set_port_ucm_cb(pa_source *s, pa_device_port *p) {
    struct userdata *u = s->userdata;

    pa_assert(u);
    pa_assert(p);
    pa_assert(u->ucm_context);

    return pa_alsa_ucm_set_port(u->ucm_context, p, false);
}

static int source_set_port_cb(pa_source *s, pa_device_port *p) {
    struct userdata *u = s->userdata;
    pa_alsa_port_data *data;

    pa_assert(u);
    pa_assert(p);
    pa_assert(u->mixer_handle);

    data = PA_DEVICE_PORT_DATA(p);

    pa_assert_se(u->mixer_path = data->path);
    pa_alsa_path_select(u->mixer_path, data->setting, u->mixer_handle, s->muted);

    mixer_volume_init(u);

    if (s->set_mute)
        s->set_mute(s);
    if (s->flags & PA_SOURCE_DEFERRED_VOLUME) {
        if (s->write_volume)
            s->write_volume(s);
    } else {
        if (s->set_volume)
            s->set_volume(s);
    }

    return 0;
}

static void source_update_requested_latency_cb(pa_source *s) {
    struct userdata *u = s->userdata;
    pa_assert(u);
    pa_assert(u->use_tsched); /* only when timer scheduling is used
                               * we can dynamically adjust the
                               * latency */

    if (!u->pcm_handle)
        return;

    update_sw_params(u);
}

static int source_update_rate_cb(pa_source *s, uint32_t rate) {
    struct userdata *u = s->userdata;
    int i;
    bool supported = false;

    pa_assert(u);

    for (i = 0; u->rates[i]; i++) {
        if (u->rates[i] == rate) {
            supported = true;
            break;
        }
    }

    if (!supported) {
        pa_log_info("Source does not support sample rate of %d Hz", rate);
        return -1;
    }

    if (!PA_SOURCE_IS_OPENED(s->state)) {
        pa_log_info("Updating rate for device %s, new rate is %d", u->device_name, rate);
        u->source->sample_spec.rate = rate;
        return 0;
    }

    return -1;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    unsigned short revents = 0;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    for (;;) {
        int ret;
        pa_usec_t rtpoll_sleep = 0, real_sleep;

#ifdef DEBUG_TIMING
        pa_log_debug("Loop");
#endif

        /* Read some data and pass it to the sources */
        if (PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
            int work_done;
            pa_usec_t sleep_usec = 0;
            bool on_timeout = pa_rtpoll_timer_elapsed(u->rtpoll);

            if (u->first) {
                pa_log_info("Starting capture.");
                snd_pcm_start(u->pcm_handle);

                pa_smoother_resume(u->smoother, pa_rtclock_now(), true);

                u->first = false;
            }

            if (u->use_mmap)
                work_done = mmap_read(u, &sleep_usec, revents & POLLIN, on_timeout);
            else
                work_done = unix_read(u, &sleep_usec, revents & POLLIN, on_timeout);

            if (work_done < 0)
                goto fail;

/*             pa_log_debug("work_done = %i", work_done); */

            if (work_done)
                update_smoother(u);

            if (u->use_tsched) {
                pa_usec_t cusec;

                /* OK, the capture buffer is now empty, let's
                 * calculate when to wake up next */

/*                 pa_log_debug("Waking up in %0.2fms (sound card clock).", (double) sleep_usec / PA_USEC_PER_MSEC); */

                /* Convert from the sound card time domain to the
                 * system time domain */
                cusec = pa_smoother_translate(u->smoother, pa_rtclock_now(), sleep_usec);

/*                 pa_log_debug("Waking up in %0.2fms (system clock).", (double) cusec / PA_USEC_PER_MSEC); */

                /* We don't trust the conversion, so we wake up whatever comes first */
                rtpoll_sleep = PA_MIN(sleep_usec, cusec);
            }
        }

        if (u->source->flags & PA_SOURCE_DEFERRED_VOLUME) {
            pa_usec_t volume_sleep;
            pa_source_volume_change_apply(u->source, &volume_sleep);
            if (volume_sleep > 0) {
                if (rtpoll_sleep > 0)
                    rtpoll_sleep = PA_MIN(volume_sleep, rtpoll_sleep);
                else
                    rtpoll_sleep = volume_sleep;
            }
        }

        if (rtpoll_sleep > 0) {
            pa_rtpoll_set_timer_relative(u->rtpoll, rtpoll_sleep);
            real_sleep = pa_rtclock_now();
        }
        else
            pa_rtpoll_set_timer_disabled(u->rtpoll);

        /* Hmm, nothing to do. Let's sleep */
        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (rtpoll_sleep > 0) {
            real_sleep = pa_rtclock_now() - real_sleep;
#ifdef DEBUG_TIMING
            pa_log_debug("Expected sleep: %0.2fms, real sleep: %0.2fms (diff %0.2f ms)",
                (double) rtpoll_sleep / PA_USEC_PER_MSEC, (double) real_sleep / PA_USEC_PER_MSEC,
                (double) ((int64_t) real_sleep - (int64_t) rtpoll_sleep) / PA_USEC_PER_MSEC);
#endif
            if (u->use_tsched && real_sleep > rtpoll_sleep + u->tsched_watermark_usec)
                pa_log_info("Scheduling delay of %0.2f ms > %0.2f ms, you might want to investigate this to improve latency...",
                    (double) (real_sleep - rtpoll_sleep) / PA_USEC_PER_MSEC,
                    (double) (u->tsched_watermark_usec) / PA_USEC_PER_MSEC);
        }

        if (u->source->flags & PA_SOURCE_DEFERRED_VOLUME)
            pa_source_volume_change_apply(u->source, NULL);

        if (ret == 0)
            goto finish;

        /* Tell ALSA about this and process its response */
        if (PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
            struct pollfd *pollfd;
            int err;
            unsigned n;

            pollfd = pa_rtpoll_item_get_pollfd(u->alsa_rtpoll_item, &n);

            if ((err = snd_pcm_poll_descriptors_revents(u->pcm_handle, pollfd, n, &revents)) < 0) {
                pa_log("snd_pcm_poll_descriptors_revents() failed: %s", pa_alsa_strerror(err));
                goto fail;
            }

            if (revents & ~POLLIN) {
                if (pa_alsa_recover_from_poll(u->pcm_handle, revents) < 0)
                    goto fail;

                u->first = true;
                revents = 0;
            } else if (revents && u->use_tsched && pa_log_ratelimit(PA_LOG_DEBUG))
                pa_log_debug("Wakeup from ALSA!");

        } else
            revents = 0;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

static void set_source_name(pa_source_new_data *data, pa_modargs *ma, const char *device_id, const char *device_name, pa_alsa_mapping *mapping) {
    const char *n;
    char *t;

    pa_assert(data);
    pa_assert(ma);
    pa_assert(device_name);

    if ((n = pa_modargs_get_value(ma, "source_name", NULL))) {
        pa_source_new_data_set_name(data, n);
        data->namereg_fail = true;
        return;
    }

    if ((n = pa_modargs_get_value(ma, "name", NULL)))
        data->namereg_fail = true;
    else {
        n = device_id ? device_id : device_name;
        data->namereg_fail = false;
    }

    if (mapping)
        t = pa_sprintf_malloc("alsa_input.%s.%s", n, mapping->name);
    else
        t = pa_sprintf_malloc("alsa_input.%s", n);

    pa_source_new_data_set_name(data, t);
    pa_xfree(t);
}

static void find_mixer(struct userdata *u, pa_alsa_mapping *mapping, const char *element, bool ignore_dB) {
    if (!mapping && !element)
        return;

    if (!(u->mixer_handle = pa_alsa_open_mixer_for_pcm(u->pcm_handle, &u->control_device))) {
        pa_log_info("Failed to find a working mixer device.");
        return;
    }

    if (element) {

        if (!(u->mixer_path = pa_alsa_path_synthesize(element, PA_ALSA_DIRECTION_INPUT)))
            goto fail;

        if (pa_alsa_path_probe(u->mixer_path, u->mixer_handle, ignore_dB) < 0)
            goto fail;

        pa_log_debug("Probed mixer path %s:", u->mixer_path->name);
        pa_alsa_path_dump(u->mixer_path);
    } else if (!(u->mixer_path_set = mapping->input_path_set))
        goto fail;

    return;

fail:

    if (u->mixer_path) {
        pa_alsa_path_free(u->mixer_path);
        u->mixer_path = NULL;
    }

    if (u->mixer_handle) {
        snd_mixer_close(u->mixer_handle);
        u->mixer_handle = NULL;
    }
}

static int setup_mixer(struct userdata *u, bool ignore_dB) {
    bool need_mixer_callback = false;

    pa_assert(u);

    if (!u->mixer_handle)
        return 0;

    if (u->source->active_port) {
        pa_alsa_port_data *data;

        /* We have a list of supported paths, so let's activate the
         * one that has been chosen as active */

        data = PA_DEVICE_PORT_DATA(u->source->active_port);
        u->mixer_path = data->path;

        pa_alsa_path_select(data->path, data->setting, u->mixer_handle, u->source->muted);

    } else {

        if (!u->mixer_path && u->mixer_path_set)
            u->mixer_path = pa_hashmap_first(u->mixer_path_set->paths);

        if (u->mixer_path) {
            /* Hmm, we have only a single path, then let's activate it */

            pa_alsa_path_select(u->mixer_path, u->mixer_path->settings, u->mixer_handle, u->source->muted);
        } else
            return 0;
    }

    mixer_volume_init(u);

    /* Will we need to register callbacks? */
    if (u->mixer_path_set && u->mixer_path_set->paths) {
        pa_alsa_path *p;
        void *state;

        PA_HASHMAP_FOREACH(p, u->mixer_path_set->paths, state) {
            if (p->has_volume || p->has_mute)
                need_mixer_callback = true;
        }
    }
    else if (u->mixer_path)
        need_mixer_callback = u->mixer_path->has_volume || u->mixer_path->has_mute;

    if (need_mixer_callback) {
        int (*mixer_callback)(snd_mixer_elem_t *, unsigned int);
        if (u->source->flags & PA_SOURCE_DEFERRED_VOLUME) {
            u->mixer_pd = pa_alsa_mixer_pdata_new();
            mixer_callback = io_mixer_callback;

            if (pa_alsa_set_mixer_rtpoll(u->mixer_pd, u->mixer_handle, u->rtpoll) < 0) {
                pa_log("Failed to initialize file descriptor monitoring");
                return -1;
            }
        } else {
            u->mixer_fdl = pa_alsa_fdlist_new();
            mixer_callback = ctl_mixer_callback;

            if (pa_alsa_fdlist_set_handle(u->mixer_fdl, u->mixer_handle, NULL, u->core->mainloop) < 0) {
                pa_log("Failed to initialize file descriptor monitoring");
                return -1;
            }
        }

        if (u->mixer_path_set)
            pa_alsa_path_set_set_callback(u->mixer_path_set, u->mixer_handle, mixer_callback, u);
        else
            pa_alsa_path_set_callback(u->mixer_path, u->mixer_handle, mixer_callback, u);
    }

    return 0;
}

pa_source *pa_alsa_source_new(pa_module *m, pa_modargs *ma, const char*driver, pa_card *card, pa_alsa_mapping *mapping) {

    struct userdata *u = NULL;
    const char *dev_id = NULL, *key, *mod_name;
    pa_sample_spec ss;
    char *thread_name = NULL;
    uint32_t alternate_sample_rate;
    pa_channel_map map;
    uint32_t nfrags, frag_size, buffer_size, tsched_size, tsched_watermark;
    snd_pcm_uframes_t period_frames, buffer_frames, tsched_frames;
    size_t frame_size;
    bool use_mmap = true, b, use_tsched = true, d, ignore_dB = false, namereg_fail = false, deferred_volume = false, fixed_latency_range = false;
    pa_source_new_data data;
    pa_alsa_profile_set *profile_set = NULL;
    void *state = NULL;

    pa_assert(m);
    pa_assert(ma);

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;

    /* Pick sample spec overrides from the mapping, if any */
    if (mapping) {
        if (mapping->sample_spec.format != PA_SAMPLE_INVALID)
            ss.format = mapping->sample_spec.format;
        if (mapping->sample_spec.rate != 0)
            ss.rate = mapping->sample_spec.rate;
        if (mapping->sample_spec.channels != 0) {
            ss.channels = mapping->sample_spec.channels;
            if (pa_channel_map_valid(&mapping->channel_map))
                pa_assert(pa_channel_map_compatible(&mapping->channel_map, &ss));
        }
    }

    /* Override with modargs if provided */
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_ALSA) < 0) {
        pa_log("Failed to parse sample specification and channel map");
        goto fail;
    }

    alternate_sample_rate = m->core->alternate_sample_rate;
    if (pa_modargs_get_alternate_sample_rate(ma, &alternate_sample_rate) < 0) {
        pa_log("Failed to parse alternate sample rate");
        goto fail;
    }

    frame_size = pa_frame_size(&ss);

    nfrags = m->core->default_n_fragments;
    frag_size = (uint32_t) pa_usec_to_bytes(m->core->default_fragment_size_msec*PA_USEC_PER_MSEC, &ss);
    if (frag_size <= 0)
        frag_size = (uint32_t) frame_size;
    tsched_size = (uint32_t) pa_usec_to_bytes(DEFAULT_TSCHED_BUFFER_USEC, &ss);
    tsched_watermark = (uint32_t) pa_usec_to_bytes(DEFAULT_TSCHED_WATERMARK_USEC, &ss);

    if (pa_modargs_get_value_u32(ma, "fragments", &nfrags) < 0 ||
        pa_modargs_get_value_u32(ma, "fragment_size", &frag_size) < 0 ||
        pa_modargs_get_value_u32(ma, "tsched_buffer_size", &tsched_size) < 0 ||
        pa_modargs_get_value_u32(ma, "tsched_buffer_watermark", &tsched_watermark) < 0) {
        pa_log("Failed to parse buffer metrics");
        goto fail;
    }

    buffer_size = nfrags * frag_size;

    period_frames = frag_size/frame_size;
    buffer_frames = buffer_size/frame_size;
    tsched_frames = tsched_size/frame_size;

    if (pa_modargs_get_value_boolean(ma, "mmap", &use_mmap) < 0) {
        pa_log("Failed to parse mmap argument.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "tsched", &use_tsched) < 0) {
        pa_log("Failed to parse tsched argument.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "ignore_dB", &ignore_dB) < 0) {
        pa_log("Failed to parse ignore_dB argument.");
        goto fail;
    }

    deferred_volume = m->core->deferred_volume;
    if (pa_modargs_get_value_boolean(ma, "deferred_volume", &deferred_volume) < 0) {
        pa_log("Failed to parse deferred_volume argument.");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "fixed_latency_range", &fixed_latency_range) < 0) {
        pa_log("Failed to parse fixed_latency_range argument.");
        goto fail;
    }

    use_tsched = pa_alsa_may_tsched(use_tsched);

    u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->use_mmap = use_mmap;
    u->use_tsched = use_tsched;
    u->deferred_volume = deferred_volume;
    u->fixed_latency_range = fixed_latency_range;
    u->first = true;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    u->smoother = pa_smoother_new(
            SMOOTHER_ADJUST_USEC,
            SMOOTHER_WINDOW_USEC,
            true,
            true,
            5,
            pa_rtclock_now(),
            true);
    u->smoother_interval = SMOOTHER_MIN_INTERVAL;

    /* use ucm */
    if (mapping && mapping->ucm_context.ucm)
        u->ucm_context = &mapping->ucm_context;

    dev_id = pa_modargs_get_value(
            ma, "device_id",
            pa_modargs_get_value(ma, "device", DEFAULT_DEVICE));

    u->paths_dir = pa_xstrdup(pa_modargs_get_value(ma, "paths_dir", NULL));

    if (reserve_init(u, dev_id) < 0)
        goto fail;

    if (reserve_monitor_init(u, dev_id) < 0)
        goto fail;

    b = use_mmap;
    d = use_tsched;

    if (mapping) {

        if (!(dev_id = pa_modargs_get_value(ma, "device_id", NULL))) {
            pa_log("device_id= not set");
            goto fail;
        }

        if ((mod_name = pa_proplist_gets(mapping->proplist, PA_ALSA_PROP_UCM_MODIFIER))) {
            if (snd_use_case_set(u->ucm_context->ucm->ucm_mgr, "_enamod", mod_name) < 0)
                pa_log("Failed to enable ucm modifier %s", mod_name);
            else
                pa_log_debug("Enabled ucm modifier %s", mod_name);
        }

        if (!(u->pcm_handle = pa_alsa_open_by_device_id_mapping(
                      dev_id,
                      &u->device_name,
                      &ss, &map,
                      SND_PCM_STREAM_CAPTURE,
                      &period_frames, &buffer_frames, tsched_frames,
                      &b, &d, mapping)))
            goto fail;

    } else if ((dev_id = pa_modargs_get_value(ma, "device_id", NULL))) {

        if (!(profile_set = pa_alsa_profile_set_new(NULL, &map)))
            goto fail;

        if (!(u->pcm_handle = pa_alsa_open_by_device_id_auto(
                      dev_id,
                      &u->device_name,
                      &ss, &map,
                      SND_PCM_STREAM_CAPTURE,
                      &period_frames, &buffer_frames, tsched_frames,
                      &b, &d, profile_set, &mapping)))
            goto fail;

    } else {

        if (!(u->pcm_handle = pa_alsa_open_by_device_string(
                      pa_modargs_get_value(ma, "device", DEFAULT_DEVICE),
                      &u->device_name,
                      &ss, &map,
                      SND_PCM_STREAM_CAPTURE,
                      &period_frames, &buffer_frames, tsched_frames,
                      &b, &d, false)))
            goto fail;
    }

    pa_assert(u->device_name);
    pa_log_info("Successfully opened device %s.", u->device_name);

    if (pa_alsa_pcm_is_modem(u->pcm_handle)) {
        pa_log_notice("Device %s is modem, refusing further initialization.", u->device_name);
        goto fail;
    }

    if (mapping)
        pa_log_info("Selected mapping '%s' (%s).", mapping->description, mapping->name);

    if (use_mmap && !b) {
        pa_log_info("Device doesn't support mmap(), falling back to UNIX read/write mode.");
        u->use_mmap = use_mmap = false;
    }

    if (use_tsched && (!b || !d)) {
        pa_log_info("Cannot enable timer-based scheduling, falling back to sound IRQ scheduling.");
        u->use_tsched = use_tsched = false;
    }

    if (u->use_mmap)
        pa_log_info("Successfully enabled mmap() mode.");

    if (u->use_tsched) {
        pa_log_info("Successfully enabled timer-based scheduling mode.");
        if (u->fixed_latency_range)
            pa_log_info("Disabling latency range changes on overrun");
    }

    u->rates = pa_alsa_get_supported_rates(u->pcm_handle, ss.rate);
    if (!u->rates) {
        pa_log_error("Failed to find any supported sample rates.");
        goto fail;
    }

    /* ALSA might tweak the sample spec, so recalculate the frame size */
    frame_size = pa_frame_size(&ss);

    if (!u->ucm_context)
        find_mixer(u, mapping, pa_modargs_get_value(ma, "control", NULL), ignore_dB);

    pa_source_new_data_init(&data);
    data.driver = driver;
    data.module = m;
    data.card = card;
    set_source_name(&data, ma, dev_id, u->device_name, mapping);

    /* We need to give pa_modargs_get_value_boolean() a pointer to a local
     * variable instead of using &data.namereg_fail directly, because
     * data.namereg_fail is a bitfield and taking the address of a bitfield
     * variable is impossible. */
    namereg_fail = data.namereg_fail;
    if (pa_modargs_get_value_boolean(ma, "namereg_fail", &namereg_fail) < 0) {
        pa_log("Failed to parse namereg_fail argument.");
        pa_source_new_data_done(&data);
        goto fail;
    }
    data.namereg_fail = namereg_fail;

    pa_source_new_data_set_sample_spec(&data, &ss);
    pa_source_new_data_set_channel_map(&data, &map);
    pa_source_new_data_set_alternate_sample_rate(&data, alternate_sample_rate);

    pa_alsa_init_proplist_pcm(m->core, data.proplist, u->pcm_handle);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, u->device_name);
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_BUFFERING_BUFFER_SIZE, "%lu", (unsigned long) (buffer_frames * frame_size));
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_BUFFERING_FRAGMENT_SIZE, "%lu", (unsigned long) (period_frames * frame_size));
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_ACCESS_MODE, u->use_tsched ? "mmap+timer" : (u->use_mmap ? "mmap" : "serial"));

    if (mapping) {
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_PROFILE_NAME, mapping->name);
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_PROFILE_DESCRIPTION, mapping->description);

        while ((key = pa_proplist_iterate(mapping->proplist, &state)))
            pa_proplist_sets(data.proplist, key, pa_proplist_gets(mapping->proplist, key));
    }

    pa_alsa_init_description(data.proplist, card);

    if (u->control_device)
        pa_alsa_init_proplist_ctl(data.proplist, u->control_device);

    if (pa_modargs_get_proplist(ma, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&data);
        goto fail;
    }

    if (u->ucm_context)
        pa_alsa_ucm_add_ports(&data.ports, data.proplist, u->ucm_context, false, card);
    else if (u->mixer_path_set)
        pa_alsa_add_ports(&data, u->mixer_path_set, card);

    u->source = pa_source_new(m->core, &data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY|(u->use_tsched ? PA_SOURCE_DYNAMIC_LATENCY : 0));
    pa_source_new_data_done(&data);

    if (!u->source) {
        pa_log("Failed to create source object");
        goto fail;
    }

    if (pa_modargs_get_value_u32(ma, "deferred_volume_safety_margin",
                                 &u->source->thread_info.volume_change_safety_margin) < 0) {
        pa_log("Failed to parse deferred_volume_safety_margin parameter");
        goto fail;
    }

    if (pa_modargs_get_value_s32(ma, "deferred_volume_extra_delay",
                                 &u->source->thread_info.volume_change_extra_delay) < 0) {
        pa_log("Failed to parse deferred_volume_extra_delay parameter");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg;
    if (u->use_tsched)
        u->source->update_requested_latency = source_update_requested_latency_cb;
    u->source->set_state = source_set_state_cb;
    if (u->ucm_context)
        u->source->set_port = source_set_port_ucm_cb;
    else
        u->source->set_port = source_set_port_cb;
    if (u->source->alternate_sample_rate)
        u->source->update_rate = source_update_rate_cb;
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    u->frame_size = frame_size;
    u->frames_per_block = pa_mempool_block_size_max(m->core->mempool) / frame_size;
    u->fragment_size = frag_size = (size_t) (period_frames * frame_size);
    u->hwbuf_size = buffer_size = (size_t) (buffer_frames * frame_size);
    pa_cvolume_mute(&u->hardware_volume, u->source->sample_spec.channels);

    pa_log_info("Using %0.1f fragments of size %lu bytes (%0.2fms), buffer size is %lu bytes (%0.2fms)",
                (double) u->hwbuf_size / (double) u->fragment_size,
                (long unsigned) u->fragment_size,
                (double) pa_bytes_to_usec(u->fragment_size, &ss) / PA_USEC_PER_MSEC,
                (long unsigned) u->hwbuf_size,
                (double) pa_bytes_to_usec(u->hwbuf_size, &ss) / PA_USEC_PER_MSEC);

    if (u->use_tsched) {
        u->tsched_watermark_ref = tsched_watermark;
        reset_watermark(u, u->tsched_watermark_ref, &ss, false);
    }
    else
        pa_source_set_fixed_latency(u->source, pa_bytes_to_usec(u->hwbuf_size, &ss));

    reserve_update(u);

    if (update_sw_params(u) < 0)
        goto fail;

    if (u->ucm_context) {
        if (u->source->active_port && pa_alsa_ucm_set_port(u->ucm_context, u->source->active_port, false) < 0)
            goto fail;
    } else if (setup_mixer(u, ignore_dB) < 0)
        goto fail;

    pa_alsa_dump(PA_LOG_DEBUG, u->pcm_handle);

    thread_name = pa_sprintf_malloc("alsa-source-%s", pa_strnull(pa_proplist_gets(u->source->proplist, "alsa.id")));
    if (!(u->thread = pa_thread_new(thread_name, thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }
    pa_xfree(thread_name);
    thread_name = NULL;

    /* Get initial mixer settings */
    if (data.volume_is_set) {
        if (u->source->set_volume)
            u->source->set_volume(u->source);
    } else {
        if (u->source->get_volume)
            u->source->get_volume(u->source);
    }

    if (data.muted_is_set) {
        if (u->source->set_mute)
            u->source->set_mute(u->source);
    } else {
        if (u->source->get_mute) {
            bool mute;

            if (u->source->get_mute(u->source, &mute) >= 0)
                pa_source_set_mute(u->source, mute, false);
        }
    }

    if ((data.volume_is_set || data.muted_is_set) && u->source->write_volume)
        u->source->write_volume(u->source);

    pa_source_put(u->source);

    if (profile_set)
        pa_alsa_profile_set_free(profile_set);

    return u->source;

fail:
    pa_xfree(thread_name);

    if (u)
        userdata_free(u);

    if (profile_set)
        pa_alsa_profile_set_free(profile_set);

    return NULL;
}

static void userdata_free(struct userdata *u) {
    pa_assert(u);

    if (u->source)
        pa_source_unlink(u->source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->source)
        pa_source_unref(u->source);

    if (u->mixer_pd)
        pa_alsa_mixer_pdata_free(u->mixer_pd);

    if (u->alsa_rtpoll_item)
        pa_rtpoll_item_free(u->alsa_rtpoll_item);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    if (u->pcm_handle) {
        snd_pcm_drop(u->pcm_handle);
        snd_pcm_close(u->pcm_handle);
    }

    if (u->mixer_fdl)
        pa_alsa_fdlist_free(u->mixer_fdl);

    if (u->mixer_path && !u->mixer_path_set)
        pa_alsa_path_free(u->mixer_path);

    if (u->mixer_handle)
        snd_mixer_close(u->mixer_handle);

    if (u->smoother)
        pa_smoother_free(u->smoother);

    if (u->rates)
        pa_xfree(u->rates);

    reserve_done(u);
    monitor_done(u);

    pa_xfree(u->device_name);
    pa_xfree(u->control_device);
    pa_xfree(u->paths_dir);
    pa_xfree(u);
}

void pa_alsa_source_free(pa_source *s) {
    struct userdata *u;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    userdata_free(u);
}
