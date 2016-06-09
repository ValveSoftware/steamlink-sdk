/***
  This file is part of PulseAudio.

  This module is based off Lennart Poettering's LADSPA sink and swaps out
  LADSPA functionality for a dbus-aware STFT OLA based digital equalizer.
  All new work is published under PulseAudio's original license.

  Copyright 2009 Jason Newton <nevion@gmail.com>

  Original Author:
  Copyright 2004-2008 Lennart Poettering

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

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

//#undef __SSE2__
#ifdef __SSE2__
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

#include <fftw3.h>

#include <pulse/xmalloc.h>
#include <pulse/timeval.h>

#include <pulsecore/core-rtclock.h>
#include <pulsecore/i18n.h>
#include <pulsecore/aupdate.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/shared.h>
#include <pulsecore/idxset.h>
#include <pulsecore/strlist.h>
#include <pulsecore/database.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/dbus-util.h>

#include "module-equalizer-sink-symdef.h"

PA_MODULE_AUTHOR("Jason Newton");
PA_MODULE_DESCRIPTION(_("General Purpose Equalizer"));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        _("sink_name=<name of the sink> "
          "sink_properties=<properties for the sink> "
          "sink_master=<sink to connect to> "
          "format=<sample format> "
          "rate=<sample rate> "
          "channels=<number of channels> "
          "channel_map=<channel map> "
          "autoloaded=<set if this module is being loaded automatically> "
          "use_volume_sharing=<yes or no> "
         ));

#define MEMBLOCKQ_MAXLENGTH (16*1024*1024)
#define DEFAULT_AUTOLOADED false

struct userdata {
    pa_module *module;
    pa_sink *sink;
    pa_sink_input *sink_input;
    bool autoloaded;

    size_t channels;
    size_t fft_size;//length (res) of fft
    size_t window_size;/*
                        *sliding window size
                        *effectively chooses R
                        */
    size_t R;/* the hop size between overlapping windows
              * the latency of the filter, calculated from window_size
              * based on constraints of COLA and window function
              */
    //for twiddling with pulseaudio
    size_t overlap_size;//window_size-R
    size_t samples_gathered;
    size_t input_buffer_max;
    //message
    float *W;//windowing function (time domain)
    float *work_buffer, **input, **overlap_accum;
    fftwf_complex *output_window;
    fftwf_plan forward_plan, inverse_plan;
    //size_t samplings;

    float **Xs;
    float ***Hs;//thread updatable copies of the freq response filters (magnitude based)
    pa_aupdate **a_H;
    pa_memblockq *input_q;
    char *output_buffer;
    size_t output_buffer_length;
    size_t output_buffer_max_length;
    pa_memblockq *output_q;
    bool first_iteration;

    pa_dbus_protocol *dbus_protocol;
    char *dbus_path;

    pa_database *database;
    char **base_profiles;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "sink_master",
    "format",
    "rate",
    "channels",
    "channel_map",
    "autoloaded",
    "use_volume_sharing",
    NULL
};

#define v_size 4
#define SINKLIST "equalized_sinklist"
#define EQDB "equalizer_db"
#define EQ_STATE_DB "equalizer-state"
#define FILTER_SIZE(u) ((u)->fft_size / 2 + 1)
#define CHANNEL_PROFILE_SIZE(u) (FILTER_SIZE(u) + 1)
#define FILTER_STATE_SIZE(u) (CHANNEL_PROFILE_SIZE(u) * (u)->channels)

static void dbus_init(struct userdata *u);
static void dbus_done(struct userdata *u);

static void hanning_window(float *W, size_t window_size) {
    /* h=.5*(1-cos(2*pi*j/(window_size+1)), COLA for R=(M+1)/2 */
    for (size_t i = 0; i < window_size; ++i)
        W[i] = (float).5 * (1 - cos(2*M_PI*i / (window_size+1)));
}

static void fix_filter(float *H, size_t fft_size) {
    /* divide out the fft gain */
    for (size_t i = 0; i < fft_size / 2 + 1; ++i)
        H[i] /= fft_size;
}

static void interpolate(float *samples, size_t length, uint32_t *xs, float *ys, size_t n_points) {
    /* Note that xs must be monotonically increasing! */
    float x_range_lower, x_range_upper, c0;

    pa_assert(n_points >= 2);
    pa_assert(xs[0] == 0);
    pa_assert(xs[n_points - 1] == length - 1);

    for (size_t x = 0, x_range_lower_i = 0; x < length-1; ++x) {
        pa_assert(x_range_lower_i < n_points-1);

        x_range_lower = (float) xs[x_range_lower_i];
        x_range_upper = (float) xs[x_range_lower_i+1];

        pa_assert_se(x_range_lower < x_range_upper);
        pa_assert_se(x >= x_range_lower);
        pa_assert_se(x <= x_range_upper);

        /* bilinear-interpolation of coefficients specified */
        c0 = (x-x_range_lower) / (x_range_upper-x_range_lower);
        pa_assert(c0 >= 0 && c0 <= 1.0);

        samples[x] = ((1.0f - c0) * ys[x_range_lower_i] + c0 * ys[x_range_lower_i + 1]);
        while(x >= xs[x_range_lower_i + 1])
            x_range_lower_i++;
    }

    samples[length-1] = ys[n_points-1];
}

static bool is_monotonic(const uint32_t *xs, size_t length) {
    pa_assert(xs);

    if (length < 2)
        return true;

    for(size_t i = 1; i < length; ++i)
        if (xs[i] <= xs[i-1])
            return false;

    return true;
}

/* ensures memory allocated is a multiple of v_size and aligned */
static void * alloc(size_t x, size_t s) {
    size_t f;
    float *t;

    f = PA_ROUND_UP(x*s, sizeof(float)*v_size);
    pa_assert_se(t = fftwf_malloc(f));
    pa_memzero(t, f);

    return t;
}

static void alloc_input_buffers(struct userdata *u, size_t min_buffer_length) {
    if (min_buffer_length <= u->input_buffer_max)
        return;

    pa_assert(min_buffer_length >= u->window_size);
    for (size_t c = 0; c < u->channels; ++c) {
        float *tmp = alloc(min_buffer_length, sizeof(float));
        if (u->input[c]) {
            if (!u->first_iteration)
                memcpy(tmp, u->input[c], u->overlap_size * sizeof(float));
            fftwf_free(u->input[c]);
        }
        u->input[c] = tmp;
    }
    u->input_buffer_max = min_buffer_length;
}

/* Called from I/O thread context */
static int sink_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY: {
            //size_t fs=pa_frame_size(&u->sink->sample_spec);

            /* The sink is _put() before the sink input is, so let's
             * make sure we don't access it in that time. Also, the
             * sink input is first shut down, the sink second. */
            if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
                !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            *((pa_usec_t*) data) =
                /* Get the latency of the master sink */
                pa_sink_get_latency_within_thread(u->sink_input->sink) +

                /* Add the latency internal to our sink input on top */
                pa_bytes_to_usec(pa_memblockq_get_length(u->output_q) +
                                 pa_memblockq_get_length(u->input_q), &u->sink_input->sink->sample_spec) +
                pa_bytes_to_usec(pa_memblockq_get_length(u->sink_input->thread_info.render_memblockq), &u->sink_input->sink->sample_spec);
            //    pa_bytes_to_usec(u->samples_gathered * fs, &u->sink->sample_spec);
            //+ pa_bytes_to_usec(u->latency * fs, ss)
            return 0;
        }
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int sink_set_state_cb(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return 0;

    pa_sink_input_cork(u->sink_input, state == PA_SINK_SUSPENDED);
    return 0;
}

/* Called from I/O thread context */
static void sink_request_rewind_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_request_rewind(u->sink_input, s->thread_info.rewind_nbytes+pa_memblockq_get_length(u->input_q), true, false, false);
}

/* Called from I/O thread context */
static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_set_requested_latency_within_thread(
            u->sink_input,
            pa_sink_get_requested_latency_within_thread(s));
}

/* Called from main context */
static void sink_set_volume_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return;

    pa_sink_input_set_volume(u->sink_input, &s->real_volume, s->save_volume, true);
}

/* Called from main context */
static void sink_set_mute_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)) ||
        !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return;

    pa_sink_input_set_mute(u->sink_input, s->muted, s->save_muted);
}

