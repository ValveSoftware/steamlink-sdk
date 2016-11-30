/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#include <alsa/asoundlib.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"
#include "alsa.h"
#include "latencytest.h"

struct pcm_container {
	snd_pcm_t *handle;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	snd_pcm_format_t format;
	unsigned short channels;
	size_t period_bytes;
	size_t sample_bits;
	size_t frame_bits;
	char *buffer;
};

struct format_map_table {
	enum _bat_pcm_format format_bat;
	snd_pcm_format_t format_alsa;
};

static struct format_map_table map_tables[] = {
	{ BAT_PCM_FORMAT_UNKNOWN, SND_PCM_FORMAT_UNKNOWN },
	{ BAT_PCM_FORMAT_U8, SND_PCM_FORMAT_U8 },
	{ BAT_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S16_LE },
	{ BAT_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S24_3LE },
	{ BAT_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S32_LE },
	{ BAT_PCM_FORMAT_MAX, },
};

static int format_convert(struct bat *bat, snd_pcm_format_t *fmt)
{
	struct format_map_table *t = map_tables;

	for (; t->format_bat != BAT_PCM_FORMAT_MAX; t++) {
		if (t->format_bat == bat->format) {
			*fmt = t->format_alsa;
			return 0;
		}
	}
	fprintf(bat->err, _("Invalid format!\n"));
	return -EINVAL;
}

