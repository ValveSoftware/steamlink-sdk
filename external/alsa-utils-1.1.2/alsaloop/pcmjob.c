/*
 *  A simple PCM loopback utility
 *  Copyright (c) 2010 by Jaroslav Kysela <perex@perex.cz>
 *
 *     Author: Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include <syslog.h>
#include <pthread.h>
#include "alsaloop.h"

#define XRUN_PROFILE_UNKNOWN (-10000000)

static int set_rate_shift(struct loopback_handle *lhandle, double pitch);
static int get_rate(struct loopback_handle *lhandle);

#define SYNCTYPE(v) [SYNC_TYPE_##v] = #v

static const char *sync_types[] = {
	SYNCTYPE(NONE),
	SYNCTYPE(SIMPLE),
	SYNCTYPE(CAPTRATESHIFT),
	SYNCTYPE(PLAYRATESHIFT),
	SYNCTYPE(SAMPLERATE),
	SYNCTYPE(AUTO)
};

#define SRCTYPE(v) [SRC_##v] = "SRC_" #v

#ifdef USE_SAMPLERATE
static const char *src_types[] = {
	SRCTYPE(SINC_BEST_QUALITY),
	SRCTYPE(SINC_MEDIUM_QUALITY),
	SRCTYPE(SINC_FASTEST),
	SRCTYPE(ZERO_ORDER_HOLD),
	SRCTYPE(LINEAR)
};
#endif

static pthread_once_t pcm_open_mutex_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t pcm_open_mutex;

static void pcm_open_init_mutex(void)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&pcm_open_mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}

static inline void pcm_open_lock(void)
{
	pthread_once(&pcm_open_mutex_once, pcm_open_init_mutex);
	if (workarounds & WORKAROUND_SERIALOPEN)
	        pthread_mutex_lock(&pcm_open_mutex);
}
 
static inline void pcm_open_unlock(void)
{
	if (workarounds & WORKAROUND_SERIALOPEN)
	        pthread_mutex_unlock(&pcm_open_mutex);
}

static inline snd_pcm_uframes_t get_whole_latency(struct loopback *loop)
{
	return loop->latency;
}

static inline unsigned long long
			frames_to_time(unsigned int rate,
				       snd_pcm_uframes_t frames)
{
	return (frames * 1000000ULL) / rate;
}

static inline snd_pcm_uframes_t time_to_frames(unsigned int rate,
					       unsigned long long time)
{
	return (time * rate) / 1000000ULL;
}

static int setparams_stream(struct loopback_handle *lhandle,
			    snd_pcm_hw_params_t *params)
{
	snd_pcm_t *handle = lhandle->handle;
	int err;
	unsigned int rrate;

	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		logit(LOG_CRIT, "Broken configuration for %s PCM: no configurations available: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_rate_resample(handle, params, lhandle->resample);
	if (err < 0) {
		logit(LOG_CRIT, "Resample setup failed for %s (val %i): %s\n", lhandle->id, lhandle->resample, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_access(handle, params, lhandle->access);
	if (err < 0) {
		logit(LOG_CRIT, "Access type not available for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_format(handle, params, lhandle->format);
	if (err < 0) {
		logit(LOG_CRIT, "Sample format not available for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_set_channels(handle, params, lhandle->channels);
	if (err < 0) {
		logit(LOG_CRIT, "Channels count (%i) not available for %s: %s\n", lhandle->channels, lhandle->id, snd_strerror(err));
		return err;
	}
	rrate = lhandle->rate_req;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		logit(LOG_CRIT, "Rate %iHz not available for %s: %s\n", lhandle->rate_req, lhandle->id, snd_strerror(err));
		return err;
	}
	rrate = 0;
	snd_pcm_hw_params_get_rate(params, &rrate, 0);
	lhandle->rate = rrate;
	if (
#ifdef USE_SAMPLERATE
	    !lhandle->loopback->src_enable &&
#endif
	    (int)rrate != lhandle->rate) {
		logit(LOG_CRIT, "Rate does not match (requested %iHz, got %iHz, resample %i)\n", lhandle->rate, rrate, lhandle->resample);
		return -EINVAL;
	}
	lhandle->pitch = (double)lhandle->rate_req / (double)lhandle->rate;
	return 0;
}

static int setparams_bufsize(struct loopback_handle *lhandle,
			     snd_pcm_hw_params_t *params,
			     snd_pcm_hw_params_t *tparams,
			     snd_pcm_uframes_t bufsize)
{
	snd_pcm_t *handle = lhandle->handle;
	int err;
	snd_pcm_uframes_t periodsize;
	snd_pcm_uframes_t buffersize;
	snd_pcm_uframes_t last_bufsize = 0;

	if (lhandle->buffer_size_req > 0) {
		bufsize = lhandle->buffer_size_req;
		last_bufsize = bufsize;
		goto __set_it;
	}
      __again:
	if (lhandle->buffer_size_req > 0) {
		logit(LOG_CRIT, "Unable to set buffer size %li for %s\n", (long)lhandle->buffer_size, lhandle->id);
		return -EIO;
	}
	if (last_bufsize == bufsize)
		bufsize += 4;
	last_bufsize = bufsize;
	if (bufsize > 10*1024*1024) {
		logit(LOG_CRIT, "Buffer size too big\n");
		return -EIO;
	}
      __set_it:
	snd_pcm_hw_params_copy(params, tparams);
	periodsize = bufsize * 8;
	err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set buffer size %li for %s: %s\n", periodsize, lhandle->id, snd_strerror(err));
		goto __again;
	}
	snd_pcm_hw_params_get_buffer_size(params, &periodsize);
	if (verbose > 6)
		snd_output_printf(lhandle->loopback->output, "%s: buffer_size=%li\n", lhandle->id, periodsize);
	if (lhandle->period_size_req > 0)
		periodsize = lhandle->period_size_req;
	else
		periodsize /= 8;
	err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set period size %li for %s: %s\n", periodsize, lhandle->id, snd_strerror(err));
		goto __again;
	}
	snd_pcm_hw_params_get_period_size(params, &periodsize, NULL);
	if (verbose > 6)
		snd_output_printf(lhandle->loopback->output, "%s: period_size=%li\n", lhandle->id, periodsize);
	if (periodsize != bufsize)
		bufsize = periodsize;
	snd_pcm_hw_params_get_buffer_size(params, &buffersize);
	if (periodsize * 2 > buffersize)
		goto __again;
	lhandle->period_size = periodsize;
	lhandle->buffer_size = buffersize;
	return 0;
}

static int setparams_set(struct loopback_handle *lhandle,
			 snd_pcm_hw_params_t *params,
			 snd_pcm_sw_params_t *swparams,
			 snd_pcm_uframes_t bufsize)
{
	snd_pcm_t *handle = lhandle->handle;
	int err;
	snd_pcm_uframes_t val, period_size, buffer_size;

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set hw params for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to determine current swparams for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0x7fffffff);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set start threshold mode for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if (lhandle->nblock) {
		if (lhandle == lhandle->loopback->play) {
			val = buffer_size - (2 * period_size - 4);
		} else {
			val = 4;
		}
		if (verbose > 6)
			snd_output_printf(lhandle->loopback->output, "%s: avail_min1=%li\n", lhandle->id, val);
	} else {
		if (lhandle == lhandle->loopback->play) {
			val = bufsize + bufsize / 2;
			if (val > (buffer_size * 3) / 4)
				val = (buffer_size * 3) / 4;
			val = buffer_size - val;
		} else {
			val = bufsize / 2;
			if (val > buffer_size / 4)
				val = buffer_size / 4;
		}
		if (verbose > 6)
			snd_output_printf(lhandle->loopback->output, "%s: avail_min2=%li\n", lhandle->id, val);
	}
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, val);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set avail min for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	snd_pcm_sw_params_get_avail_min(swparams, &lhandle->avail_min);
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		logit(LOG_CRIT, "Unable to set sw params for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return 0;
}

static int setparams(struct loopback *loop, snd_pcm_uframes_t bufsize)
{
	int err;
	snd_pcm_hw_params_t *pt_params, *ct_params;	/* templates with rate, format and channels */
	snd_pcm_hw_params_t *p_params, *c_params;
	snd_pcm_sw_params_t *p_swparams, *c_swparams;

	snd_pcm_hw_params_alloca(&p_params);
	snd_pcm_hw_params_alloca(&c_params);
	snd_pcm_hw_params_alloca(&pt_params);
	snd_pcm_hw_params_alloca(&ct_params);
	snd_pcm_sw_params_alloca(&p_swparams);
	snd_pcm_sw_params_alloca(&c_swparams);
	if ((err = setparams_stream(loop->play, pt_params)) < 0) {
		logit(LOG_CRIT, "Unable to set parameters for %s stream: %s\n", loop->play->id, snd_strerror(err));
		return err;
	}
	if ((err = setparams_stream(loop->capt, ct_params)) < 0) {
		logit(LOG_CRIT, "Unable to set parameters for %s stream: %s\n", loop->capt->id, snd_strerror(err));
		return err;
	}

	if ((err = setparams_bufsize(loop->play, p_params, pt_params, bufsize / loop->play->pitch)) < 0) {
		logit(LOG_CRIT, "Unable to set buffer parameters for %s stream: %s\n", loop->play->id, snd_strerror(err));
		return err;
	}
	if ((err = setparams_bufsize(loop->capt, c_params, ct_params, bufsize / loop->capt->pitch)) < 0) {
		logit(LOG_CRIT, "Unable to set buffer parameters for %s stream: %s\n", loop->capt->id, snd_strerror(err));
		return err;
	}

	if ((err = setparams_set(loop->play, p_params, p_swparams, bufsize / loop->play->pitch)) < 0) {
		logit(LOG_CRIT, "Unable to set sw parameters for %s stream: %s\n", loop->play->id, snd_strerror(err));
		return err;
	}
	if ((err = setparams_set(loop->capt, c_params, c_swparams, bufsize / loop->capt->pitch)) < 0) {
		logit(LOG_CRIT, "Unable to set sw parameters for %s stream: %s\n", loop->capt->id, snd_strerror(err));
		return err;
	}