#if 1
//reference implementation
static void dsp_logic(
    float * restrict dst,//used as a temp array too, needs to be fft_length!
    float * restrict src,/*input data w/ overlap at start,
                               *automatically cycled in routine
                               */
    float * restrict overlap,
    const float X,//multiplier
    const float * restrict H,//The freq. magnitude scalers filter
    const float * restrict W,//The windowing function
    fftwf_complex * restrict output_window,//The transformed windowed src
    struct userdata *u) {

    //use a linear-phase sliding STFT and overlap-add method (for each channel)
    //window the data
    for(size_t j = 0; j < u->window_size; ++j) {
        dst[j] = X * W[j] * src[j];
    }
    //zero pad the remaining fft window
    memset(dst + u->window_size, 0, (u->fft_size - u->window_size) * sizeof(float));
    //Processing is done here!
    //do fft
    fftwf_execute_dft_r2c(u->forward_plan, dst, output_window);
    //perform filtering
    for(size_t j = 0; j < FILTER_SIZE(u); ++j) {
        u->output_window[j][0] *= H[j];
        u->output_window[j][1] *= H[j];
    }
    //inverse fft
    fftwf_execute_dft_c2r(u->inverse_plan, output_window, dst);
    ////debug: tests overlapping add
    ////and negates ALL PREVIOUS processing
    ////yields a perfect reconstruction if COLA is held
    //for(size_t j = 0; j < u->window_size; ++j) {
    //    u->work_buffer[j] = u->W[j] * u->input[c][j];
    //}

    //overlap add and preserve overlap component from this window (linear phase)
    for(size_t j = 0; j < u->overlap_size; ++j) {
        u->work_buffer[j] += overlap[j];
        overlap[j] = dst[u->R + j];
    }
    ////debug: tests if basic buffering works
    ////shouldn't modify the signal AT ALL (beyond roundoff)
    //for(size_t j = 0; j < u->window_size;++j) {
    //    u->work_buffer[j] = u->input[c][j];
    //}

    //preserve the needed input for the next window's overlap
    memmove(src, src + u->R,
        (u->samples_gathered - u->R) * sizeof(float)
    );
}
#else
typedef float v4sf __attribute__ ((__aligned__(v_size * sizeof(float))));
typedef union float_vector {
    float f[v_size];
    v4sf v;
    __m128 m;
} float_vector_t;

//regardless of sse enabled, the loops in here assume
//16 byte aligned addresses and memory allocations divisible by v_size
static void dsp_logic(
    float * restrict dst,//used as a temp array too, needs to be fft_length!
    float * restrict src,/*input data w/ overlap at start,
                               *automatically cycled in routine
                               */
    float * restrict overlap,//The size of the overlap
    const float X,//multiplier
    const float * restrict H,//The freq. magnitude scalers filter
    const float * restrict W,//The windowing function
    fftwf_complex * restrict output_window,//The transformed windowed src
    struct userdata *u) {//Collection of constants
    const size_t overlap_size = PA_ROUND_UP(u->overlap_size, v_size);
    float_vector_t x;
    x.f[0] = x.f[1] = x.f[2] = x.f[3] = X;

    //assert(u->samples_gathered >= u->R);
    //use a linear-phase sliding STFT and overlap-add method
    for(size_t j = 0; j < u->window_size; j += v_size) {
        //dst[j] = W[j] * src[j];
        float_vector_t *d = (float_vector_t*) (dst + j);
        float_vector_t *w = (float_vector_t*) (W + j);
        float_vector_t *s = (float_vector_t*) (src + j);
//#if __SSE2__
        d->m = _mm_mul_ps(x.m, _mm_mul_ps(w->m, s->m));
//        d->v = x->v * w->v * s->v;
//#endif
    }
    //zero pad the remaining fft window
    memset(dst + u->window_size, 0, (u->fft_size - u->window_size) * sizeof(float));

    //Processing is done here!
    //do fft
    fftwf_execute_dft_r2c(u->forward_plan, dst, output_window);
    //perform filtering - purely magnitude based
    for(size_t j = 0; j < FILTER_SIZE; j += v_size / 2) {
        //output_window[j][0]*=H[j];
        //output_window[j][1]*=H[j];
        float_vector_t *d = (float_vector_t*)( ((float *) output_window) + 2 * j);
        float_vector_t h;
        h.f[0] = h.f[1] = H[j];
        h.f[2] = h.f[3] = H[j + 1];
//#if __SSE2__
        d->m = _mm_mul_ps(d->m, h.m);
//#else
//        d->v = d->v * h.v;
//#endif
    }

    //inverse fft
    fftwf_execute_dft_c2r(u->inverse_plan, output_window, dst);

    ////debug: tests overlapping add
    ////and negates ALL PREVIOUS processing
    ////yields a perfect reconstruction if COLA is held
    //for(size_t j = 0; j < u->window_size; ++j) {
    //    dst[j] = W[j] * src[j];
    //}

    //overlap add and preserve overlap component from this window (linear phase)
    for(size_t j = 0; j < overlap_size; j += v_size) {
        //dst[j]+=overlap[j];
        //overlap[j]+=dst[j+R];
        float_vector_t *d = (float_vector_t*)(dst + j);
        float_vector_t *o = (float_vector_t*)(overlap + j);
//#if __SSE2__
        d->m = _mm_add_ps(d->m, o->m);
        o->m = ((float_vector_t*)(dst + u->R + j))->m;
//#else
//        d->v = d->v + o->v;
//        o->v = ((float_vector_t*)(dst + u->R + j))->v;
//#endif
    }
    //memcpy(overlap, dst+u->R, u->overlap_size * sizeof(float)); //overlap preserve (debug)
    //zero out the bit beyond the real overlap so we don't add garbage next iteration
    memset(overlap + u->overlap_size, 0, overlap_size - u->overlap_size);

    ////debug: tests if basic buffering works
    ////shouldn't modify the signal AT ALL (beyond roundoff)
    //for(size_t j = 0; j < u->window_size; ++j) {
    //    dst[j] = src[j];
    //}

    //preserve the needed input for the next window's overlap
    memmove(src, src + u->R,
        (u->samples_gathered - u->R) * sizeof(float)
    );
}
#endif

static void flatten_to_memblockq(struct userdata *u) {
    size_t mbs = pa_mempool_block_size_max(u->sink->core->mempool);
    pa_memchunk tchunk;
    char *dst;
    size_t i = 0;
    while(i < u->output_buffer_length) {
        tchunk.index = 0;
        tchunk.length = PA_MIN((u->output_buffer_length - i), mbs);
        tchunk.memblock = pa_memblock_new(u->sink->core->mempool, tchunk.length);
        //pa_log_debug("pushing %ld into the q", tchunk.length);
        dst = pa_memblock_acquire(tchunk.memblock);
        memcpy(dst, u->output_buffer + i, tchunk.length);
        pa_memblock_release(tchunk.memblock);
        pa_memblockq_push(u->output_q, &tchunk);
        pa_memblock_unref(tchunk.memblock);
        i += tchunk.length;
    }
}

static void process_samples(struct userdata *u) {
    size_t fs = pa_frame_size(&(u->sink->sample_spec));
    unsigned a_i;
    float *H, X;
    size_t iterations, offset;
    pa_assert(u->samples_gathered >= u->window_size);
    iterations = (u->samples_gathered - u->overlap_size) / u->R;
    //make sure there is enough buffer memory allocated
    if (iterations * u->R * fs > u->output_buffer_max_length) {
        u->output_buffer_max_length = iterations * u->R * fs;
        pa_xfree(u->output_buffer);
        u->output_buffer = pa_xmalloc(u->output_buffer_max_length);
    }
    u->output_buffer_length = iterations * u->R * fs;

    for(size_t iter = 0; iter < iterations; ++iter) {
        offset = iter * u->R * fs;
        for(size_t c = 0;c < u->channels; c++) {
            a_i = pa_aupdate_read_begin(u->a_H[c]);
            X = u->Xs[c][a_i];
            H = u->Hs[c][a_i];
            dsp_logic(
                u->work_buffer,
                u->input[c],
                u->overlap_accum[c],
                X,
                H,
                u->W,
                u->output_window,
                u
            );
            pa_aupdate_read_end(u->a_H[c]);
            if (u->first_iteration) {
                /* The windowing function will make the audio ramped in, as a cheap fix we can
                 * undo the windowing (for non-zero window values)
                 */
                for(size_t i = 0; i < u->overlap_size; ++i) {
                    u->work_buffer[i] = u->W[i] <= FLT_EPSILON ? u->work_buffer[i] : u->work_buffer[i] / u->W[i];
                }
            }
            pa_sample_clamp(PA_SAMPLE_FLOAT32NE, (uint8_t *) (((float *)u->output_buffer) + c) + offset, fs, u->work_buffer, sizeof(float), u->R);
        }
        if (u->first_iteration) {
            u->first_iteration = false;
        }
        u->samples_gathered -= u->R;
    }
    flatten_to_memblockq(u);
}

static void input_buffer(struct userdata *u, pa_memchunk *in) {
    size_t fs = pa_frame_size(&(u->sink->sample_spec));
    size_t samples = in->length/fs;
    float *src = pa_memblock_acquire_chunk(in);
    pa_assert(u->samples_gathered + samples <= u->input_buffer_max);
    for(size_t c = 0; c < u->channels; c++) {
        //buffer with an offset after the overlap from previous
        //iterations
        pa_assert_se(
            u->input[c] + u->samples_gathered + samples <= u->input[c] + u->input_buffer_max
        );
        pa_sample_clamp(PA_SAMPLE_FLOAT32NE, u->input[c] + u->samples_gathered, sizeof(float), src + c, fs, samples);
    }
    u->samples_gathered += samples;
    pa_memblock_release(in->memblock);
}