static int set_snd_pcm_params(struct bat *bat, struct pcm_container *sndpcm)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_format_t format;
	unsigned int buffer_time = 0;
	unsigned int period_time = 0;
	snd_pcm_uframes_t buffer_size = 0;
	snd_pcm_uframes_t period_size = 0;
	unsigned int rate;
	int err;
	const char *device_name = snd_pcm_name(sndpcm->handle);

	/* Convert common format to ALSA format */
	err = format_convert(bat, &format);
	if (err != 0)
		return err;

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	err = snd_pcm_hw_params_any(sndpcm->handle, params);
	if (err < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("default params: %s: %s(%d)\n"),
				device_name, snd_strerror(err), err);
		return err;
	}

	/* Set access mode */
	err = snd_pcm_hw_params_set_access(sndpcm->handle, params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("access type: %s: %s(%d)\n"),
				device_name, snd_strerror(err), err);
		return err;
	}

	/* Set format */
	err = snd_pcm_hw_params_set_format(sndpcm->handle, params, format);
	if (err < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("PCM format: %d %s: %s(%d)\n"), format,
				device_name, snd_strerror(err), err);
		return err;
	}

	/* Set channels */
	err = snd_pcm_hw_params_set_channels(sndpcm->handle,
			params, bat->channels);
	if (err < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("channel number: %d %s: %s(%d)\n"),
				bat->channels,
				device_name, snd_strerror(err), err);
		return err;
	}

	/* Set sampling rate */
	rate = bat->rate;
	err = snd_pcm_hw_params_set_rate_near(sndpcm->handle,
			params, &bat->rate,
			0);
	if (err < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("sample rate: %d %s: %s(%d)\n"),
				bat->rate,
				device_name, snd_strerror(err), err);
		return err;
	}
	if ((float) rate * (1 + RATE_RANGE) < bat->rate
			|| (float) rate * (1 - RATE_RANGE) > bat->rate) {
		fprintf(bat->err, _("Invalid parameters: sample rate: "));
		fprintf(bat->err, _("requested %dHz, got %dHz\n"),
				rate, bat->rate);
		return -EINVAL;
	}

	if (bat->buffer_size > 0 && bat->period_size == 0)
		bat->period_size = bat->buffer_size / DIV_BUFFERSIZE;

	if (bat->roundtriplatency && bat->buffer_size == 0) {
		/* Set to minimum buffer size and period size
		   for latency test */
		if (snd_pcm_hw_params_get_buffer_size_min(params,
				&buffer_size) < 0) {
			fprintf(bat->err,
					_("Get parameter from device error: "));
			fprintf(bat->err, _("buffer size min: %d %s: %s(%d)\n"),
					(int) buffer_size,
					device_name, snd_strerror(err), err);
			return -EINVAL;
		}

		if (snd_pcm_hw_params_get_period_size_min(params,
				&period_size, 0) < 0) {
			fprintf(bat->err,
					_("Get parameter from device error: "));
			fprintf(bat->err, _("period size min: %d %s: %s(%d)\n"),
					(int) period_size,
					device_name, snd_strerror(err), err);
			return -EINVAL;
		}
		bat->buffer_size = (int) buffer_size;
		bat->period_size = (int) period_size;
	}

	if (bat->buffer_size > 0) {
		buffer_size = bat->buffer_size;
		period_size = bat->period_size;

		fprintf(bat->log, _("Set period size: %d  buffer size: %d\n"),
				(int) period_size, (int) buffer_size);

		err = snd_pcm_hw_params_set_buffer_size_near(sndpcm->handle,
				params, &buffer_size);
		if (err < 0) {
			fprintf(bat->err, _("Set parameter to device error: "));
			fprintf(bat->err, _("buffer size: %d %s: %s(%d)\n"),
					(int) buffer_size,
					device_name, snd_strerror(err), err);
			return err;
		}

		err = snd_pcm_hw_params_set_period_size_near(sndpcm->handle,
				params, &period_size, 0);
		if (err < 0) {
			fprintf(bat->err, _("Set parameter to device error: "));
			fprintf(bat->err, _("period size: %d %s: %s(%d)\n"),
					(int) period_size,
					device_name, snd_strerror(err), err);
			return err;
		}
	} else {
		if (snd_pcm_hw_params_get_buffer_time_max(params,
				&buffer_time, 0) < 0) {
			fprintf(bat->err,
					_("Get parameter from device error: "));
			fprintf(bat->err, _("buffer time: %d %s: %s(%d)\n"),
					buffer_time,
					device_name, snd_strerror(err), err);
			return -EINVAL;
		}

		if (buffer_time > MAX_BUFFERTIME)
			buffer_time = MAX_BUFFERTIME;

		period_time = buffer_time / DIV_BUFFERTIME;

		/* Set buffer time and period time */
		err = snd_pcm_hw_params_set_buffer_time_near(sndpcm->handle,
				params, &buffer_time, 0);
		if (err < 0) {
			fprintf(bat->err, _("Set parameter to device error: "));
			fprintf(bat->err, _("buffer time: %d %s: %s(%d)\n"),
					buffer_time,
					device_name, snd_strerror(err), err);
			return err;
		}

		err = snd_pcm_hw_params_set_period_time_near(sndpcm->handle,
				params, &period_time, 0);
		if (err < 0) {
			fprintf(bat->err, _("Set parameter to device error: "));
			fprintf(bat->err, _("period time: %d %s: %s(%d)\n"),
					period_time,
					device_name, snd_strerror(err), err);
			return err;
		}
	}

	/* Write the parameters to the driver */
	if (snd_pcm_hw_params(sndpcm->handle, params) < 0) {
		fprintf(bat->err, _("Set parameter to device error: "));
		fprintf(bat->err, _("hw params: %s: %s(%d)\n"),
				device_name, snd_strerror(err), err);
		return -EINVAL;
	}

	err = snd_pcm_hw_params_get_period_size(params,
			&sndpcm->period_size, 0);
	if (err < 0) {
		fprintf(bat->err, _("Get parameter from device error: "));
		fprintf(bat->err, _("period size: %zd %s: %s(%d)\n"),
				sndpcm->period_size,
				device_name, snd_strerror(err), err);
		return err;
	}

	err = snd_pcm_hw_params_get_buffer_size(params, &sndpcm->buffer_size);
	if (err < 0) {
		fprintf(bat->err, _("Get parameter from device error: "));
		fprintf(bat->err, _("buffer size: %zd %s: %s(%d)\n"),
				sndpcm->buffer_size,
				device_name, snd_strerror(err), err);
		return err;
	}

	if (sndpcm->period_size == sndpcm->buffer_size) {
		fprintf(bat->err, _("Invalid parameters: can't use period "));
		fprintf(bat->err, _("equal to buffer size (%zd)\n"),
				sndpcm->period_size);
		return -EINVAL;
	}

	fprintf(bat->log, _("Get period size: %d  buffer size: %d\n"),
			(int) sndpcm->period_size, (int) sndpcm->buffer_size);

	err = snd_pcm_format_physical_width(format);
	if (err < 0) {
		fprintf(bat->err, _("Invalid parameters: "));
		fprintf(bat->err, _("snd_pcm_format_physical_width: %d\n"),
				err);
		return err;
	}
	sndpcm->sample_bits = err;

	sndpcm->frame_bits = sndpcm->sample_bits * bat->channels;

	/* Calculate the period bytes */
	sndpcm->period_bytes = sndpcm->period_size * sndpcm->frame_bits / 8;
	sndpcm->buffer = (char *) malloc(sndpcm->period_bytes);
	if (sndpcm->buffer == NULL) {
		fprintf(bat->err, _("Not enough memory: size=%zd\n"),
				sndpcm->period_bytes);
		return -ENOMEM;
	}

	return 0;
}