#if 0
	if (!loop->linked)
		if (snd_pcm_link(loop->capt->handle, loop->play->handle) >= 0)
			loop->linked = 1;
#endif
	if ((err = snd_pcm_prepare(loop->play->handle)) < 0) {
		logit(LOG_CRIT, "Prepare %s error: %s\n", loop->play->id, snd_strerror(err));
		return err;
	}
	if (!loop->linked && (err = snd_pcm_prepare(loop->capt->handle)) < 0) {
		logit(LOG_CRIT, "Prepare %s error: %s\n", loop->capt->id, snd_strerror(err));
		return err;
	}

	if (verbose) {
		snd_pcm_dump(loop->play->handle, loop->output);
		snd_pcm_dump(loop->capt->handle, loop->output);
	}
	return 0;
}

static void showlatency(snd_output_t *out, size_t latency, unsigned int rate,
			char *prefix)
{
	double d;
	d = (double)latency / (double)rate;
	snd_output_printf(out, "%s %li frames, %.3fus, %.6fms (%.4fHz)\n", prefix, (long)latency, d * 1000000, d * 1000, (double)1 / d);
}

static long timediff(snd_timestamp_t t1, snd_timestamp_t t2)
{
	signed long l;

	t1.tv_sec -= t2.tv_sec;
	if (t1.tv_usec < t2.tv_usec) {
		l = ((t1.tv_usec + 1000000) - t2.tv_usec) % 1000000;
		t1.tv_sec--;
	} else {
		l = t1.tv_usec - t2.tv_usec;
	}
	return (t1.tv_sec * 1000000) + l;
}

static int getcurtimestamp(snd_timestamp_t *ts)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts->tv_sec = tv.tv_sec;
	ts->tv_usec = tv.tv_usec;
	return 0;
}

static void xrun_profile0(struct loopback *loop)
{
	snd_pcm_sframes_t pdelay, cdelay;

	if (snd_pcm_delay(loop->play->handle, &pdelay) >= 0 &&
	    snd_pcm_delay(loop->capt->handle, &cdelay) >= 0) {
		getcurtimestamp(&loop->xrun_last_update);
		loop->xrun_last_pdelay = pdelay;
		loop->xrun_last_cdelay = cdelay;
		loop->xrun_buf_pcount = loop->play->buf_count;
		loop->xrun_buf_ccount = loop->capt->buf_count;
#ifdef USE_SAMPLERATE
		loop->xrun_out_frames = loop->src_out_frames;
#endif
	}
}

static inline void xrun_profile(struct loopback *loop)
{
	if (loop->xrun)
		xrun_profile0(loop);
}

static void xrun_stats0(struct loopback *loop)
{
	snd_timestamp_t t;
	double expected, last, wake, check, queued = -1, proc, missing = -1;
	double maxbuf, pfilled, cfilled, cqueued = -1, avail_min;
	double sincejob;

	expected = ((double)loop->latency /
				(double)loop->play->rate_req) * 1000;
	getcurtimestamp(&t);
	last = (double)timediff(t, loop->xrun_last_update) / 1000;
	wake = (double)timediff(t, loop->xrun_last_wake) / 1000;
	check = (double)timediff(t, loop->xrun_last_check) / 1000;
	sincejob = (double)timediff(t, loop->tstamp_start) / 1000;
	if (loop->xrun_last_pdelay != XRUN_PROFILE_UNKNOWN)
		queued = ((double)loop->xrun_last_pdelay /
				(double)loop->play->rate) * 1000;
	if (loop->xrun_last_cdelay != XRUN_PROFILE_UNKNOWN)
		cqueued = ((double)loop->xrun_last_cdelay /
				(double)loop->capt->rate) * 1000;
	maxbuf = ((double)loop->play->buffer_size /
				(double)loop->play->rate) * 1000;
	proc = (double)loop->xrun_max_proctime / 1000;
	pfilled = ((double)(loop->xrun_buf_pcount + loop->xrun_out_frames) /
				(double)loop->play->rate) * 1000;
	cfilled = ((double)loop->xrun_buf_ccount /
				(double)loop->capt->rate) * 1000;
	avail_min = (((double)loop->play->buffer_size - 
				(double)loop->play->avail_min ) / 
				(double)loop->play->rate) * 1000;
	avail_min = expected - avail_min;
	if (queued >= 0)
		missing = last - queued;
	if (missing >= 0 && loop->xrun_max_missing < missing)
		loop->xrun_max_missing = missing;
	loop->xrun_max_proctime = 0;
	getcurtimestamp(&t);
	logit(LOG_INFO, "  last write before %.4fms, queued %.4fms/%.4fms -> missing %.4fms\n", last, queued, cqueued, missing);
	logit(LOG_INFO, "  expected %.4fms, processing %.4fms, max missing %.4fms\n", expected, proc, loop->xrun_max_missing);
	logit(LOG_INFO, "  last wake %.4fms, last check %.4fms, avail_min %.4fms\n", wake, check, avail_min);
	logit(LOG_INFO, "  max buf %.4fms, pfilled %.4fms, cfilled %.4fms\n", maxbuf, pfilled, cfilled);
	logit(LOG_INFO, "  job started before %.4fms\n", sincejob);
}

static inline void xrun_stats(struct loopback *loop)
{
	if (loop->xrun)
		xrun_stats0(loop);
}

static inline snd_pcm_uframes_t buf_avail(struct loopback_handle *lhandle)
{
	return lhandle->buf_size - lhandle->buf_count;
}

static void buf_remove(struct loopback *loop, snd_pcm_uframes_t count)
{
	/* remove samples from the capture buffer */
	if (count <= 0)
		return;
	if (loop->play->buf == loop->capt->buf) {
		if (count < loop->capt->buf_count)
			loop->capt->buf_count -= count;
		else
			loop->capt->buf_count = 0;
	}
}

#if 0
static void buf_add_copy(struct loopback *loop)
{
	struct loopback_handle *capt = loop->capt;
	struct loopback_handle *play = loop->play;
	snd_pcm_uframes_t count, count1, cpos, ppos;

	count = capt->buf_count;
	cpos = capt->buf_pos - count;
	if (cpos > capt->buf_size)
		cpos += capt->buf_size;
	ppos = (play->buf_pos + play->buf_count) % play->buf_size;
	while (count > 0) {
		count1 = count;
		if (count1 + cpos > capt->buf_size)
			count1 = capt->buf_size - cpos;
		if (count1 > buf_avail(play))
			count1 = buf_avail(play);
		if (count1 + ppos > play->buf_size)
			count1 = play->buf_size - ppos;
		if (count1 == 0)
			break;
		memcpy(play->buf + ppos * play->frame_size,
		       capt->buf + cpos * capt->frame_size,
		       count1 * capt->frame_size);
		play->buf_count += count1;
		capt->buf_count -= count1;
		ppos += count1;
		ppos %= play->buf_size;
		cpos += count1;
		cpos %= capt->buf_size;
		count -= count1;
	}
}
#endif