/* Called from I/O thread context */
static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;
    size_t fs, target_samples;
    size_t mbs;
    //struct timeval start, end;
    pa_memchunk tchunk;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);
    pa_assert(chunk);
    pa_assert(u->sink);

    /* FIXME: Please clean this up. I see more commented code lines
     * than uncommented code lines. I am sorry, but I am too dumb to
     * understand this. */

    fs = pa_frame_size(&(u->sink->sample_spec));
    mbs = pa_mempool_block_size_max(u->sink->core->mempool);
    if (pa_memblockq_get_length(u->output_q) > 0) {
        //pa_log_debug("qsize is %ld", pa_memblockq_get_length(u->output_q));
        goto END;
    }
    //nbytes = PA_MIN(nbytes, pa_mempool_block_size_max(u->sink->core->mempool));
    target_samples = PA_ROUND_UP(nbytes / fs, u->R);
    ////pa_log_debug("vanilla mbs = %ld",mbs);
    //mbs = PA_ROUND_DOWN(mbs / fs, u->R);
    //mbs = PA_MAX(mbs, u->R);
    //target_samples = PA_MAX(target_samples, mbs);
    //pa_log_debug("target samples: %ld", target_samples);
    if (u->first_iteration) {
        //allocate request_size
        target_samples = PA_MAX(target_samples, u->window_size);
    }else{
        //allocate request_size + overlap
        target_samples += u->overlap_size;
    }
    alloc_input_buffers(u, target_samples);
    //pa_log_debug("post target samples: %ld", target_samples);
    chunk->memblock = NULL;

    /* Hmm, process any rewind request that might be queued up */
    pa_sink_process_rewind(u->sink, 0);

    //pa_log_debug("start output-buffered %ld, input-buffered %ld, requested %ld",buffered_samples,u->samples_gathered,samples_requested);
    //pa_rtclock_get(&start);
    do{
        size_t input_remaining = target_samples - u->samples_gathered;
       // pa_log_debug("input remaining %ld samples", input_remaining);
        pa_assert(input_remaining > 0);
        while (pa_memblockq_peek(u->input_q, &tchunk) < 0) {
            //pa_sink_render(u->sink, input_remaining * fs, &tchunk);
            pa_sink_render_full(u->sink, PA_MIN(input_remaining * fs, mbs), &tchunk);
            pa_memblockq_push(u->input_q, &tchunk);
            pa_memblock_unref(tchunk.memblock);
        }
        pa_assert(tchunk.memblock);

        tchunk.length = PA_MIN(input_remaining * fs, tchunk.length);

        pa_memblockq_drop(u->input_q, tchunk.length);
        //pa_log_debug("asked for %ld input samples, got %ld samples",input_remaining,buffer->length/fs);
        /* copy new input */
        //pa_rtclock_get(start);
       // pa_log_debug("buffering %ld bytes", tchunk.length);
        input_buffer(u, &tchunk);
        //pa_rtclock_get(&end);
        //pa_log_debug("Took %0.5f seconds to setup", pa_timeval_diff(end, start) / (double) PA_USEC_PER_SEC);
        pa_memblock_unref(tchunk.memblock);
    } while(u->samples_gathered < target_samples);

    //pa_rtclock_get(&end);
    //pa_log_debug("Took %0.6f seconds to get data", (double) pa_timeval_diff(&end, &start) / PA_USEC_PER_SEC);

    pa_assert(u->fft_size >= u->window_size);
    pa_assert(u->R < u->window_size);
    //pa_rtclock_get(&start);
    /* process a block */
    process_samples(u);
    //pa_rtclock_get(&end);
    //pa_log_debug("Took %0.6f seconds to process", (double) pa_timeval_diff(&end, &start) / PA_USEC_PER_SEC);
END:
    pa_assert_se(pa_memblockq_peek(u->output_q, chunk) >= 0);
    pa_assert(chunk->memblock);
    pa_memblockq_drop(u->output_q, chunk->length);

    //pa_log_debug("gave %ld", chunk->length/fs);
    //pa_log_debug("end pop");
    return 0;
}

/* Called from main context */
static void sink_input_volume_changed_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_volume_changed(u->sink, &i->volume);
}

/* Called from main context */
static void sink_input_mute_changed_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_mute_changed(u->sink, i->muted);
}

#if 0
static void reset_filter(struct userdata *u) {
    size_t fs = pa_frame_size(&u->sink->sample_spec);
    size_t max_request;

    u->samples_gathered = 0;

    for(size_t i = 0; i < u->channels; ++i)
        pa_memzero(u->overlap_accum[i], u->overlap_size * sizeof(float));

    u->first_iteration = true;
    //set buffer size to max request, no overlap copy
    max_request = PA_ROUND_UP(pa_sink_input_get_max_request(u->sink_input) / fs , u->R);
    max_request = PA_MAX(max_request, u->window_size);
    pa_sink_set_max_request_within_thread(u->sink, max_request * fs);
}
#endif

/* Called from I/O thread context */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;
    size_t amount = 0;

    pa_log_debug("Rewind callback!");
    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->sink->thread_info.rewind_nbytes > 0) {
        size_t max_rewrite;

        //max_rewrite = nbytes;
        max_rewrite = nbytes + pa_memblockq_get_length(u->input_q);
        //PA_MIN(pa_memblockq_get_length(u->input_q), nbytes);
        amount = PA_MIN(u->sink->thread_info.rewind_nbytes, max_rewrite);
        u->sink->thread_info.rewind_nbytes = 0;

        if (amount > 0) {
            //invalidate the output q
            pa_memblockq_seek(u->input_q, - (int64_t) amount, PA_SEEK_RELATIVE, true);
            pa_log("Resetting filter");
            //reset_filter(u); //this is the "proper" thing to do...
        }
    }

    pa_sink_process_rewind(u->sink, amount);
    pa_memblockq_rewind(u->input_q, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_memblockq_set_maxrewind(u->input_q, nbytes);
    pa_sink_set_max_rewind_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;
    size_t fs;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    fs = pa_frame_size(&u->sink_input->sample_spec);
    pa_sink_set_max_request_within_thread(u->sink, PA_ROUND_UP(nbytes / fs, u->R) * fs);
}

/* Called from I/O thread context */
static void sink_input_update_sink_latency_range_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
}

/* Called from I/O thread context */
static void sink_input_update_sink_fixed_latency_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
}

/* Called from I/O thread context */
static void sink_input_detach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_detach_within_thread(u->sink);

    pa_sink_set_rtpoll(u->sink, NULL);
}

/* Called from I/O thread context */
static void sink_input_attach_cb(pa_sink_input *i) {
    struct userdata *u;
    size_t fs, max_request;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_rtpoll(u->sink, i->sink->thread_info.rtpoll);
    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);

    fs = pa_frame_size(&u->sink_input->sample_spec);
    /* set buffer size to max request, no overlap copy */
    max_request = PA_ROUND_UP(pa_sink_input_get_max_request(u->sink_input) / fs, u->R);
    max_request = PA_MAX(max_request, u->window_size);

    pa_sink_set_max_request_within_thread(u->sink, max_request * fs);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i));

    pa_sink_attach_within_thread(u->sink);
}

/* Called from main context */
static void sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* The order here matters! We first kill the sink input, followed
     * by the sink. That means the sink callbacks must be protected
     * against an unconnected sink input! */
    pa_sink_input_unlink(u->sink_input);
    pa_sink_unlink(u->sink);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    /* Leave u->sink alone for now, it will be cleaned up on module
     * unload (and it is needed during unload as well). */

    pa_module_unload_request(u->module, true);
}

/* Called from IO thread context */
static void sink_input_state_change_cb(pa_sink_input *i, pa_sink_input_state_t state) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* If we are added for the first time, ask for a rewinding so that
     * we are heard right-away. */
    if (PA_SINK_INPUT_IS_LINKED(state) &&
        i->thread_info.state == PA_SINK_INPUT_INIT) {
        pa_log_debug("Requesting rewind due to state change.");
        pa_sink_input_request_rewind(i, 0, false, true, true);
    }
}