static int write_to_pcm(const struct pcm_container *sndpcm,
		int frames, struct bat *bat)
{
	int err;
	int offset = 0;
	int remain = frames;

	while (remain > 0) {
		err = snd_pcm_writei(sndpcm->handle, sndpcm->buffer + offset,
				remain);
		if (err == -EAGAIN || (err >= 0 && err < frames)) {
			snd_pcm_wait(sndpcm->handle, 500);
		} else if (err == -EPIPE) {
			fprintf(bat->err, _("Underrun: %s(%d)\n"),
					snd_strerror(err), err);
			if (bat->roundtriplatency)
				bat->latency.xrun_error = true;
			snd_pcm_prepare(sndpcm->handle);
		} else if (err < 0) {
			fprintf(bat->err, _("Write PCM device error: %s(%d)\n"),
					snd_strerror(err), err);
			return err;
		}

		if (err > 0) {
			remain -= err;
			offset += err * sndpcm->frame_bits / 8;
		}
	}

	return 0;
}

/**
 * Process output data for latency test
 */
static int latencytest_process_output(struct pcm_container *sndpcm,
		struct bat *bat)
{
	int err = 0;
	int bytes = sndpcm->period_bytes; /* playback buffer size */
	int frames = sndpcm->period_size; /* frame count */

	bat->latency.is_playing = true;

	while (1) {
		/* generate output data */
		err = handleoutput(bat, sndpcm->buffer, bytes, frames);
		if (err != 0)
			break;

		err = write_to_pcm(sndpcm, frames, bat);
		if (err != 0)
			break;

		/* Xrun error, terminate the playback thread*/
		if (bat->latency.xrun_error == true)
			break;

		if (bat->latency.state == LATENCY_STATE_COMPLETE_SUCCESS)
			break;

		bat->periods_played++;
	}

	bat->latency.is_playing = false;

	return err;
}

static int write_to_pcm_loop(struct pcm_container *sndpcm, struct bat *bat)
{
	int err = 0;
	int bytes = sndpcm->period_bytes; /* playback buffer size */
	int frames = bytes * 8 / sndpcm->frame_bits; /* frame count */
	FILE *fp = NULL;
	int bytes_total = 0;

	if (bat->debugplay) {
		fp = fopen(bat->debugplay, "wb");
		err = -errno;
		if (fp == NULL) {
			fprintf(bat->err, _("Cannot open file: %s %d\n"),
					bat->debugplay, err);
			return err;
		}
		/* leave space for wav header */
		if (fseek(fp, sizeof(struct wav_container), SEEK_SET) != 0) {
			err = -errno;
			fclose(fp);
			return err;
		}
	}

	while (1) {
		err = generate_input_data(bat, sndpcm->buffer, bytes, frames);
		if (err != 0)
			break;

		if (bat->debugplay) {
			if (fwrite(sndpcm->buffer, 1, bytes, fp) != bytes) {
				err = -EIO;
				break;
			}
			bytes_total += bytes;
		}

		bat->periods_played++;
		if (bat->period_is_limited
				&& bat->periods_played >= bat->periods_total)
			break;

		err = write_to_pcm(sndpcm, frames, bat);
		if (err != 0)
			break;
	}

	if (bat->debugplay) {
		update_wav_header(bat, fp, bytes_total);
		fclose(fp);
	}

	snd_pcm_drain(sndpcm->handle);

	return err;
}