#ifdef USE_SAMPLERATE
static void buf_add_src(struct loopback *loop)
{
	struct loopback_handle *capt = loop->capt;
	struct loopback_handle *play = loop->play;
	float *old_data_out;
	snd_pcm_uframes_t count, pos, count1, pos1;
	count = capt->buf_count;
	pos = 0;
	pos1 = capt->buf_pos - count;
	if (pos1 > capt->buf_size)
		pos1 += capt->buf_size;
	while (count > 0) {
		count1 = count;
		if (count1 + pos1 > capt->buf_size)
			count1 = capt->buf_size - pos1;
		if (capt->format == SND_PCM_FORMAT_S32)
			src_int_to_float_array((int *)(capt->buf +
						pos1 * capt->frame_size),
					 loop->src_data.data_in +
					   pos * capt->channels,
					 count1 * capt->channels);
		else
			src_short_to_float_array((short *)(capt->buf +
						pos1 * capt->frame_size),
					 loop->src_data.data_in +
					   pos * capt->channels,
					 count1 * capt->channels);
		count -= count1;
		pos += count1;
		pos1 += count1;
		pos1 %= capt->buf_size;
	}
	loop->src_data.input_frames = pos;
	loop->src_data.output_frames = play->buf_size -
						loop->src_out_frames;
	loop->src_data.end_of_input = 0;
	old_data_out = loop->src_data.data_out;
	loop->src_data.data_out = old_data_out + loop->src_out_frames;
	src_process(loop->src_state, &loop->src_data);
	loop->src_data.data_out = old_data_out;
	capt->buf_count -= loop->src_data.input_frames_used;
	count = loop->src_data.output_frames_gen +
		loop->src_out_frames;
	pos = 0;
	pos1 = (play->buf_pos + play->buf_count) % play->buf_size;
	while (count > 0) {
		count1 = count;
		if (count1 + pos1 > play->buf_size)
			count1 = play->buf_size - pos1;
		if (count1 > buf_avail(play))
			count1 = buf_avail(play);
		if (count1 == 0)
			break;
		if (capt->format == SND_PCM_FORMAT_S32)
			src_float_to_int_array(loop->src_data.data_out +
					   pos * play->channels,
					 (int *)(play->buf +
					   pos1 * play->frame_size),
					 count1 * play->channels);
		else
			src_float_to_short_array(loop->src_data.data_out +
					   pos * play->channels,
					 (short *)(play->buf +
					   pos1 * play->frame_size),
					 count1 * play->channels);
		play->buf_count += count1;
		count -= count1;
		pos += count1;
		pos1 += count1;
		pos1 %= play->buf_size;
	}
#if 0
	printf("src: pos = %li, gen = %li, out = %li, count = %li\n",
		(long)pos, (long)loop->src_data.output_frames_gen,
		(long)loop->src_out_frames, play->buf_count);
#endif
	loop->src_out_frames = (loop->src_data.output_frames_gen +
					loop->src_out_frames) - pos;
	if (loop->src_out_frames > 0) {
		memmove(loop->src_data.data_out,
			loop->src_data.data_out + pos * play->channels,
			loop->src_out_frames * play->channels * sizeof(float));
	}
}
#else
static void buf_add_src(struct loopback *loop)
{
}
#endif

static void buf_add(struct loopback *loop, snd_pcm_uframes_t count)
{
	/* copy samples from capture to playback buffer */
	if (count <= 0)
		return;
	if (loop->play->buf == loop->capt->buf) {
		loop->play->buf_count += count;
	} else {
		buf_add_src(loop);
	}
}

static int xrun(struct loopback_handle *lhandle)
{
	int err;

	if (lhandle == lhandle->loopback->play) {
		logit(LOG_DEBUG, "underrun for %s\n", lhandle->id);
		xrun_stats(lhandle->loopback);
		if ((err = snd_pcm_prepare(lhandle->handle)) < 0)
			return err;
		lhandle->xrun_pending = 1;
	} else {
		logit(LOG_DEBUG, "overrun for %s\n", lhandle->id);
		xrun_stats(lhandle->loopback);
		if ((err = snd_pcm_prepare(lhandle->handle)) < 0)
			return err;
		lhandle->xrun_pending = 1;
	}
	return 0;
}

static int suspend(struct loopback_handle *lhandle)
{
	int err;

	while ((err = snd_pcm_resume(lhandle->handle)) == -EAGAIN)
		usleep(1);
	if (err < 0)
		return xrun(lhandle);
	return 0;
}

static int readit(struct loopback_handle *lhandle)
{
	snd_pcm_sframes_t r, res = 0;
	snd_pcm_sframes_t avail;
	int err;

	avail = snd_pcm_avail_update(lhandle->handle);
	if (avail == -EPIPE) {
		return xrun(lhandle);
	} else if (avail == -ESTRPIPE) {
		if ((err = suspend(lhandle)) < 0)
			return err;
	}
	if (avail > buf_avail(lhandle)) {
		lhandle->buf_over += avail - buf_avail(lhandle);
		avail = buf_avail(lhandle);
	} else if (avail == 0) {
		if (snd_pcm_state(lhandle->handle) == SND_PCM_STATE_DRAINING) {
			lhandle->loopback->reinit = 1;
			return 0;
		}
	}
	while (avail > 0) {
		r = buf_avail(lhandle);
		if (r + lhandle->buf_pos > lhandle->buf_size)
			r = lhandle->buf_size - lhandle->buf_pos;
		if (r > avail)
			r = avail;
		r = snd_pcm_readi(lhandle->handle,
				  lhandle->buf +
				  lhandle->buf_pos *
				  lhandle->frame_size, r);
		if (r == 0)
			return res;
		if (r < 0) {
			if (r == -EPIPE) {
				err = xrun(lhandle);
				return res > 0 ? res : err;
			} else if (r == -ESTRPIPE) {
				if ((err = suspend(lhandle)) < 0)
					return res > 0 ? res : err;
				r = 0;
			} else {
				return res > 0 ? res : r;
			}
		}
#ifdef FILE_CWRITE
		if (lhandle->loopback->cfile)
			fwrite(lhandle->buf + lhandle->buf_pos * lhandle->frame_size,
			       r, lhandle->frame_size, lhandle->loopback->cfile);
#endif
		res += r;
		if (lhandle->max < res)
			lhandle->max = res;
		lhandle->counter += r;
		lhandle->buf_count += r;
		lhandle->buf_pos += r;
		lhandle->buf_pos %= lhandle->buf_size;
		avail -= r;
	}
	return res;
}

static int writeit(struct loopback_handle *lhandle)
{
	snd_pcm_sframes_t avail;
	snd_pcm_sframes_t r, res = 0;
	int err;

      __again:
	avail = snd_pcm_avail_update(lhandle->handle);
	if (avail == -EPIPE) {
		if ((err = xrun(lhandle)) < 0)
			return err;
		return res;
	} else if (avail == -ESTRPIPE) {
		if ((err = suspend(lhandle)) < 0)
			return err;
		goto __again;
	}
	while (avail > 0 && lhandle->buf_count > 0) {
		r = lhandle->buf_count;
		if (r + lhandle->buf_pos > lhandle->buf_size)
			r = lhandle->buf_size - lhandle->buf_pos;
		if (r > avail)
			r = avail;
		r = snd_pcm_writei(lhandle->handle,
				   lhandle->buf +
				   lhandle->buf_pos *
				   lhandle->frame_size, r);
		if (r <= 0) {
			if (r == -EPIPE) {
				if ((err = xrun(lhandle)) < 0)
					return err;
				return res;
			} else if (r == -ESTRPIPE) {
			}
			return res > 0 ? res : r;
		}
#ifdef FILE_PWRITE
		if (lhandle->loopback->pfile)
			fwrite(lhandle->buf + lhandle->buf_pos * lhandle->frame_size,
			       r, lhandle->frame_size, lhandle->loopback->pfile);
#endif
		res += r;
		lhandle->counter += r;
		lhandle->buf_count -= r;
		lhandle->buf_pos += r;
		lhandle->buf_pos %= lhandle->buf_size;
		xrun_profile(lhandle->loopback);
		if (lhandle->loopback->stop_pending) {
			lhandle->loopback->stop_count += r;
			if (lhandle->loopback->stop_count * lhandle->pitch >
			    lhandle->loopback->latency * 3) {
				lhandle->loopback->stop_pending = 0;
				lhandle->loopback->reinit = 1;
				break;
			}
		}
	}
	return res;
}

static snd_pcm_sframes_t remove_samples(struct loopback *loop,
					int capture_preferred,
					snd_pcm_sframes_t count)
{
	struct loopback_handle *play = loop->play;
	struct loopback_handle *capt = loop->capt;

	if (loop->play->buf == loop->capt->buf) {
		if (count > loop->play->buf_count)
			count = loop->play->buf_count;
		if (count > loop->capt->buf_count)
			count = loop->capt->buf_count;
		capt->buf_count -= count;
		play->buf_pos += count;
		play->buf_pos %= play->buf_size;
		play->buf_count -= count;
		return count;
	}
	if (capture_preferred) {
		if (count > capt->buf_count)
			count = capt->buf_count;
		capt->buf_count -= count;
	} else {
		if (count > play->buf_count)
			count = play->buf_count;
		play->buf_count -= count;
	}
	return count;
}