static void pack(char **strs, size_t len, char **packed, size_t *length) {
    size_t t_len = 0;
    size_t headers = (1+len) * sizeof(uint16_t);
    char *p;
    for(size_t i = 0; i < len; ++i) {
        t_len += strlen(strs[i]);
    }
    *length = headers + t_len;
    p = *packed = pa_xmalloc0(*length);
    *((uint16_t *) p) = (uint16_t) len;
    p += sizeof(uint16_t);
    for(size_t i = 0; i < len; ++i) {
        uint16_t l = strlen(strs[i]);
        *((uint16_t *) p) = (uint16_t) l;
        p += sizeof(uint16_t);
        memcpy(p, strs[i], l);
        p += l;
    }
}
static void unpack(char *str, size_t length, char ***strs, size_t *len) {
    char *p = str;
    *len = *((uint16_t *) p);
    p += sizeof(uint16_t);
    *strs = pa_xnew(char *, *len);

    for(size_t i = 0; i < *len; ++i) {
        size_t l = *((uint16_t *) p);
        p += sizeof(uint16_t);
        (*strs)[i] = pa_xnew(char, l + 1);
        memcpy((*strs)[i], p, l);
        (*strs)[i][l] = '\0';
        p += l;
    }
}
static void save_profile(struct userdata *u, size_t channel, char *name) {
    unsigned a_i;
    const size_t profile_size = CHANNEL_PROFILE_SIZE(u) * sizeof(float);
    float *H_n, *profile;
    const float *H;
    pa_datum key, data;
    profile = pa_xnew0(float, profile_size);
    a_i = pa_aupdate_read_begin(u->a_H[channel]);
    profile[0] = u->Xs[a_i][channel];
    H = u->Hs[channel][a_i];
    H_n = profile + 1;
    for(size_t i = 0 ; i < FILTER_SIZE(u); ++i) {
        H_n[i] = H[i] * u->fft_size;
        //H_n[i] = H[i];
    }
    pa_aupdate_read_end(u->a_H[channel]);
    key.data=name;
    key.size = strlen(key.data);
    data.data = profile;
    data.size = profile_size;
    pa_database_set(u->database, &key, &data, true);
    pa_database_sync(u->database);
    if (u->base_profiles[channel]) {
        pa_xfree(u->base_profiles[channel]);
    }
    u->base_profiles[channel] = pa_xstrdup(name);
}

static void save_state(struct userdata *u) {
    unsigned a_i;
    const size_t filter_state_size = FILTER_STATE_SIZE(u) * sizeof(float);
    float *H_n, *state;
    float *H;
    pa_datum key, data;
    pa_database *database;
    char *dbname;
    char *packed;
    size_t packed_length;

    pack(u->base_profiles, u->channels, &packed, &packed_length);
    state = (float *) pa_xmalloc0(filter_state_size + packed_length);
    memcpy(state + FILTER_STATE_SIZE(u), packed, packed_length);
    pa_xfree(packed);

    for(size_t c = 0; c < u->channels; ++c) {
        a_i = pa_aupdate_read_begin(u->a_H[c]);
        state[c * CHANNEL_PROFILE_SIZE(u)] = u->Xs[c][a_i];
        H = u->Hs[c][a_i];
        H_n = &state[c * CHANNEL_PROFILE_SIZE(u) + 1];
        memcpy(H_n, H, FILTER_SIZE(u) * sizeof(float));
        pa_aupdate_read_end(u->a_H[c]);
    }

    key.data = u->sink->name;
    key.size = strlen(key.data);
    data.data = state;
    data.size = filter_state_size + packed_length;
    //thread safety for 0.9.17?
    pa_assert_se(dbname = pa_state_path(EQ_STATE_DB, false));
    pa_assert_se(database = pa_database_open(dbname, true));
    pa_xfree(dbname);

    pa_database_set(database, &key, &data, true);
    pa_database_sync(database);
    pa_database_close(database);
    pa_xfree(state);
}

static void remove_profile(pa_core *c, char *name) {
    pa_datum key;
    pa_database *database;
    key.data = name;
    key.size = strlen(key.data);
    pa_assert_se(database = pa_shared_get(c, EQDB));
    pa_database_unset(database, &key);
    pa_database_sync(database);
}

static const char* load_profile(struct userdata *u, size_t channel, char *name) {
    unsigned a_i;
    pa_datum key, value;
    const size_t profile_size = CHANNEL_PROFILE_SIZE(u) * sizeof(float);
    key.data = name;
    key.size = strlen(key.data);
    if (pa_database_get(u->database, &key, &value) != NULL) {
        if (value.size == profile_size) {
            float *profile = (float *) value.data;
            a_i = pa_aupdate_write_begin(u->a_H[channel]);
            u->Xs[channel][a_i] = profile[0];
            memcpy(u->Hs[channel][a_i], profile + 1, FILTER_SIZE(u) * sizeof(float));
            fix_filter(u->Hs[channel][a_i], u->fft_size);
            pa_aupdate_write_end(u->a_H[channel]);
            pa_xfree(u->base_profiles[channel]);
            u->base_profiles[channel] = pa_xstrdup(name);
        }else{
            return "incompatible size";
        }
        pa_datum_free(&value);
    }else{
        return "profile doesn't exist";
    }
    return NULL;
}

static void load_state(struct userdata *u) {
    unsigned a_i;
    float *H;
    pa_datum key, value;
    pa_database *database;
    char *dbname;
    pa_assert_se(dbname = pa_state_path(EQ_STATE_DB, false));
    database = pa_database_open(dbname, false);
    pa_xfree(dbname);
    if (!database) {
        pa_log("No resume state");
        return;
    }

    key.data = u->sink->name;
    key.size = strlen(key.data);

    if (pa_database_get(database, &key, &value) != NULL) {
        if (value.size > FILTER_STATE_SIZE(u) * sizeof(float) + sizeof(uint16_t)) {
            float *state = (float *) value.data;
            size_t n_profs;
            char **names;
            for(size_t c = 0; c < u->channels; ++c) {
                a_i = pa_aupdate_write_begin(u->a_H[c]);
                H = state + c * CHANNEL_PROFILE_SIZE(u) + 1;
                u->Xs[c][a_i] = state[c * CHANNEL_PROFILE_SIZE(u)];
                memcpy(u->Hs[c][a_i], H, FILTER_SIZE(u) * sizeof(float));
                pa_aupdate_write_end(u->a_H[c]);
            }
            unpack(((char *)value.data) + FILTER_STATE_SIZE(u) * sizeof(float), value.size - FILTER_STATE_SIZE(u) * sizeof(float), &names, &n_profs);
            n_profs = PA_MIN(n_profs, u->channels);
            for(size_t c = 0; c < n_profs; ++c) {
                pa_xfree(u->base_profiles[c]);
                u->base_profiles[c] = names[c];
            }
            pa_xfree(names);
        }
        pa_datum_free(&value);
    }else{
        pa_log("resume state exists but is wrong size!");
    }
    pa_database_close(database);
}

/* Called from main context */
static bool sink_input_may_move_to_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    return u->sink != dest;
}