/**
 * Play
 */
void *playback_alsa(struct bat *bat)
{
	int err = 0;
	struct pcm_container sndpcm;

	fprintf(bat->log, _("Entering playback thread (ALSA).\n"));

	retval_play = 0;
	memset(&sndpcm, 0, sizeof(sndpcm));

	err = snd_pcm_open(&sndpcm.handle, bat->playback.device,
			SND_PCM_STREAM_PLAYBACK, 0);
	if (err != 0) {
		fprintf(bat->err, _("Cannot open PCM playback device: "));
		fprintf(bat->err, _("%s(%d)\n"), snd_strerror(err), err);
		retval_play = err;
		goto exit1;
	}

	err = set_snd_pcm_params(bat, &sndpcm);
	if (err != 0) {
		retval_play = err;
		goto exit2;
	}

	if (bat->playback.file == NULL) {
		fprintf(bat->log, _("Playing generated audio sine wave"));
		bat->sinus_duration == 0 ?
			fprintf(bat->log, _(" endlessly\n")) :
			fprintf(bat->log, _("\n"));
	} else {
		fprintf(bat->log, _("Playing input audio file: %s\n"),
				bat->playback.file);
		bat->fp = fopen(bat->playback.file, "rb");
		err = -errno;
		if (bat->fp == NULL) {
			fprintf(bat->err, _("Cannot open file: %s %d\n"),
					bat->playback.file, err);
			retval_play = err;
			goto exit3;
		}
		/* Skip header */
		err = read_wav_header(bat, bat->playback.file, bat->fp, true);
		if (err != 0) {
			retval_play = err;
			goto exit4;
		}
	}

	if (bat->roundtriplatency)
		err = latencytest_process_output(&sndpcm, bat);
	else
		err = write_to_pcm_loop(&sndpcm, bat);
	if (err < 0) {
		retval_play = err;
		goto exit4;
	}

exit4:
	if (bat->playback.file)
		fclose(bat->fp);
exit3:
	free(sndpcm.buffer);
exit2:
	snd_pcm_close(sndpcm.handle);
exit1:
	pthread_exit(&retval_play);
}

static int read_from_pcm(struct pcm_container *sndpcm,
		int frames, struct bat *bat)
{
	int err = 0;
	int offset = 0;
	int remain = frames;

	while (remain > 0) {
		err = snd_pcm_readi(sndpcm->handle,
				sndpcm->buffer + offset, remain);
		if (err == -EAGAIN || (err >= 0 && err < remain)) {
			snd_pcm_wait(sndpcm->handle, 500);
		} else if (err == -EPIPE) {
			snd_pcm_prepare(sndpcm->handle);
			fprintf(bat->err, _("Overrun: %s(%d)\n"),
					snd_strerror(err), err);
			if (bat->roundtriplatency)
				bat->latency.xrun_error = true;
		} else if (err < 0) {
			fprintf(bat->err, _("Read PCM device error: %s(%d)\n"),
					snd_strerror(err), err);
			return err;
		}

		if (err > 0) {
			remain -= err;
			offset += err * sndpcm->frame_bits / 8;
		}
	}

	return 0;
}