static int xrun_sync(struct loopback *loop)
{
	struct loopback_handle *play = loop->play;
	struct loopback_handle *capt = loop->capt;
	snd_pcm_uframes_t fill = get_whole_latency(loop);
	snd_pcm_sframes_t pdelay, cdelay, delay1, pdelay1, cdelay1, diff;
	int err;

      __again:
	if (verbose > 5)
		snd_output_printf(loop->output, "%s: xrun sync %i %i\n", loop->id, capt->xrun_pending, play->xrun_pending);
	if (capt->xrun_pending) {
	      __pagain:
		capt->xrun_pending = 0;
		if ((err = snd_pcm_prepare(capt->handle)) < 0) {
			logit(LOG_CRIT, "%s prepare failed: %s\n", capt->id, snd_strerror(err));
			return err;
		}
		if ((err = snd_pcm_start(capt->handle)) < 0) {
			logit(LOG_CRIT, "%s start failed: %s\n", capt->id, snd_strerror(err));
			return err;
		}
	} else {
		diff = readit(capt);
		buf_add(loop, diff);
		if (capt->xrun_pending)
			goto __pagain;
	}
	/* skip additional playback samples */
	if ((err = snd_pcm_delay(capt->handle, &cdelay)) < 0) {
		if (err == -EPIPE) {
			capt->xrun_pending = 1;
			goto __again;
		}
		if (err == -ESTRPIPE) {
			err = suspend(capt);
			if (err < 0)
				return err;
			goto __again;
		}
		logit(LOG_CRIT, "%s capture delay failed: %s\n", capt->id, snd_strerror(err));
		return err;
	}
	if ((err = snd_pcm_delay(play->handle, &pdelay)) < 0) {
		if (err == -EPIPE) {
			pdelay = 0;
			play->xrun_pending = 1;
		} else if (err == -ESTRPIPE) {
			err = suspend(play);
			if (err < 0)
				return err;
			goto __again;
		} else {
			logit(LOG_CRIT, "%s playback delay failed: %s\n", play->id, snd_strerror(err));
			return err;
		}
	}
	capt->counter = cdelay;
	play->counter = pdelay;
	if (play->buf != capt->buf)
		cdelay += capt->buf_count;
	pdelay += play->buf_count;
#ifdef USE_SAMPLERATE
	pdelay += loop->src_out_frames;
#endif
	cdelay1 = cdelay * capt->pitch;
	pdelay1 = pdelay * play->pitch;
	delay1 = cdelay1 + pdelay1;
	capt->total_queued = 0;
	play->total_queued = 0;
	loop->total_queued_count = 0;
	loop->pitch_diff = loop->pitch_diff_min = loop->pitch_diff_max = 0;
	if (verbose > 6) {
		snd_output_printf(loop->output,
			"sync: cdelay=%li(%li), pdelay=%li(%li), fill=%li (delay=%li)"
#ifdef USE_SAMPLERATE
			", src_out=%li"
#endif
			"\n",
			(long)cdelay, (long)cdelay1, (long)pdelay, (long)pdelay1,
			(long)fill, (long)delay1
#ifdef USE_SAMPLERATE
			, (long)loop->src_out_frames
#endif
			);
		snd_output_printf(loop->output,
			"sync: cbufcount=%li, pbufcount=%li\n",
			(long)capt->buf_count, (long)play->buf_count);
	}
	if (delay1 > fill && capt->counter > 0) {
		if ((err = snd_pcm_drop(capt->handle)) < 0)
			return err;
		if ((err = snd_pcm_prepare(capt->handle)) < 0)
			return err;
		if ((err = snd_pcm_start(capt->handle)) < 0)
			return err;
		diff = remove_samples(loop, 1, (delay1 - fill) / capt->pitch);
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: capt stop removed %li samples\n", (long)diff);
		goto __again;
	}
	if (delay1 > fill) {
		diff = (delay1 - fill) / play->pitch;
		if (diff > play->buf_count)
			diff = play->buf_count;
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: removing %li playback samples, delay1=%li\n", (long)diff, (long)delay1);
		diff = remove_samples(loop, 0, diff);
		pdelay -= diff;
		pdelay1 = pdelay * play->pitch;
		delay1 = cdelay1 + pdelay1;
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: removed %li playback samples, delay1=%li\n", (long)diff, (long)delay1);
	}
	if (delay1 > fill) {
		diff = (delay1 - fill) / capt->pitch;
		if (diff > capt->buf_count)
			diff = capt->buf_count;
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: removing %li captured samples, delay1=%li\n", (long)diff, (long)delay1);
		diff -= remove_samples(loop, 1, diff);
		cdelay -= diff;
		cdelay1 = cdelay * capt->pitch;
		delay1 = cdelay1 + pdelay1;		
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: removed %li captured samples, delay1=%li\n", (long)diff, (long)delay1);
	}
	if (play->xrun_pending) {
		play->xrun_pending = 0;
		diff = (fill - delay1) / play->pitch;
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: xrun_pending, silence filling %li / buf_count=%li\n", (long)diff, play->buf_count);
		if (fill > delay1 && play->buf_count < diff) {
			diff = diff - play->buf_count;
			if (verbose > 6)
				snd_output_printf(loop->output,
					"sync: playback silence added %li samples\n", (long)diff);
			play->buf_pos -= diff;
			play->buf_pos %= play->buf_size;
			if ((err = snd_pcm_format_set_silence(play->format, play->buf + play->buf_pos * play->channels, diff)) < 0)
				return err;
			play->buf_count += diff;
		}
		if ((err = snd_pcm_prepare(play->handle)) < 0) {
			logit(LOG_CRIT, "%s prepare failed: %s\n", play->id, snd_strerror(err));

			return err;
		}
		delay1 = writeit(play);
		if (verbose > 6)
			snd_output_printf(loop->output,
				"sync: playback wrote %li samples\n", (long)delay1);
		if (delay1 > diff) {
			buf_remove(loop, delay1 - diff);
			if (verbose > 6)
				snd_output_printf(loop->output,
					"sync: playback buf_remove %li samples\n", (long)(delay1 - diff));
		}
		if ((err = snd_pcm_start(play->handle)) < 0) {
			logit(LOG_CRIT, "%s start failed: %s\n", play->id, snd_strerror(err));
			return err;
		}
	} else if (delay1 < fill) {
		diff = (fill - delay1) / play->pitch;
		while (diff > 0) {
			delay1 = play->buf_size - play->buf_pos;
			if (verbose > 6)
				snd_output_printf(loop->output,
					"sync: playback short, silence filling %li / buf_count=%li\n", (long)delay1, play->buf_count);
			if (delay1 > diff)
				delay1 = diff;
			if ((err = snd_pcm_format_set_silence(play->format, play->buf + play->buf_pos * play->channels, delay1)) < 0)
				return err;
			play->buf_pos += delay1;
			play->buf_pos %= play->buf_size;
			play->buf_count += delay1;
			diff -= delay1;
		}
		writeit(play);
	}
	if (verbose > 5) {
		snd_output_printf(loop->output, "%s: xrun sync ok\n", loop->id);
		if (verbose > 6) {
			if (snd_pcm_delay(capt->handle, &cdelay) < 0)
				cdelay = -1;
			if (snd_pcm_delay(play->handle, &pdelay) < 0)
				pdelay = -1;
			if (play->buf != capt->buf)
				cdelay += capt->buf_count;
			pdelay += play->buf_count;
#ifdef USE_SAMPLERATE
			pdelay += loop->src_out_frames;
#endif
			cdelay1 = cdelay * capt->pitch;
			pdelay1 = pdelay * play->pitch;
			delay1 = cdelay1 + pdelay1;
			snd_output_printf(loop->output, "%s: sync verify: %li\n", loop->id, delay1);
		}
	}
	loop->xrun_max_proctime = 0;
	return 0;
}