/* Called from main context */
static void sink_input_moving_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->autoloaded) {
        /* We were autoloaded, and don't support moving. Let's unload ourselves. */
        pa_log_debug("Can't move autoloaded stream, unloading");
        pa_module_unload_request(u->module, true);
    }

    if (dest) {
        pa_sink_set_asyncmsgq(u->sink, dest->asyncmsgq);
        pa_sink_update_flags(u->sink, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY, dest->flags);
    } else
        pa_sink_set_asyncmsgq(u->sink, NULL);
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma;
    const char *z;
    pa_sink *master;
    pa_sink_input_new_data sink_input_data;
    pa_sink_new_data sink_data;
    size_t i;
    unsigned c;
    float *H;
    unsigned a_i;
    bool use_volume_sharing = true;

    pa_assert(m);

    pa_log_warn("module-equalizer-sink is currently unsupported, and can sometimes cause "
                "PulseAudio crashes, increased latency or audible artifacts.");
    pa_log_warn("If you're facing audio problems, try unloading this module as a potential workaround.");

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "sink_master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }

    ss = master->sample_spec;
    ss.format = PA_SAMPLE_FLOAT32;
    map = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    //fs = pa_frame_size(&ss);

    if (pa_modargs_get_value_boolean(ma, "use_volume_sharing", &use_volume_sharing) < 0) {
        pa_log("use_volume_sharing= expects a boolean argument");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;

    u->channels = ss.channels;
    u->fft_size = pow(2, ceil(log(ss.rate) / log(2)));//probably unstable near corner cases of powers of 2
    pa_log_debug("fft size: %zd", u->fft_size);
    u->window_size = 15999;
    if (u->window_size % 2 == 0)
        u->window_size--;
    u->R = (u->window_size + 1) / 2;
    u->overlap_size = u->window_size - u->R;
    u->samples_gathered = 0;
    u->input_buffer_max = 0;

    u->a_H = pa_xnew0(pa_aupdate *, u->channels);
    u->Xs = pa_xnew0(float *, u->channels);
    u->Hs = pa_xnew0(float **, u->channels);

    for (c = 0; c < u->channels; ++c) {
        u->Xs[c] = pa_xnew0(float, 2);
        u->Hs[c] = pa_xnew0(float *, 2);
        for (i = 0; i < 2; ++i)
            u->Hs[c][i] = alloc(FILTER_SIZE(u), sizeof(float));
    }

    u->W = alloc(u->window_size, sizeof(float));
    u->work_buffer = alloc(u->fft_size, sizeof(float));
    u->input = pa_xnew0(float *, u->channels);
    u->overlap_accum = pa_xnew0(float *, u->channels);
    for (c = 0; c < u->channels; ++c) {
        u->a_H[c] = pa_aupdate_new();
        u->input[c] = NULL;
        u->overlap_accum[c] = alloc(u->overlap_size, sizeof(float));
    }
    u->output_window = alloc(FILTER_SIZE(u), sizeof(fftwf_complex));
    u->forward_plan = fftwf_plan_dft_r2c_1d(u->fft_size, u->work_buffer, u->output_window, FFTW_ESTIMATE);
    u->inverse_plan = fftwf_plan_dft_c2r_1d(u->fft_size, u->output_window, u->work_buffer, FFTW_ESTIMATE);

    hanning_window(u->W, u->window_size);
    u->first_iteration = true;

    u->base_profiles = pa_xnew0(char *, u->channels);
    for (c = 0; c < u->channels; ++c)
        u->base_profiles[c] = pa_xstrdup("default");

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if (!(sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sink_data.name = pa_sprintf_malloc("%s.equalizer", master->name);
    pa_sink_new_data_set_sample_spec(&sink_data, &ss);
    pa_sink_new_data_set_channel_map(&sink_data, &map);

    z = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
    pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "FFT based equalizer on %s", z ? z : master->name);

    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "filter");

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    u->autoloaded = DEFAULT_AUTOLOADED;
    if (pa_modargs_get_value_boolean(ma, "autoloaded", &u->autoloaded) < 0) {
        pa_log("Failed to parse autoloaded value");
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &sink_data, (master->flags & (PA_SINK_LATENCY | PA_SINK_DYNAMIC_LATENCY))
                                               | (use_volume_sharing ? PA_SINK_SHARE_VOLUME_WITH_MASTER : 0));
    pa_sink_new_data_done(&sink_data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg_cb;
    u->sink->set_state = sink_set_state_cb;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    u->sink->request_rewind = sink_request_rewind_cb;
    pa_sink_set_set_mute_callback(u->sink, sink_set_mute_cb);
    if (!use_volume_sharing) {
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        pa_sink_enable_decibel_volume(u->sink, true);
    }
    u->sink->userdata = u;

    u->input_q = pa_memblockq_new("module-equalizer-sink input_q", 0, MEMBLOCKQ_MAXLENGTH, 0, &ss, 1, 1, 0, &u->sink->silence);
    u->output_q = pa_memblockq_new("module-equalizer-sink output_q", 0, MEMBLOCKQ_MAXLENGTH, 0, &ss, 1, 1, 0, NULL);
    u->output_buffer = NULL;
    u->output_buffer_length = 0;
    u->output_buffer_max_length = 0;

    pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);
    //pa_sink_set_fixed_latency(u->sink, pa_bytes_to_usec(u->R*fs, &ss));

    /* Create sink input */
    pa_sink_input_new_data_init(&sink_input_data);
    sink_input_data.driver = __FILE__;
    sink_input_data.module = m;
    pa_sink_input_new_data_set_sink(&sink_input_data, master, false);
    sink_input_data.origin_sink = u->sink;
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_NAME, "Equalized Stream");
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sink_input_data, &ss);
    pa_sink_input_new_data_set_channel_map(&sink_input_data, &map);

    pa_sink_input_new(&u->sink_input, m->core, &sink_input_data);
    pa_sink_input_new_data_done(&sink_input_data);

    if (!u->sink_input)
        goto fail;

    u->sink_input->pop = sink_input_pop_cb;
    u->sink_input->process_rewind = sink_input_process_rewind_cb;
    u->sink_input->update_max_rewind = sink_input_update_max_rewind_cb;
    u->sink_input->update_max_request = sink_input_update_max_request_cb;
    u->sink_input->update_sink_latency_range = sink_input_update_sink_latency_range_cb;
    u->sink_input->update_sink_fixed_latency = sink_input_update_sink_fixed_latency_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->attach = sink_input_attach_cb;
    u->sink_input->detach = sink_input_detach_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->may_move_to = sink_input_may_move_to_cb;
    u->sink_input->moving = sink_input_moving_cb;
    if (!use_volume_sharing)
        u->sink_input->volume_changed = sink_input_volume_changed_cb;
    u->sink_input->mute_changed = sink_input_mute_changed_cb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;

    dbus_init(u);

    /* default filter to these */
    for (c = 0; c< u->channels; ++c) {
        a_i = pa_aupdate_write_begin(u->a_H[c]);
        H = u->Hs[c][a_i];
        u->Xs[c][a_i] = 1.0f;

        for(i = 0; i < FILTER_SIZE(u); ++i)
            H[i] = 1.0 / sqrtf(2.0f);

        fix_filter(H, u->fft_size);
        pa_aupdate_write_end(u->a_H[c]);
    }

    /* load old parameters */
    load_state(u);

    pa_sink_put(u->sink);
    pa_sink_input_put(u->sink_input);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink);
}

void pa__done(pa_module*m) {
    struct userdata *u;
    unsigned c;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    save_state(u);

    dbus_done(u);

    for(c = 0; c < u->channels; ++c)
        pa_xfree(u->base_profiles[c]);
    pa_xfree(u->base_profiles);

    /* See comments in sink_input_kill_cb() above regarding
     * destruction order! */

    if (u->sink_input)
        pa_sink_input_unlink(u->sink_input);

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->sink_input)
        pa_sink_input_unref(u->sink_input);

    if (u->sink)
        pa_sink_unref(u->sink);

    pa_xfree(u->output_buffer);
    pa_memblockq_free(u->output_q);
    pa_memblockq_free(u->input_q);

    fftwf_destroy_plan(u->inverse_plan);
    fftwf_destroy_plan(u->forward_plan);
    fftwf_free(u->output_window);
    for (c = 0; c < u->channels; ++c) {
        pa_aupdate_free(u->a_H[c]);
        fftwf_free(u->overlap_accum[c]);
        fftwf_free(u->input[c]);
    }
    pa_xfree(u->a_H);
    pa_xfree(u->overlap_accum);
    pa_xfree(u->input);
    fftwf_free(u->work_buffer);
    fftwf_free(u->W);
    for (c = 0; c < u->channels; ++c) {
        pa_xfree(u->Xs[c]);
        for (size_t i = 0; i < 2; ++i)
            fftwf_free(u->Hs[c][i]);
        fftwf_free(u->Hs[c]);
    }
    pa_xfree(u->Xs);
    pa_xfree(u->Hs);

    pa_xfree(u);
}

/*
 * DBus Routines and Callbacks
 */