static int read_from_pcm_loop(struct pcm_container *sndpcm, struct bat *bat)
{
	int err = 0;
	FILE *fp = NULL;
	int size, frames;
	int bytes_read = 0;
	int bytes_count = bat->frames * bat->frame_size;
	int remain = bytes_count;

	remove(bat->capture.file);
	fp = fopen(bat->capture.file, "wb");
	err = -errno;
	if (fp == NULL) {
		fprintf(bat->err, _("Cannot open file: %s %d\n"),
				bat->capture.file, err);
		return err;
	}
	/* leave space for file header */
	if (fseek(fp, sizeof(struct wav_container), SEEK_SET) != 0) {
		err = -errno;
		fclose(fp);
		return err;
	}

	while (remain > 0) {
		size = (remain <= sndpcm->period_bytes) ?
			remain : sndpcm->period_bytes;
		frames = size * 8 / sndpcm->frame_bits;

		/* read a chunk from pcm device */
		err = read_from_pcm(sndpcm, frames, bat);
		if (err != 0)
			break;

		/* write the chunk to file */
		if (fwrite(sndpcm->buffer, 1, size, fp) != size) {
			err = -EIO;
			break;
		}

		bytes_read += size;
		remain -= size;
		bat->periods_played++;

		if (bat->period_is_limited
				&& bat->periods_played >= bat->periods_total)
			break;
	}

	update_wav_header(bat, fp, bytes_read);

	fclose(fp);
	return err;
}

/**
 * Process input data for latency test
 */
static int latencytest_process_input(struct pcm_container *sndpcm,
		struct bat *bat)
{
	int err = 0;
	FILE *fp = NULL;
	int bytes_read = 0;
	int frames = sndpcm->period_size;
	int size = sndpcm->period_bytes;
	int bytes_count = bat->frames * bat->frame_size;

	remove(bat->capture.file);
	fp = fopen(bat->capture.file, "wb");
	err = -errno;
	if (fp == NULL) {
		fprintf(bat->err, _("Cannot open file: %s %d\n"),
				bat->capture.file, err);
		return err;
	}
	/* leave space for file header */
	if (fseek(fp, sizeof(struct wav_container), SEEK_SET) != 0) {
		fclose(fp);
		return err;
	}

	bat->latency.is_capturing = true;

	while (bytes_read < bytes_count) {
		/* read a chunk from pcm device */
		err = read_from_pcm(sndpcm, frames, bat);
		if (err != 0)
			break;

		/* Xrun error, terminate the capture thread*/
		if (bat->latency.xrun_error == true)
			break;

		err = handleinput(bat, sndpcm->buffer, frames);
		if (err != 0)
			break;

		if (bat->latency.is_playing == false)
			break;

		/* write the chunk to file */
		if (fwrite(sndpcm->buffer, 1, size, fp) != size) {
			err = -EIO;
			break;
		}

		bytes_read += size;
	}

	bat->latency.is_capturing = false;

	update_wav_header(bat, fp, bytes_read);

	fclose(fp);
	return err;
}


static void pcm_cleanup(void *p)
{
	snd_pcm_close(p);
}

/**
 * Record
 */
void *record_alsa(struct bat *bat)
{
	int err = 0;
	struct pcm_container sndpcm;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	fprintf(bat->log, _("Entering capture thread (ALSA).\n"));

	retval_record = 0;
	memset(&sndpcm, 0, sizeof(sndpcm));

	err = snd_pcm_open(&sndpcm.handle, bat->capture.device,
			SND_PCM_STREAM_CAPTURE, 0);
	if (err != 0) {
		fprintf(bat->err, _("Cannot open PCM capture device: "));
		fprintf(bat->err, _("%s(%d)\n"), snd_strerror(err), err);
		retval_record = err;
		goto exit1;
	}

	err = set_snd_pcm_params(bat, &sndpcm);
	if (err != 0) {
		retval_record = err;
		goto exit2;
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push(pcm_cleanup, sndpcm.handle);
	pthread_cleanup_push(free, sndpcm.buffer);

	fprintf(bat->log, _("Recording ...\n"));
	if (bat->roundtriplatency)
		err = latencytest_process_input(&sndpcm, bat);
	else
		err = read_from_pcm_loop(&sndpcm, bat);
	if (err != 0) {
		retval_record = err;
		goto exit3;
	}

	/* Normally we will never reach this part of code (unless error in
	 * previous call) (before exit3) as this thread will be cancelled
	 * by end of play thread. Except in single line mode. */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);

	snd_pcm_drain(sndpcm.handle);
	pthread_exit(&retval_record);

exit3:
	free(sndpcm.buffer);
exit2:
	snd_pcm_close(sndpcm.handle);
exit1:
	pthread_exit(&retval_record);
}