static int set_notify(struct loopback_handle *lhandle, int enable)
{
	int err;

	if (lhandle->ctl_notify == NULL)
		return 0;
	snd_ctl_elem_value_set_boolean(lhandle->ctl_notify, 0, enable);
	err = snd_ctl_elem_write(lhandle->ctl, lhandle->ctl_notify);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot set PCM Notify element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	err = snd_ctl_elem_read(lhandle->ctl, lhandle->ctl_notify);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot get PCM Notify element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_rate_shift(struct loopback_handle *lhandle, double pitch)
{
	int err;

	if (lhandle->ctl_rate_shift == NULL)
		return 0;
	snd_ctl_elem_value_set_integer(lhandle->ctl_rate_shift, 0, pitch * 100000);
	err = snd_ctl_elem_write(lhandle->ctl, lhandle->ctl_rate_shift);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot set PCM Rate Shift element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return 0;
}

void update_pitch(struct loopback *loop)
{
	double pitch = loop->pitch;

#ifdef USE_SAMPLERATE
	if (loop->sync == SYNC_TYPE_SAMPLERATE) {
		loop->src_data.src_ratio = (double)1.0 / (pitch *
				loop->play->pitch * loop->capt->pitch);
		if (verbose > 2)
			snd_output_printf(loop->output, "%s: Samplerate src_ratio update1: %.8f\n", loop->id, loop->src_data.src_ratio);
	} else
#endif
	if (loop->sync == SYNC_TYPE_CAPTRATESHIFT) {
		set_rate_shift(loop->capt, pitch);
#ifdef USE_SAMPLERATE
		if (loop->use_samplerate) {
			loop->src_data.src_ratio = 
				(double)1.0 /
					(loop->play->pitch * loop->capt->pitch);
			if (verbose > 2)
				snd_output_printf(loop->output, "%s: Samplerate src_ratio update2: %.8f\n", loop->id, loop->src_data.src_ratio);
		}
#endif
	}
	else if (loop->sync == SYNC_TYPE_PLAYRATESHIFT) {
		set_rate_shift(loop->play, pitch);
#ifdef USE_SAMPLERATE
		if (loop->use_samplerate) {
			loop->src_data.src_ratio = 
				(double)1.0 /
					(loop->play->pitch * loop->capt->pitch);
			if (verbose > 2)
				snd_output_printf(loop->output, "%s: Samplerate src_ratio update3: %.8f\n", loop->id, loop->src_data.src_ratio);
		}
#endif
	}
	if (verbose)
		snd_output_printf(loop->output, "New pitch for %s: %.8f (min/max samples = %li/%li)\n", loop->id, pitch, loop->pitch_diff_min, loop->pitch_diff_max);
}

static int get_active(struct loopback_handle *lhandle)
{
	int err;

	if (lhandle->ctl_active == NULL)
		return 0;
	err = snd_ctl_elem_read(lhandle->ctl, lhandle->ctl_active);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot get PCM Slave Active element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return snd_ctl_elem_value_get_boolean(lhandle->ctl_active, 0);
}

static int get_format(struct loopback_handle *lhandle)
{
	int err;

	if (lhandle->ctl_format == NULL)
		return 0;
	err = snd_ctl_elem_read(lhandle->ctl, lhandle->ctl_format);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot get PCM Format element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return snd_ctl_elem_value_get_integer(lhandle->ctl_format, 0);
}

static int get_rate(struct loopback_handle *lhandle)
{
	int err;

	if (lhandle->ctl_rate == NULL)
		return 0;
	err = snd_ctl_elem_read(lhandle->ctl, lhandle->ctl_rate);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot get PCM Rate element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return snd_ctl_elem_value_get_integer(lhandle->ctl_rate, 0);
}

static int get_channels(struct loopback_handle *lhandle)
{
	int err;

	if (lhandle->ctl_channels == NULL)
		return 0;
	err = snd_ctl_elem_read(lhandle->ctl, lhandle->ctl_channels);
	if (err < 0) {
		logit(LOG_CRIT, "Cannot get PCM Channels element for %s: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	return snd_ctl_elem_value_get_integer(lhandle->ctl_channels, 0);
}

static void openctl_elem(struct loopback_handle *lhandle,
			 int device, int subdevice,
			 const char *name,
			 snd_ctl_elem_value_t **elem)
{
	int err;

	if (snd_ctl_elem_value_malloc(elem) < 0) {
		*elem = NULL;
	} else {
		snd_ctl_elem_value_set_interface(*elem,
						 SND_CTL_ELEM_IFACE_PCM);
		snd_ctl_elem_value_set_device(*elem, device);
		snd_ctl_elem_value_set_subdevice(*elem, subdevice);
		snd_ctl_elem_value_set_name(*elem, name);
		err = snd_ctl_elem_read(lhandle->ctl, *elem);
		if (err < 0) {
			snd_ctl_elem_value_free(*elem);
			*elem = NULL;
		}
	}
}

static int openctl(struct loopback_handle *lhandle, int device, int subdevice)
{
	int err;

	lhandle->ctl_rate_shift = NULL;
	if (lhandle->loopback->play == lhandle) {
		if (lhandle->loopback->controls)
			goto __events;
		return 0;
	}
	openctl_elem(lhandle, device, subdevice, "PCM Notify",
			&lhandle->ctl_notify);
	openctl_elem(lhandle, device, subdevice, "PCM Rate Shift 100000",
			&lhandle->ctl_rate_shift);
	set_rate_shift(lhandle, 1);
	openctl_elem(lhandle, device, subdevice, "PCM Slave Active",
			&lhandle->ctl_active);
	openctl_elem(lhandle, device, subdevice, "PCM Slave Format",
			&lhandle->ctl_format);
	openctl_elem(lhandle, device, subdevice, "PCM Slave Rate",
			&lhandle->ctl_rate);
	openctl_elem(lhandle, device, subdevice, "PCM Slave Channels",
			&lhandle->ctl_channels);
	if ((lhandle->ctl_active &&
	     lhandle->ctl_format &&
	     lhandle->ctl_rate &&
	     lhandle->ctl_channels) ||
	    lhandle->loopback->controls) {
	      __events:
		if ((err = snd_ctl_poll_descriptors_count(lhandle->ctl)) < 0)
			lhandle->ctl_pollfd_count = 0;
		else
			lhandle->ctl_pollfd_count = err;
		if (snd_ctl_subscribe_events(lhandle->ctl, 1) < 0)
			lhandle->ctl_pollfd_count = 0;
	}
	return 0;
}

static int openit(struct loopback_handle *lhandle)
{
	snd_pcm_info_t *info;
	int stream = lhandle == lhandle->loopback->play ?
				SND_PCM_STREAM_PLAYBACK :
				SND_PCM_STREAM_CAPTURE;
	int err, card, device, subdevice;
	pcm_open_lock();
	err = snd_pcm_open(&lhandle->handle, lhandle->device, stream, SND_PCM_NONBLOCK);
	pcm_open_unlock();
	if (err < 0) {
		logit(LOG_CRIT, "%s open error: %s\n", lhandle->id, snd_strerror(err));
		return err;
	}
	if ((err = snd_pcm_info_malloc(&info)) < 0)
		return err;
	if ((err = snd_pcm_info(lhandle->handle, info)) < 0) {
		snd_pcm_info_free(info);
		return err;
	}
	card = snd_pcm_info_get_card(info);
	device = snd_pcm_info_get_device(info);
	subdevice = snd_pcm_info_get_subdevice(info);
	snd_pcm_info_free(info);
	lhandle->card_number = card;
	lhandle->ctl = NULL;
	if (card >= 0 || lhandle->ctldev) {
		char name[16], *dev = lhandle->ctldev;
		if (dev == NULL) {
			sprintf(name, "hw:%i", card);
			dev = name;
		}
		pcm_open_lock();
		err = snd_ctl_open(&lhandle->ctl, dev, SND_CTL_NONBLOCK);
		pcm_open_unlock();
		if (err < 0) {
			logit(LOG_CRIT, "%s [%s] ctl open error: %s\n", lhandle->id, dev, snd_strerror(err));
			lhandle->ctl = NULL;
		}
		if (lhandle->ctl)
			openctl(lhandle, device, subdevice);
	}
	return 0;
}

static int freeit(struct loopback_handle *lhandle)
{
	free(lhandle->buf);
	lhandle->buf = NULL;
	return 0;
}

static int closeit(struct loopback_handle *lhandle)
{
	int err = 0;

	set_rate_shift(lhandle, 1);
	if (lhandle->ctl_rate_shift)
		snd_ctl_elem_value_free(lhandle->ctl_rate_shift);
	lhandle->ctl_rate_shift = NULL;
	if (lhandle->ctl)
		err = snd_ctl_close(lhandle->ctl);
	lhandle->ctl = NULL;
	if (lhandle->handle)
		err = snd_pcm_close(lhandle->handle);
	lhandle->handle = NULL;
	return err;
}

static int init_handle(struct loopback_handle *lhandle, int alloc)
{
	snd_pcm_uframes_t lat;
	lhandle->frame_size = (snd_pcm_format_physical_width(lhandle->format) 
						/ 8) * lhandle->channels;
	lhandle->sync_point = lhandle->rate * 15;	/* every 15 seconds */
	lat = lhandle->loopback->latency;
	if (lhandle->buffer_size > lat)
		lat = lhandle->buffer_size;
	lhandle->buf_size = lat * 2;
	if (alloc) {
		lhandle->buf = calloc(1, lhandle->buf_size * lhandle->frame_size);
		if (lhandle->buf == NULL)
			return -ENOMEM;
	}
	return 0;
}

int pcmjob_init(struct loopback *loop)
{
	int err;
	char id[128];

#ifdef FILE_CWRITE
	loop->cfile = fopen(FILE_CWRITE, "w+");
#endif
#ifdef FILE_PWRITE
	loop->pfile = fopen(FILE_PWRITE, "w+");
#endif
	if ((err = openit(loop->play)) < 0)
		goto __error;
	if ((err = openit(loop->capt)) < 0)
		goto __error;
	snprintf(id, sizeof(id), "%s/%s", loop->play->id, loop->capt->id);
	id[sizeof(id)-1] = '\0';
	loop->id = strdup(id);
	if (loop->sync == SYNC_TYPE_AUTO && loop->capt->ctl_rate_shift)
		loop->sync = SYNC_TYPE_CAPTRATESHIFT;
	if (loop->sync == SYNC_TYPE_AUTO && loop->play->ctl_rate_shift)
		loop->sync = SYNC_TYPE_PLAYRATESHIFT;
#ifdef USE_SAMPLERATE
	if (loop->sync == SYNC_TYPE_AUTO && loop->src_enable)
		loop->sync = SYNC_TYPE_SAMPLERATE;
#endif
	if (loop->sync == SYNC_TYPE_AUTO)
		loop->sync = SYNC_TYPE_SIMPLE;
	if (loop->slave == SLAVE_TYPE_AUTO &&
	    loop->capt->ctl_notify &&
	    loop->capt->ctl_active &&
	    loop->capt->ctl_format &&
	    loop->capt->ctl_rate &&
	    loop->capt->ctl_channels)
		loop->slave = SLAVE_TYPE_ON;
	if (loop->slave == SLAVE_TYPE_ON) {
		err = set_notify(loop->capt, 1);
		if (err < 0)
			goto __error;
		if (loop->capt->ctl_notify == NULL ||
		    snd_ctl_elem_value_get_boolean(loop->capt->ctl_notify, 0) == 0) {
			logit(LOG_CRIT, "unable to enable slave mode for %s\n", loop->id);
			err = -EINVAL;
			goto __error;
		}
	}
	err = control_init(loop);
	if (err < 0)
		goto __error;
	return 0;
      __error:
	pcmjob_done(loop);
	return err;
}

static void freeloop(struct loopback *loop)
{
#ifdef USE_SAMPLERATE
	if (loop->use_samplerate) {
		if (loop->src_state)
			src_delete(loop->src_state);
		loop->src_state = NULL;
		free(loop->src_data.data_in);
		loop->src_data.data_in = NULL;
		free(loop->src_data.data_out);
		loop->src_data.data_out = NULL;
	}
#endif
	if (loop->play->buf == loop->capt->buf)
		loop->play->buf = NULL;
	freeit(loop->play);
	freeit(loop->capt);
}

int pcmjob_done(struct loopback *loop)
{
	control_done(loop);
	closeit(loop->play);
	closeit(loop->capt);
	freeloop(loop);
	free(loop->id);
	loop->id = NULL;
#ifdef FILE_PWRITE
	if (loop->pfile) {
		fclose(loop->pfile);
		loop->pfile = NULL;
	}
#endif
#ifdef FILE_CWRITE
	if (loop->cfile) {
		fclose(loop->cfile);
		loop->cfile = NULL;
	}
#endif
	return 0;
}

static void lhandle_start(struct loopback_handle *lhandle)
{
	lhandle->buf_pos = 0;
	lhandle->buf_count = 0;
	lhandle->counter = 0;
	lhandle->total_queued = 0;
}

static void fix_format(struct loopback *loop, int force)
{
	snd_pcm_format_t format = loop->capt->format;

	if (!force && loop->sync != SYNC_TYPE_SAMPLERATE)
		return;
	if (format == SND_PCM_FORMAT_S16 ||
	    format == SND_PCM_FORMAT_S32)
		return;
	if (snd_pcm_format_width(format) > 16)
		format = SND_PCM_FORMAT_S32;
	else
		format = SND_PCM_FORMAT_S16;
	loop->capt->format = format;
	loop->play->format = format;
}

int pcmjob_start(struct loopback *loop)
{
	snd_pcm_uframes_t count;
	int err;

	loop->pollfd_count = loop->play->ctl_pollfd_count +
			     loop->capt->ctl_pollfd_count;
	if ((err = snd_pcm_poll_descriptors_count(loop->play->handle)) < 0)
		goto __error;
	loop->play->pollfd_count = err;
	loop->pollfd_count += err;
	if ((err = snd_pcm_poll_descriptors_count(loop->capt->handle)) < 0)
		goto __error;
	loop->capt->pollfd_count = err;
	loop->pollfd_count += err;
	if (loop->slave == SLAVE_TYPE_ON) {
		err = get_active(loop->capt);
		if (err < 0)
			goto __error;
		if (err == 0)		/* stream is not active */
			return 0;
		err = get_format(loop->capt);
		if (err < 0)
			goto __error;
		loop->play->format = loop->capt->format = err;
		fix_format(loop, 0);
		err = get_rate(loop->capt);
		if (err < 0)
			goto __error;
		loop->play->rate_req = loop->capt->rate_req = err;
		err = get_channels(loop->capt);
		if (err < 0)
			goto __error;
		loop->play->channels = loop->capt->channels = err;
	}
	loop->reinit = 0;
	loop->use_samplerate = 0;
__again:
	if (loop->latency_req) {
		loop->latency_reqtime = frames_to_time(loop->play->rate_req,
						       loop->latency_req);
		loop->latency_req = 0;
	}
	loop->latency = time_to_frames(loop->play->rate_req, loop->latency_reqtime);
	if ((err = setparams(loop, loop->latency/2)) < 0)
		goto __error;
	if (verbose)
		showlatency(loop->output, loop->latency, loop->play->rate_req, "Latency");
	if (loop->play->access == loop->capt->access &&
	    loop->play->format == loop->capt->format &&
	    loop->play->rate == loop->capt->rate &&
	    loop->play->channels == loop->play->channels &&
	    loop->sync != SYNC_TYPE_SAMPLERATE) {
		if (verbose > 1)
			snd_output_printf(loop->output, "shared buffer!!!\n");
		if ((err = init_handle(loop->play, 1)) < 0)
			goto __error;
		if ((err = init_handle(loop->capt, 0)) < 0)
			goto __error;
		if (loop->play->buf_size < loop->capt->buf_size) {
			char *nbuf = realloc(loop->play->buf,
					     loop->capt->buf_size *
					       loop->capt->frame_size);
			if (nbuf == NULL) {
				err = -ENOMEM;
				goto __error;
			}
			loop->play->buf = nbuf;
			loop->play->buf_size = loop->capt->buf_size;
		} else if (loop->capt->buf_size < loop->play->buf_size) {
			char *nbuf = realloc(loop->capt->buf,
					     loop->play->buf_size *
					       loop->play->frame_size);
			if (nbuf == NULL) {
				err = -ENOMEM;
				goto __error;
			}
			loop->capt->buf = nbuf;
			loop->capt->buf_size = loop->play->buf_size;
		}
		loop->capt->buf = loop->play->buf;
	} else {
		if ((err = init_handle(loop->play, 1)) < 0)
			goto __error;
		if ((err = init_handle(loop->capt, 1)) < 0)
			goto __error;
		if (loop->play->rate_req != loop->play->rate ||
                    loop->capt->rate_req != loop->capt->rate) {
                        snd_pcm_format_t format1, format2;
			loop->use_samplerate = 1;
                        format1 = loop->play->format;
                        format2 = loop->capt->format;
                        fix_format(loop, 1);
                        if (loop->play->format != format1 ||
                            loop->capt->format != format2) {
                                pcmjob_stop(loop);
                                goto __again;
                        }
                }
	}
#ifdef USE_SAMPLERATE
	if (loop->sync == SYNC_TYPE_SAMPLERATE)
		loop->use_samplerate = 1;
	if (loop->use_samplerate && !loop->src_enable) {
		logit(LOG_CRIT, "samplerate conversion required but disabled\n");
		loop->use_samplerate = 0;
		err = -EIO;
		goto __error;		
	}
	if (loop->use_samplerate) {
		if ((loop->capt->format != SND_PCM_FORMAT_S16 ||
		    loop->play->format != SND_PCM_FORMAT_S16) &&
		    (loop->capt->format != SND_PCM_FORMAT_S32 ||
		     loop->play->format != SND_PCM_FORMAT_S32)) {
			logit(LOG_CRIT, "samplerate conversion supports only %s or %s formats (play=%s, capt=%s)\n", snd_pcm_format_name(SND_PCM_FORMAT_S16), snd_pcm_format_name(SND_PCM_FORMAT_S32), snd_pcm_format_name(loop->play->format), snd_pcm_format_name(loop->capt->format));
			loop->use_samplerate = 0;
			err = -EIO;
			goto __error;		
		}
		loop->src_state = src_new(loop->src_converter_type,
					  loop->play->channels, &err);
		loop->src_data.data_in = calloc(1, sizeof(float)*loop->capt->channels*loop->capt->buf_size);
		if (loop->src_data.data_in == NULL) {
			err = -ENOMEM;
			goto __error;
		}
		loop->src_data.data_out =  calloc(1, sizeof(float)*loop->play->channels*loop->play->buf_size);
		if (loop->src_data.data_out == NULL) {
			err = -ENOMEM;
			goto __error;
		}
		loop->src_data.src_ratio = (double)loop->play->rate /
					   (double)loop->capt->rate;
		loop->src_data.end_of_input = 0;
		loop->src_out_frames = 0;
	} else {
		loop->src_state = NULL;
	}
#else
	if (loop->sync == SYNC_TYPE_SAMPLERATE || loop->use_samplerate) {
		logit(LOG_CRIT, "alsaloop is compiled without libsamplerate support\n");
		err = -EIO;
		goto __error;
	}
#endif
	if (verbose) {
		snd_output_printf(loop->output, "%s sync type: %s", loop->id, sync_types[loop->sync]);
#ifdef USE_SAMPLERATE
		if (loop->sync == SYNC_TYPE_SAMPLERATE)
			snd_output_printf(loop->output, " (%s)", src_types[loop->src_converter_type]);
#endif
		snd_output_printf(loop->output, "\n");
	}
	lhandle_start(loop->play);
	lhandle_start(loop->capt);
	if ((err = snd_pcm_format_set_silence(loop->play->format,
					      loop->play->buf,
					      loop->play->buf_size * loop->play->channels)) < 0) {
		logit(LOG_CRIT, "%s: silence error\n", loop->id);
		goto __error;
	}
	if (verbose > 4)
		snd_output_printf(loop->output, "%s: capt->buffer_size = %li, play->buffer_size = %li\n", loop->id, loop->capt->buf_size, loop->play->buf_size);
	loop->pitch = 1.0;
	update_pitch(loop);
	loop->pitch_delta = 1.0 / ((double)loop->capt->rate * 4);
	loop->total_queued_count = 0;
	loop->pitch_diff = 0;
	count = get_whole_latency(loop) / loop->play->pitch;
	loop->play->buf_count = count;
	if (loop->play->buf == loop->capt->buf)
		loop->capt->buf_pos = count;
	err = writeit(loop->play);
	if (verbose > 4)
		snd_output_printf(loop->output, "%s: silence queued %i samples\n", loop->id, err);
	if (count > loop->play->buffer_size)
		count = loop->play->buffer_size;
	if (err != count) {
		logit(LOG_CRIT, "%s: initial playback fill error (%i/%i/%i)\n", loop->id, err, (int)count, loop->play->buffer_size);
		err = -EIO;
		goto __error;
	}
	loop->running = 1;
	loop->stop_pending = 0;
	if (loop->xrun) {
		getcurtimestamp(&loop->xrun_last_update);
		loop->xrun_last_pdelay = XRUN_PROFILE_UNKNOWN;
		loop->xrun_last_cdelay = XRUN_PROFILE_UNKNOWN;
		loop->xrun_max_proctime = 0;
	}
	if ((err = snd_pcm_start(loop->capt->handle)) < 0) {
		logit(LOG_CRIT, "pcm start %s error: %s\n", loop->capt->id, snd_strerror(err));
		goto __error;
	}
	if (!loop->linked) {
		if ((err = snd_pcm_start(loop->play->handle)) < 0) {
			logit(LOG_CRIT, "pcm start %s error: %s\n", loop->play->id, snd_strerror(err));
			goto __error;
		}
	}
	return 0;
      __error:
	pcmjob_stop(loop);
	return err;
}

int pcmjob_stop(struct loopback *loop)
{
	int err;

	if (loop->running) {
		if ((err = snd_pcm_drop(loop->capt->handle)) < 0)
			logit(LOG_WARNING, "pcm drop %s error: %s\n", loop->capt->id, snd_strerror(err));
		if ((err = snd_pcm_drop(loop->play->handle)) < 0)
			logit(LOG_WARNING, "pcm drop %s error: %s\n", loop->play->id, snd_strerror(err));
		if ((err = snd_pcm_hw_free(loop->capt->handle)) < 0)
			logit(LOG_WARNING, "pcm hw_free %s error: %s\n", loop->capt->id, snd_strerror(err));
		if ((err = snd_pcm_hw_free(loop->play->handle)) < 0)
			logit(LOG_WARNING, "pcm hw_free %s error: %s\n", loop->play->id, snd_strerror(err));
		loop->running = 0;
	}
	freeloop(loop);
	return 0;
}

int pcmjob_pollfds_init(struct loopback *loop, struct pollfd *fds)
{
	int err, idx = 0;

	if (loop->running) {
		err = snd_pcm_poll_descriptors(loop->play->handle, fds + idx, loop->play->pollfd_count);
		if (err < 0)
			return err;
		idx += loop->play->pollfd_count;
		err = snd_pcm_poll_descriptors(loop->capt->handle, fds + idx, loop->capt->pollfd_count);
		if (err < 0)
			return err;
		idx += loop->capt->pollfd_count;
	}
	if (loop->play->ctl_pollfd_count > 0 &&
	    (loop->slave == SLAVE_TYPE_ON || loop->controls)) {
		err = snd_ctl_poll_descriptors(loop->play->ctl, fds + idx, loop->play->ctl_pollfd_count);
		if (err < 0)
			return err;
		idx += loop->play->ctl_pollfd_count;
	}
	if (loop->capt->ctl_pollfd_count > 0 &&
	    (loop->slave == SLAVE_TYPE_ON || loop->controls)) {
		err = snd_ctl_poll_descriptors(loop->capt->ctl, fds + idx, loop->capt->ctl_pollfd_count);
		if (err < 0)
			return err;
		idx += loop->capt->ctl_pollfd_count;
	}
	loop->active_pollfd_count = idx;
	return idx;
}

static snd_pcm_sframes_t get_queued_playback_samples(struct loopback *loop)
{
	snd_pcm_sframes_t delay;
	int err;

	if ((err = snd_pcm_delay(loop->play->handle, &delay)) < 0)
		return 0;
	loop->play->last_delay = delay;
	delay += loop->play->buf_count;
#ifdef USE_SAMPLERATE
	delay += loop->src_out_frames;
#endif
	return delay;
}

static snd_pcm_sframes_t get_queued_capture_samples(struct loopback *loop)
{
	snd_pcm_sframes_t delay;
	int err;

	if ((err = snd_pcm_delay(loop->capt->handle, &delay)) < 0)
		return 0;
	loop->capt->last_delay = delay;
	delay += loop->capt->buf_count;
	return delay;
}

static int ctl_event_check(snd_ctl_elem_value_t *val, snd_ctl_event_t *ev)
{
	snd_ctl_elem_id_t *id1, *id2;
	snd_ctl_elem_id_alloca(&id1);
	snd_ctl_elem_id_alloca(&id2);
	snd_ctl_elem_value_get_id(val, id1);
	snd_ctl_event_elem_get_id(ev, id2);
	if (snd_ctl_event_elem_get_mask(ev) == SND_CTL_EVENT_MASK_REMOVE)
		return 0;
	if ((snd_ctl_event_elem_get_mask(ev) & SND_CTL_EVENT_MASK_VALUE) == 0)
		return 0;
	return control_id_match(id1, id2);
}

static int handle_ctl_events(struct loopback_handle *lhandle,
			     unsigned short events)
{
	struct loopback *loop = lhandle->loopback;
	snd_ctl_event_t *ev;
	int err, restart = 0;

	snd_ctl_event_alloca(&ev);
	while ((err = snd_ctl_read(lhandle->ctl, ev)) != 0 && err != -EAGAIN) {
		if (err < 0)
			break;
		if (snd_ctl_event_get_type(ev) != SND_CTL_EVENT_ELEM)
			continue;
		if (lhandle == loop->play)
			goto __ctl_check;
		if (verbose > 6)
			snd_output_printf(loop->output, "%s: ctl event!!!! %s\n", lhandle->id, snd_ctl_event_elem_get_name(ev));
		if (ctl_event_check(lhandle->ctl_active, ev)) {
			continue;
		} else if (ctl_event_check(lhandle->ctl_format, ev)) {
			err = get_format(lhandle);
			if (lhandle->format != err)
				restart = 1;
			continue;
		} else if (ctl_event_check(lhandle->ctl_rate, ev)) {
			err = get_rate(lhandle);
			if (lhandle->rate != err)
				restart = 1;
			continue;
		} else if (ctl_event_check(lhandle->ctl_channels, ev)) {
			err = get_channels(lhandle);
			if (lhandle->channels != err)
				restart = 1;
			continue;
		}
	      __ctl_check:
		control_event(lhandle, ev);
	}
	err = get_active(lhandle);
	if (verbose > 7)
		snd_output_printf(loop->output, "%s: ctl event active %i\n", lhandle->id, err);
	if (!err) {
		if (lhandle->loopback->running) {
			loop->stop_pending = 1;
			loop->stop_count = 0;
		}
	} else {
		loop->stop_pending = 0;
		if (loop->running == 0)
			restart = 1;
	}
	if (restart) {
		pcmjob_stop(loop);
		err = pcmjob_start(loop);
		if (err < 0)
			return err;
	}
	return 1;
}

int pcmjob_pollfds_handle(struct loopback *loop, struct pollfd *fds)
{
	struct loopback_handle *play = loop->play;
	struct loopback_handle *capt = loop->capt;
	unsigned short prevents, crevents, events;
	snd_pcm_uframes_t ccount, pcount;
	int err, loopcount = 10, idx;

	if (verbose > 11)
		snd_output_printf(loop->output, "%s: pollfds handle\n", loop->id);
	if (verbose > 13 || loop->xrun)
		getcurtimestamp(&loop->tstamp_start);
	if (verbose > 12) {
		snd_pcm_sframes_t pdelay, cdelay;
		if ((err = snd_pcm_delay(play->handle, &pdelay)) < 0)
			snd_output_printf(loop->output, "%s: delay error: %s / %li / %li\n", play->id, snd_strerror(err), play->buf_size, play->buf_count);
		else
			snd_output_printf(loop->output, "%s: delay %li / %li / %li\n", play->id, pdelay, play->buf_size, play->buf_count);
		if ((err = snd_pcm_delay(capt->handle, &cdelay)) < 0)
			snd_output_printf(loop->output, "%s: delay error: %s / %li / %li\n", capt->id, snd_strerror(err), capt->buf_size, capt->buf_count);
		else
			snd_output_printf(loop->output, "%s: delay %li / %li / %li\n", capt->id, cdelay, capt->buf_size, capt->buf_count);
	}
	idx = 0;
	if (loop->running) {
		err = snd_pcm_poll_descriptors_revents(play->handle, fds,
						       play->pollfd_count,
						       &prevents);
		if (err < 0)
			return err;
		idx += play->pollfd_count;
		err = snd_pcm_poll_descriptors_revents(capt->handle, fds + idx,
						       capt->pollfd_count,
						       &crevents);
		if (err < 0)
			return err;
		idx += capt->pollfd_count;
		if (loop->xrun) {
			if (prevents || crevents) {
				loop->xrun_last_wake = loop->xrun_last_wake0;
				loop->xrun_last_wake0 = loop->tstamp_start;
			}
			loop->xrun_last_check = loop->xrun_last_check0;
			loop->xrun_last_check0 = loop->tstamp_start;
		}
	} else {
		prevents = crevents = 0;
	}
	if (play->ctl_pollfd_count > 0 &&
	    (loop->slave == SLAVE_TYPE_ON || loop->controls)) {
		err = snd_ctl_poll_descriptors_revents(play->ctl, fds + idx,
						       play->ctl_pollfd_count,
						       &events);
		if (err < 0)
			return err;
		if (events) {
			err = handle_ctl_events(play, events);
			if (err == 1)
				return 0;
			if (err < 0)
				return err;
		}
		idx += play->ctl_pollfd_count;
	}
	if (capt->ctl_pollfd_count > 0 &&
	    (loop->slave == SLAVE_TYPE_ON || loop->controls)) {
		err = snd_ctl_poll_descriptors_revents(capt->ctl, fds + idx,
						       capt->ctl_pollfd_count,
						       &events);
		if (err < 0)
			return err;
		if (events) {
			err = handle_ctl_events(capt, events);
			if (err == 1)
				return 0;
			if (err < 0)
				return err;
		}
		idx += capt->ctl_pollfd_count;
	}
	if (verbose > 9)
		snd_output_printf(loop->output, "%s: prevents = 0x%x, crevents = 0x%x\n", loop->id, prevents, crevents);
	if (!loop->running)
		goto __pcm_end;
	do {
		ccount = readit(capt);
		buf_add(loop, ccount);
		if (capt->xrun_pending || loop->reinit)
			break;
		/* we read new samples, if we have a room in the playback
		   buffer, feed them there */
		pcount = writeit(play);
		buf_remove(loop, pcount);
		if (play->xrun_pending || loop->reinit)
			break;
		loopcount--;
	} while ((ccount > 0 || pcount > 0) && loopcount > 0);
	if (play->xrun_pending || capt->xrun_pending) {
		if ((err = xrun_sync(loop)) < 0)
			return err;
	}
	if (loop->reinit) {
		err = pcmjob_stop(loop);
		if (err < 0)
			return err;
		err = pcmjob_start(loop);
		if (err < 0)
			return err;
	}
	if (loop->sync != SYNC_TYPE_NONE &&
	    play->counter >= play->sync_point &&
	    capt->counter >= play->sync_point) {
		snd_pcm_sframes_t diff, lat = get_whole_latency(loop);
		diff = ((double)(((double)play->total_queued * play->pitch) +
				 ((double)capt->total_queued * capt->pitch)) /
			(double)loop->total_queued_count) - lat;
		/* FIXME: this algorithm may be slightly better */
		if (verbose > 3)
			snd_output_printf(loop->output, "%s: sync diff %li old diff %li\n", loop->id, diff, loop->pitch_diff);
		if (diff > 0) {
			if (diff == loop->pitch_diff)
				loop->pitch += loop->pitch_delta;
			else if (diff > loop->pitch_diff)
				loop->pitch += loop->pitch_delta*2;
		} else if (diff < 0) {
			if (diff == loop->pitch_diff)
				loop->pitch -= loop->pitch_delta;
			else if (diff < loop->pitch_diff)
				loop->pitch -= loop->pitch_delta*2;
		}
		loop->pitch_diff = diff;
		if (loop->pitch_diff_min > diff)
			loop->pitch_diff_min = diff;
		if (loop->pitch_diff_max < diff)
			loop->pitch_diff_max = diff;
		update_pitch(loop);
		play->counter -= play->sync_point;
		capt->counter -= play->sync_point;
		play->total_queued = 0;
		capt->total_queued = 0;
		loop->total_queued_count = 0;
	}
	if (loop->sync != SYNC_TYPE_NONE) {
		snd_pcm_sframes_t pqueued, cqueued;
		pqueued = get_queued_playback_samples(loop);
		cqueued = get_queued_capture_samples(loop);
		if (verbose > 4)
			snd_output_printf(loop->output, "%s: queued %li/%li samples\n", loop->id, pqueued, cqueued);
		if (pqueued > 0)
			play->total_queued += pqueued;
		if (cqueued > 0)
			capt->total_queued += cqueued;
		if (pqueued > 0 || cqueued > 0)
			loop->total_queued_count += 1;
	}
	if (verbose > 12) {
		snd_pcm_sframes_t pdelay, cdelay;
		if ((err = snd_pcm_delay(play->handle, &pdelay)) < 0)
			snd_output_printf(loop->output, "%s: end delay error: %s / %li / %li\n", play->id, snd_strerror(err), play->buf_size, play->buf_count);
		else
			snd_output_printf(loop->output, "%s: end delay %li / %li / %li\n", play->id, pdelay, play->buf_size, play->buf_count);
		if ((err = snd_pcm_delay(capt->handle, &cdelay)) < 0)
			snd_output_printf(loop->output, "%s: end delay error: %s / %li / %li\n", capt->id, snd_strerror(err), capt->buf_size, capt->buf_count);
		else
			snd_output_printf(loop->output, "%s: end delay %li / %li / %li\n", capt->id, cdelay, capt->buf_size, capt->buf_count);
	}
      __pcm_end:
	if (verbose > 13 || loop->xrun) {
		long diff;
		getcurtimestamp(&loop->tstamp_end);
		diff = timediff(loop->tstamp_end, loop->tstamp_start);
		if (verbose > 13)
			snd_output_printf(loop->output, "%s: processing time %lius\n", loop->id, diff);
		if (loop->xrun && loop->xrun_max_proctime < diff)
			loop->xrun_max_proctime = diff;
	}
	return 0;
}

#define OUT(args...) \
	snd_output_printf(loop->state, ##args)

static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void show_handle(struct loopback_handle *lhandle, const char *id)
{
	struct loopback *loop = lhandle->loopback;

	OUT("  %s: %s:\n", id, lhandle->id);
	OUT("    device = '%s', ctldev '%s'\n", lhandle->device, lhandle->ctldev);
	OUT("    card_number = %i\n", lhandle->card_number);
	if (!loop->running)
		return;
	OUT("    access = %s, format = %s, rate = %u, channels = %u\n", snd_pcm_access_name(lhandle->access), snd_pcm_format_name(lhandle->format), lhandle->rate, lhandle->channels);
	OUT("    buffer_size = %u, period_size = %u, avail_min = %li\n", lhandle->buffer_size, lhandle->period_size, lhandle->avail_min);
	OUT("    xrun_pending = %i\n", lhandle->xrun_pending);
	OUT("    buf_size = %li, buf_pos = %li, buf_count = %li, buf_over = %li\n", lhandle->buf_size, lhandle->buf_pos, lhandle->buf_count, lhandle->buf_over);
	OUT("    pitch = %.8f\n", lhandle->pitch);
}

void pcmjob_state(struct loopback *loop)
{
	pthread_t self = pthread_self();
	pthread_mutex_lock(&state_mutex);
	OUT("State dump for thread %p job %i: %s:\n", (void *)self, loop->thread, loop->id);
	OUT("  running = %i\n", loop->running);
	OUT("  sync = %i\n", loop->sync);
	OUT("  slave = %i\n", loop->slave);
	if (!loop->running)
		goto __skip;
	OUT("  pollfd_count = %i\n", loop->pollfd_count);
	OUT("  pitch = %.8f, delta = %.8f, diff = %li, min = %li, max = %li\n", loop->pitch, loop->pitch_delta, loop->pitch_diff, loop->pitch_diff_min, loop->pitch_diff_max);
	OUT("  use_samplerate = %i\n", loop->use_samplerate);
      __skip:
	show_handle(loop->play, "playback");
	show_handle(loop->capt, "capture");
	pthread_mutex_unlock(&state_mutex);
}