#define EXTNAME "org.PulseAudio.Ext.Equalizing1"
#define MANAGER_PATH "/org/pulseaudio/equalizing1"
#define MANAGER_IFACE EXTNAME ".Manager"
#define EQUALIZER_IFACE EXTNAME ".Equalizer"
static void manager_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u);
static void manager_get_sinks(DBusConnection *conn, DBusMessage *msg, void *_u);
static void manager_get_profiles(DBusConnection *conn, DBusMessage *msg, void *_u);
static void manager_get_all(DBusConnection *conn, DBusMessage *msg, void *_u);
static void manager_handle_remove_profile(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_filter_rate(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_n_coefs(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_n_channels(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_get_all(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_seed_filter(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_get_filter_points(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_get_filter(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_set_filter(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_save_profile(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_load_profile(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_save_state(DBusConnection *conn, DBusMessage *msg, void *_u);
static void equalizer_handle_get_profile_name(DBusConnection *conn, DBusMessage *msg, void *_u);
enum manager_method_index {
    MANAGER_METHOD_REMOVE_PROFILE,
    MANAGER_METHOD_MAX
};

pa_dbus_arg_info remove_profile_args[]={
    {"name", "s","in"},
};

static pa_dbus_method_handler manager_methods[MANAGER_METHOD_MAX]={
    [MANAGER_METHOD_REMOVE_PROFILE]={
        .method_name="RemoveProfile",
        .arguments=remove_profile_args,
        .n_arguments=sizeof(remove_profile_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=manager_handle_remove_profile}
};

enum manager_handler_index {
    MANAGER_HANDLER_REVISION,
    MANAGER_HANDLER_EQUALIZED_SINKS,
    MANAGER_HANDLER_PROFILES,
    MANAGER_HANDLER_MAX
};

static pa_dbus_property_handler manager_handlers[MANAGER_HANDLER_MAX]={
    [MANAGER_HANDLER_REVISION]={.property_name="InterfaceRevision",.type="u",.get_cb=manager_get_revision,.set_cb=NULL},
    [MANAGER_HANDLER_EQUALIZED_SINKS]={.property_name="EqualizedSinks",.type="ao",.get_cb=manager_get_sinks,.set_cb=NULL},
    [MANAGER_HANDLER_PROFILES]={.property_name="Profiles",.type="as",.get_cb=manager_get_profiles,.set_cb=NULL}
};

pa_dbus_arg_info sink_args[]={
    {"sink", "o", NULL}
};

enum manager_signal_index{
    MANAGER_SIGNAL_SINK_ADDED,
    MANAGER_SIGNAL_SINK_REMOVED,
    MANAGER_SIGNAL_PROFILES_CHANGED,
    MANAGER_SIGNAL_MAX
};

static pa_dbus_signal_info manager_signals[MANAGER_SIGNAL_MAX]={
    [MANAGER_SIGNAL_SINK_ADDED]={.name="SinkAdded", .arguments=sink_args, .n_arguments=sizeof(sink_args)/sizeof(pa_dbus_arg_info)},
    [MANAGER_SIGNAL_SINK_REMOVED]={.name="SinkRemoved", .arguments=sink_args, .n_arguments=sizeof(sink_args)/sizeof(pa_dbus_arg_info)},
    [MANAGER_SIGNAL_PROFILES_CHANGED]={.name="ProfilesChanged", .arguments=NULL, .n_arguments=0}
};

static pa_dbus_interface_info manager_info={
    .name=MANAGER_IFACE,
    .method_handlers=manager_methods,
    .n_method_handlers=MANAGER_METHOD_MAX,
    .property_handlers=manager_handlers,
    .n_property_handlers=MANAGER_HANDLER_MAX,
    .get_all_properties_cb=manager_get_all,
    .signals=manager_signals,
    .n_signals=MANAGER_SIGNAL_MAX
};

enum equalizer_method_index {
    EQUALIZER_METHOD_FILTER_POINTS,
    EQUALIZER_METHOD_SEED_FILTER,
    EQUALIZER_METHOD_SAVE_PROFILE,
    EQUALIZER_METHOD_LOAD_PROFILE,
    EQUALIZER_METHOD_SET_FILTER,
    EQUALIZER_METHOD_GET_FILTER,
    EQUALIZER_METHOD_SAVE_STATE,
    EQUALIZER_METHOD_GET_PROFILE_NAME,
    EQUALIZER_METHOD_MAX
};

enum equalizer_handler_index {
    EQUALIZER_HANDLER_REVISION,
    EQUALIZER_HANDLER_SAMPLERATE,
    EQUALIZER_HANDLER_FILTERSAMPLERATE,
    EQUALIZER_HANDLER_N_COEFS,
    EQUALIZER_HANDLER_N_CHANNELS,
    EQUALIZER_HANDLER_MAX
};

pa_dbus_arg_info filter_points_args[]={
    {"channel", "u","in"},
    {"xs", "au","in"},
    {"ys", "ad","out"},
    {"preamp", "d","out"}
};
pa_dbus_arg_info seed_filter_args[]={
    {"channel", "u","in"},
    {"xs", "au","in"},
    {"ys", "ad","in"},
    {"preamp", "d","in"}
};

pa_dbus_arg_info set_filter_args[]={
    {"channel", "u","in"},
    {"ys", "ad","in"},
    {"preamp", "d","in"}
};
pa_dbus_arg_info get_filter_args[]={
    {"channel", "u","in"},
    {"ys", "ad","out"},
    {"preamp", "d","out"}
};

pa_dbus_arg_info save_profile_args[]={
    {"channel", "u","in"},
    {"name", "s","in"}
};
pa_dbus_arg_info load_profile_args[]={
    {"channel", "u","in"},
    {"name", "s","in"}
};
pa_dbus_arg_info base_profile_name_args[]={
    {"channel", "u","in"},
    {"name", "s","out"}
};

static pa_dbus_method_handler equalizer_methods[EQUALIZER_METHOD_MAX]={
    [EQUALIZER_METHOD_SEED_FILTER]={
        .method_name="SeedFilter",
        .arguments=seed_filter_args,
        .n_arguments=sizeof(seed_filter_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_seed_filter},
    [EQUALIZER_METHOD_FILTER_POINTS]={
        .method_name="FilterAtPoints",
        .arguments=filter_points_args,
        .n_arguments=sizeof(filter_points_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_get_filter_points},
    [EQUALIZER_METHOD_SET_FILTER]={
        .method_name="SetFilter",
        .arguments=set_filter_args,
        .n_arguments=sizeof(set_filter_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_set_filter},
    [EQUALIZER_METHOD_GET_FILTER]={
        .method_name="GetFilter",
        .arguments=get_filter_args,
        .n_arguments=sizeof(get_filter_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_get_filter},
    [EQUALIZER_METHOD_SAVE_PROFILE]={
        .method_name="SaveProfile",
        .arguments=save_profile_args,
        .n_arguments=sizeof(save_profile_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_save_profile},
    [EQUALIZER_METHOD_LOAD_PROFILE]={
        .method_name="LoadProfile",
        .arguments=load_profile_args,
        .n_arguments=sizeof(load_profile_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_load_profile},
    [EQUALIZER_METHOD_SAVE_STATE]={
        .method_name="SaveState",
        .arguments=NULL,
        .n_arguments=0,
        .receive_cb=equalizer_handle_save_state},
    [EQUALIZER_METHOD_GET_PROFILE_NAME]={
        .method_name="BaseProfile",
        .arguments=base_profile_name_args,
        .n_arguments=sizeof(base_profile_name_args)/sizeof(pa_dbus_arg_info),
        .receive_cb=equalizer_handle_get_profile_name}
};

static pa_dbus_property_handler equalizer_handlers[EQUALIZER_HANDLER_MAX]={
    [EQUALIZER_HANDLER_REVISION]={.property_name="InterfaceRevision",.type="u",.get_cb=equalizer_get_revision,.set_cb=NULL},
    [EQUALIZER_HANDLER_SAMPLERATE]={.property_name="SampleRate",.type="u",.get_cb=equalizer_get_sample_rate,.set_cb=NULL},
    [EQUALIZER_HANDLER_FILTERSAMPLERATE]={.property_name="FilterSampleRate",.type="u",.get_cb=equalizer_get_filter_rate,.set_cb=NULL},
    [EQUALIZER_HANDLER_N_COEFS]={.property_name="NFilterCoefficients",.type="u",.get_cb=equalizer_get_n_coefs,.set_cb=NULL},
    [EQUALIZER_HANDLER_N_CHANNELS]={.property_name="NChannels",.type="u",.get_cb=equalizer_get_n_channels,.set_cb=NULL},
};

enum equalizer_signal_index{
    EQUALIZER_SIGNAL_FILTER_CHANGED,
    EQUALIZER_SIGNAL_SINK_RECONFIGURED,
    EQUALIZER_SIGNAL_MAX
};

static pa_dbus_signal_info equalizer_signals[EQUALIZER_SIGNAL_MAX]={
    [EQUALIZER_SIGNAL_FILTER_CHANGED]={.name="FilterChanged", .arguments=NULL, .n_arguments=0},
    [EQUALIZER_SIGNAL_SINK_RECONFIGURED]={.name="SinkReconfigured", .arguments=NULL, .n_arguments=0},
};

static pa_dbus_interface_info equalizer_info={
    .name=EQUALIZER_IFACE,
    .method_handlers=equalizer_methods,
    .n_method_handlers=EQUALIZER_METHOD_MAX,
    .property_handlers=equalizer_handlers,
    .n_property_handlers=EQUALIZER_HANDLER_MAX,
    .get_all_properties_cb=equalizer_get_all,
    .signals=equalizer_signals,
    .n_signals=EQUALIZER_SIGNAL_MAX
};

void dbus_init(struct userdata *u) {
    uint32_t dummy;
    DBusMessage *message = NULL;
    pa_idxset *sink_list = NULL;
    u->dbus_protocol=pa_dbus_protocol_get(u->sink->core);
    u->dbus_path=pa_sprintf_malloc("/org/pulseaudio/core1/sink%d", u->sink->index);

    pa_assert_se(pa_dbus_protocol_add_interface(u->dbus_protocol, u->dbus_path, &equalizer_info, u) >= 0);
    sink_list = pa_shared_get(u->sink->core, SINKLIST);
    u->database = pa_shared_get(u->sink->core, EQDB);
    if (sink_list == NULL) {
        char *dbname;
        sink_list=pa_idxset_new(&pa_idxset_trivial_hash_func, &pa_idxset_trivial_compare_func);
        pa_shared_set(u->sink->core, SINKLIST, sink_list);
        pa_assert_se(dbname = pa_state_path("equalizer-presets", false));
        pa_assert_se(u->database = pa_database_open(dbname, true));
        pa_xfree(dbname);
        pa_shared_set(u->sink->core, EQDB, u->database);
        pa_dbus_protocol_add_interface(u->dbus_protocol, MANAGER_PATH, &manager_info, u->sink->core);
        pa_dbus_protocol_register_extension(u->dbus_protocol, EXTNAME);
    }
    pa_idxset_put(sink_list, u, &dummy);

    pa_assert_se((message = dbus_message_new_signal(MANAGER_PATH, MANAGER_IFACE, manager_signals[MANAGER_SIGNAL_SINK_ADDED].name)));
    dbus_message_append_args(message, DBUS_TYPE_OBJECT_PATH, &u->dbus_path, DBUS_TYPE_INVALID);
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);
}

void dbus_done(struct userdata *u) {
    pa_idxset *sink_list;
    uint32_t dummy;

    DBusMessage *message = NULL;
    pa_assert_se((message = dbus_message_new_signal(MANAGER_PATH, MANAGER_IFACE, manager_signals[MANAGER_SIGNAL_SINK_REMOVED].name)));
    dbus_message_append_args(message, DBUS_TYPE_OBJECT_PATH, &u->dbus_path, DBUS_TYPE_INVALID);
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);

    pa_assert_se(sink_list=pa_shared_get(u->sink->core,SINKLIST));
    pa_idxset_remove_by_data(sink_list,u,&dummy);
    if (pa_idxset_size(sink_list) == 0) {
        pa_dbus_protocol_unregister_extension(u->dbus_protocol, EXTNAME);
        pa_dbus_protocol_remove_interface(u->dbus_protocol, MANAGER_PATH, manager_info.name);
        pa_shared_remove(u->sink->core, EQDB);
        pa_database_close(u->database);
        pa_shared_remove(u->sink->core, SINKLIST);
        pa_xfree(sink_list);
    }
    pa_dbus_protocol_remove_interface(u->dbus_protocol, u->dbus_path, equalizer_info.name);
    pa_xfree(u->dbus_path);
    pa_dbus_protocol_unref(u->dbus_protocol);
}

void manager_handle_remove_profile(DBusConnection *conn, DBusMessage *msg, void *_u) {
    DBusError error;
    pa_core *c = (pa_core *)_u;
    DBusMessage *message = NULL;
    pa_dbus_protocol *dbus_protocol;
    char *name;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert(c);
    dbus_error_init(&error);
    if (!dbus_message_get_args(msg, &error,
                 DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    remove_profile(c,name);
    pa_dbus_send_empty_reply(conn, msg);

    pa_assert_se((message = dbus_message_new_signal(MANAGER_PATH, MANAGER_IFACE, manager_signals[MANAGER_SIGNAL_PROFILES_CHANGED].name)));
    dbus_protocol = pa_dbus_protocol_get(c);
    pa_dbus_protocol_send_signal(dbus_protocol, message);
    pa_dbus_protocol_unref(dbus_protocol);
    dbus_message_unref(message);
}

void manager_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u) {
    uint32_t rev=1;
    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_UINT32, &rev);
}

static void get_sinks(pa_core *u, char ***names, unsigned *n_sinks) {
    void *iter = NULL;
    struct userdata *sink_u = NULL;
    uint32_t dummy;
    pa_idxset *sink_list;
    pa_assert(u);
    pa_assert(names);
    pa_assert(n_sinks);

    pa_assert_se(sink_list = pa_shared_get(u, SINKLIST));
    *n_sinks = (unsigned) pa_idxset_size(sink_list);
    *names = *n_sinks > 0 ? pa_xnew0(char *,*n_sinks) : NULL;
    for(uint32_t i = 0; i < *n_sinks; ++i) {
        sink_u = (struct userdata *) pa_idxset_iterate(sink_list, &iter, &dummy);
        (*names)[i] = pa_xstrdup(sink_u->dbus_path);
    }
}

void manager_get_sinks(DBusConnection *conn, DBusMessage *msg, void *_u) {
    unsigned n;
    char **names = NULL;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert(_u);

    get_sinks((pa_core *) _u, &names, &n);
    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, names, n);
    for(unsigned i = 0; i < n; ++i) {
        pa_xfree(names[i]);
    }
    pa_xfree(names);
}

static void get_profiles(pa_core *c, char ***names, unsigned *n) {
    char *name;
    pa_database *database;
    pa_datum key, next_key;
    pa_strlist *head=NULL, *iter;
    bool done;
    pa_assert_se(database = pa_shared_get(c, EQDB));

    pa_assert(c);
    pa_assert(names);
    pa_assert(n);
    done = !pa_database_first(database, &key, NULL);
    *n = 0;
    while(!done) {
        done = !pa_database_next(database, &key, &next_key, NULL);
        name=pa_xmalloc(key.size + 1);
        memcpy(name, key.data, key.size);
        name[key.size] = '\0';
        pa_datum_free(&key);
        head = pa_strlist_prepend(head, name);
        pa_xfree(name);
        key = next_key;
        (*n)++;
    }
    (*names) = *n > 0 ? pa_xnew0(char *, *n) : NULL;
    iter=head;
    for(unsigned i = 0; i < *n; ++i) {
        (*names)[*n - 1 - i] = pa_xstrdup(pa_strlist_data(iter));
        iter = pa_strlist_next(iter);
    }
    pa_strlist_free(head);
}

void manager_get_profiles(DBusConnection *conn, DBusMessage *msg, void *_u) {
    char **names;
    unsigned n;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert(_u);

    get_profiles((pa_core *)_u, &names, &n);
    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_STRING, names, n);
    for(unsigned i = 0; i < n; ++i) {
        pa_xfree(names[i]);
    }
    pa_xfree(names);
}

void manager_get_all(DBusConnection *conn, DBusMessage *msg, void *_u) {
    pa_core *c;
    char **names = NULL;
    unsigned n;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter, dict_iter;
    uint32_t rev;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert_se(c = _u);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    rev = 1;
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, manager_handlers[MANAGER_HANDLER_REVISION].property_name, DBUS_TYPE_UINT32, &rev);

    get_sinks(c, &names, &n);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter,manager_handlers[MANAGER_HANDLER_EQUALIZED_SINKS].property_name, DBUS_TYPE_OBJECT_PATH, names, n);
    for(unsigned i = 0; i < n; ++i) {
        pa_xfree(names[i]);
    }
    pa_xfree(names);

    get_profiles(c, &names, &n);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, manager_handlers[MANAGER_HANDLER_PROFILES].property_name, DBUS_TYPE_STRING, names, n);
    for(unsigned i = 0; i < n; ++i) {
        pa_xfree(names[i]);
    }
    pa_xfree(names);
    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

void equalizer_handle_seed_filter(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = _u;
    DBusError error;
    DBusMessage *message = NULL;
    float *ys;
    uint32_t *xs, channel, r_channel;
    double *_ys, preamp;
    unsigned x_npoints, y_npoints, a_i;
    float *H;
    bool points_good = true;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &xs, &x_npoints,
                DBUS_TYPE_ARRAY, DBUS_TYPE_DOUBLE, &_ys, &y_npoints,
                DBUS_TYPE_DOUBLE, &preamp,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }
    for(size_t i = 0; i < x_npoints; ++i) {
        if (xs[i] >= FILTER_SIZE(u)) {
            points_good = false;
            break;
        }
    }
    if (!is_monotonic(xs, x_npoints) || !points_good) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "xs must be monotonic and 0<=x<=%zd", u->fft_size / 2);
        dbus_error_free(&error);
        return;
    }else if (x_npoints != y_npoints || x_npoints < 2 || x_npoints > FILTER_SIZE(u)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "xs and ys must be the same length and 2<=l<=%zd!", FILTER_SIZE(u));
        dbus_error_free(&error);
        return;
    }else if (xs[0] != 0 || xs[x_npoints - 1] != u->fft_size / 2) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "xs[0] must be 0 and xs[-1]=fft_size/2");
        dbus_error_free(&error);
        return;
    }

    ys = pa_xmalloc(x_npoints * sizeof(float));
    for(uint32_t i = 0; i < x_npoints; ++i) {
        ys[i] = (float) _ys[i];
    }
    r_channel = channel == u->channels ? 0 : channel;
    a_i = pa_aupdate_write_begin(u->a_H[r_channel]);
    H = u->Hs[r_channel][a_i];
    u->Xs[r_channel][a_i] = preamp;
    interpolate(H, FILTER_SIZE(u), xs, ys, x_npoints);
    fix_filter(H, u->fft_size);
    if (channel == u->channels) {
        for(size_t c = 1; c < u->channels; ++c) {
            unsigned b_i = pa_aupdate_write_begin(u->a_H[c]);
            float *H_p = u->Hs[c][b_i];
            u->Xs[c][b_i] = preamp;
            memcpy(H_p, H, FILTER_SIZE(u) * sizeof(float));
            pa_aupdate_write_end(u->a_H[c]);
        }
    }
    pa_aupdate_write_end(u->a_H[r_channel]);
    pa_xfree(ys);

    pa_dbus_send_empty_reply(conn, msg);

    pa_assert_se((message = dbus_message_new_signal(u->dbus_path, EQUALIZER_IFACE, equalizer_signals[EQUALIZER_SIGNAL_FILTER_CHANGED].name)));
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);
}

void equalizer_handle_get_filter_points(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = (struct userdata *) _u;
    uint32_t *xs, channel, r_channel;
    double *ys, preamp;
    unsigned x_npoints, a_i;
    float *H;
    bool points_good=true;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusError error;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    dbus_error_init(&error);
    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32, &xs, &x_npoints,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }

    for(size_t i = 0; i < x_npoints; ++i) {
        if (xs[i] >= FILTER_SIZE(u)) {
            points_good=false;
            break;
        }
    }

    if (x_npoints > FILTER_SIZE(u) || !points_good) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "xs indices/length must be <= %zd!", FILTER_SIZE(u));
        dbus_error_free(&error);
        return;
    }

    r_channel = channel == u->channels ? 0 : channel;
    ys = pa_xmalloc(x_npoints * sizeof(double));
    a_i = pa_aupdate_read_begin(u->a_H[r_channel]);
    H = u->Hs[r_channel][a_i];
    preamp = u->Xs[r_channel][a_i];
    for(uint32_t i = 0; i < x_npoints; ++i) {
        ys[i] = H[xs[i]] * u->fft_size;
    }
    pa_aupdate_read_end(u->a_H[r_channel]);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);

    pa_dbus_append_basic_array(&msg_iter, DBUS_TYPE_DOUBLE, ys, x_npoints);
    pa_dbus_append_basic_variant(&msg_iter, DBUS_TYPE_DOUBLE, &preamp);

    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
    pa_xfree(ys);
}

static void get_filter(struct userdata *u, size_t channel, double **H_, double *preamp) {
    float *H;
    unsigned a_i;
    size_t r_channel = channel == u->channels ? 0 : channel;
    *H_ = pa_xnew0(double, FILTER_SIZE(u));
    a_i = pa_aupdate_read_begin(u->a_H[r_channel]);
    H = u->Hs[r_channel][a_i];
    for(size_t i = 0;i < FILTER_SIZE(u); ++i) {
        (*H_)[i] = H[i] * u->fft_size;
    }
    *preamp = u->Xs[r_channel][a_i];

    pa_aupdate_read_end(u->a_H[r_channel]);
}

void equalizer_handle_get_filter(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    unsigned n_coefs;
    uint32_t channel;
    double *H_, preamp;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusError error;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    dbus_error_init(&error);
    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }

    n_coefs = CHANNEL_PROFILE_SIZE(u);
    pa_assert(conn);
    pa_assert(msg);
    get_filter(u, channel, &H_, &preamp);
    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);

    pa_dbus_append_basic_array(&msg_iter, DBUS_TYPE_DOUBLE, H_, n_coefs);
    pa_dbus_append_basic_variant(&msg_iter, DBUS_TYPE_DOUBLE, &preamp);

    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
    pa_xfree(H_);
}

static void set_filter(struct userdata *u, size_t channel, double *H_, double preamp) {
    unsigned a_i;
    size_t r_channel = channel == u->channels ? 0 : channel;
    float *H;
    //all channels
    a_i = pa_aupdate_write_begin(u->a_H[r_channel]);
    u->Xs[r_channel][a_i] = (float) preamp;
    H = u->Hs[r_channel][a_i];
    for(size_t i = 0; i < FILTER_SIZE(u); ++i) {
        H[i] = (float) H_[i];
    }
    fix_filter(H, u->fft_size);
    if (channel == u->channels) {
        for(size_t c = 1; c < u->channels; ++c) {
            unsigned b_i = pa_aupdate_write_begin(u->a_H[c]);
            u->Xs[c][b_i] = u->Xs[r_channel][a_i];
            memcpy(u->Hs[c][b_i], u->Hs[r_channel][a_i], FILTER_SIZE(u) * sizeof(float));
            pa_aupdate_write_end(u->a_H[c]);
        }
    }
    pa_aupdate_write_end(u->a_H[r_channel]);
}

void equalizer_handle_set_filter(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    double *H, preamp;
    uint32_t channel;
    unsigned _n_coefs;
    DBusMessage *message = NULL;
    DBusError error;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    dbus_error_init(&error);
    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_ARRAY, DBUS_TYPE_DOUBLE, &H, &_n_coefs,
                DBUS_TYPE_DOUBLE, &preamp,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }
    if (_n_coefs != FILTER_SIZE(u)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "This filter takes exactly %zd coefficients, you gave %d", FILTER_SIZE(u), _n_coefs);
        return;
    }
    set_filter(u, channel, H, preamp);

    pa_dbus_send_empty_reply(conn, msg);

    pa_assert_se((message = dbus_message_new_signal(u->dbus_path, EQUALIZER_IFACE, equalizer_signals[EQUALIZER_SIGNAL_FILTER_CHANGED].name)));
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);
}

void equalizer_handle_save_profile(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = (struct userdata *) _u;
    char *name;
    uint32_t channel, r_channel;
    DBusMessage *message = NULL;
    DBusError error;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);
    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }
    r_channel = channel == u->channels ? 0 : channel;
    save_profile(u, r_channel, name);
    pa_dbus_send_empty_reply(conn, msg);

    pa_assert_se((message = dbus_message_new_signal(MANAGER_PATH, MANAGER_IFACE, manager_signals[MANAGER_SIGNAL_PROFILES_CHANGED].name)));
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);
}

void equalizer_handle_load_profile(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = (struct userdata *) _u;
    char *name;
    DBusError error;
    uint32_t channel, r_channel;
    const char *err_msg = NULL;
    DBusMessage *message = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);
    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }
    r_channel = channel == u->channels ? 0 : channel;

    err_msg = load_profile(u, r_channel, name);
    if (err_msg != NULL) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "error loading profile %s: %s", name, err_msg);
        dbus_error_free(&error);
        return;
    }
    if (channel == u->channels) {
        for(uint32_t c = 1; c < u->channels; ++c) {
            load_profile(u, c, name);
        }
    }
    pa_dbus_send_empty_reply(conn, msg);

    pa_assert_se((message = dbus_message_new_signal(u->dbus_path, EQUALIZER_IFACE, equalizer_signals[EQUALIZER_SIGNAL_FILTER_CHANGED].name)));
    pa_dbus_protocol_send_signal(u->dbus_protocol, message);
    dbus_message_unref(message);
}

void equalizer_handle_save_state(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = (struct userdata *) _u;
    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    save_state(u);
    pa_dbus_send_empty_reply(conn, msg);
}

void equalizer_handle_get_profile_name(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u = (struct userdata *) _u;
    DBusError error;
    uint32_t channel, r_channel;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);
    dbus_error_init(&error);

    if (!dbus_message_get_args(msg, &error,
                DBUS_TYPE_UINT32, &channel,
                DBUS_TYPE_INVALID)) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "%s", error.message);
        dbus_error_free(&error);
        return;
    }
    if (channel > u->channels) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "invalid channel: %d", channel);
        dbus_error_free(&error);
        return;
    }
    r_channel = channel == u->channels ? 0 : channel;
    pa_assert(u->base_profiles[r_channel]);
    pa_dbus_send_basic_value_reply(conn,msg, DBUS_TYPE_STRING, &u->base_profiles[r_channel]);
}

void equalizer_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u) {
    uint32_t rev=1;
    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_UINT32, &rev);
}

void equalizer_get_n_channels(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    uint32_t channels;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    channels = (uint32_t) u->channels;
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &channels);
}

void equalizer_get_n_coefs(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    uint32_t n_coefs;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    n_coefs = (uint32_t) CHANNEL_PROFILE_SIZE(u);
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &n_coefs);
}

void equalizer_get_sample_rate(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    uint32_t rate;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    rate = (uint32_t) u->sink->sample_spec.rate;
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &rate);
}

void equalizer_get_filter_rate(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    uint32_t fft_size;
    pa_assert_se(u = (struct userdata *) _u);
    pa_assert(conn);
    pa_assert(msg);

    fft_size = (uint32_t) u->fft_size;
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &fft_size);
}

void equalizer_get_all(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter, dict_iter;
    uint32_t rev, n_coefs, rate, fft_size, channels;

    pa_assert_se(u = _u);
    pa_assert(msg);

    rev = 1;
    n_coefs = (uint32_t) CHANNEL_PROFILE_SIZE(u);
    rate = (uint32_t) u->sink->sample_spec.rate;
    fft_size = (uint32_t) u->fft_size;
    channels = (uint32_t) u->channels;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, equalizer_handlers[EQUALIZER_HANDLER_REVISION].property_name, DBUS_TYPE_UINT32, &rev);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, equalizer_handlers[EQUALIZER_HANDLER_SAMPLERATE].property_name, DBUS_TYPE_UINT32, &rate);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, equalizer_handlers[EQUALIZER_HANDLER_FILTERSAMPLERATE].property_name, DBUS_TYPE_UINT32, &fft_size);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, equalizer_handlers[EQUALIZER_HANDLER_N_COEFS].property_name, DBUS_TYPE_UINT32, &n_coefs);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, equalizer_handlers[EQUALIZER_HANDLER_N_CHANNELS].property_name, DBUS_TYPE_UINT32, &channels);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}
