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
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include <tinyalsa/asoundlib.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"
#include "tinyalsa.h"
#include "latencytest.h"

struct format_map_table {
	enum _bat_pcm_format format_bat;
	enum pcm_format format_tiny;
};

static struct format_map_table map_tables[] = {
	{ BAT_PCM_FORMAT_S16_LE, PCM_FORMAT_S16_LE },
	{ BAT_PCM_FORMAT_S32_LE, PCM_FORMAT_S32_LE },
	{ BAT_PCM_FORMAT_MAX, },
};

static int format_convert(struct bat *bat, struct pcm_config *config)
{
	struct format_map_table *t = map_tables;

	for (; t->format_bat != BAT_PCM_FORMAT_MAX; t++) {
		if (t->format_bat == bat->format) {
			config->format = t->format_tiny;
			return 0;
		}
	}
	fprintf(bat->err, _("Invalid format!\n"));
	return -EINVAL;
}

static int init_config(struct bat *bat, struct pcm_config *config)
{
	config->channels = bat->channels;
	config->rate = bat->rate;
	if (bat->period_size > 0)
		config->period_size = bat->period_size;
	else
		config->period_size = TINYALSA_PERIODSIZE;
	config->period_count = 4;
	config->start_threshold = 0;
	config->stop_threshold = 0;
	config->silence_threshold = 0;

	return format_convert(bat, config);
}

/**
 * Called when thread is finished
 */
static void close_handle(void *handle)
{
	struct pcm *pcm = handle;

	if (NULL != pcm)
		pcm_close(pcm);
}

/**
 * Check that a parameter is inside bounds
 */
static int check_param(struct bat *bat, struct pcm_params *params,
		unsigned int param, unsigned int value,
		char *param_name, char *param_unit)
{
	unsigned int min;
	unsigned int max;
	int ret = 0;

	min = pcm_params_get_min(params, param);
	if (value < min) {
		fprintf(bat->err,
			_("%s is %u%s, device only supports >= %u%s!\n"),
			param_name, value, param_unit, min, param_unit);
		ret = -EINVAL;
	}

	max = pcm_params_get_max(params, param);
	if (value > max) {
		fprintf(bat->err,
			_("%s is %u%s, device only supports <= %u%s!\n"),
			param_name, value, param_unit, max, param_unit);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * Check all parameters
 */
static int check_playback_params(struct bat *bat,
		struct pcm_config *config)
{
	struct pcm_params *params;
	unsigned int card = bat->playback.card_tiny;
	unsigned int device = bat->playback.device_tiny;
	int err = 0;

	params = pcm_params_get(card, device, PCM_OUT);
	if (params == NULL) {
		fprintf(bat->err, _("Unable to open PCM device %u!\n"),
				device);
		return -EINVAL;
	}

	err = check_param(bat, params, PCM_PARAM_RATE,
			config->rate, "Sample rate", "Hz");
	if (err < 0)
		goto exit;
	err = check_param(bat, params, PCM_PARAM_CHANNELS,
			config->channels, "Sample", " channels");
	if (err < 0)
		goto exit;
	err = check_param(bat, params, PCM_PARAM_SAMPLE_BITS,
			bat->sample_size * 8, "Bitrate", " bits");
	if (err < 0)
		goto exit;
	err = check_param(bat, params, PCM_PARAM_PERIOD_SIZE,
			config->period_size, "Period size", "Hz");
	if (err < 0)
		goto exit;
	err = check_param(bat, params, PCM_PARAM_PERIODS,
			config->period_count, "Period count", "Hz");
	if (err < 0)
		goto exit;

exit:
	pcm_params_free(params);

	return err;
}

/**
 * Process output data for latency test
 */
static int latencytest_process_output(struct bat *bat, struct pcm *pcm,
		void *buffer, int bytes)
{
	int err = 0;
	int frames = bytes / bat->frame_size;

	fprintf(bat->log, _("Play sample with %d frames buffer\n"), frames);

	bat->latency.is_playing = true;

	while (1) {
		/* generate output data */
		err = handleoutput(bat, buffer, bytes, frames);
		if (err != 0)
			break;

		err = pcm_write(pcm, buffer, bytes);
		if (err != 0)
			break;

		if (bat->latency.state == LATENCY_STATE_COMPLETE_SUCCESS)
			break;

		bat->periods_played++;
	}

	bat->latency.is_playing = false;

	return err;
}

/**
 * Play sample
 */
static int play_sample(struct bat *bat, struct pcm *pcm,
		void *buffer, int bytes)
{
	int err = 0;
	int frames = bytes / bat->frame_size;
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
		/* leave space for file header */
		if (fseek(fp, sizeof(struct wav_container), SEEK_SET) != 0) {
			err = -errno;
			fclose(fp);
			return err;
		}
	}

	while (1) {
		err = generate_input_data(bat, buffer, bytes, frames);
		if (err != 0)
			break;

		if (bat->debugplay) {
			if (fwrite(buffer, 1, bytes, fp) != bytes) {
				err = -EIO;
				break;
			}
			bytes_total += bytes;
		}

		bat->periods_played++;
		if (bat->period_is_limited
				&& bat->periods_played >= bat->periods_total)
			break;

		err = pcm_write(pcm, buffer, bytes);
		if (err != 0)
			break;
	}

	if (bat->debugplay) {
		update_wav_header(bat, fp, bytes_total);
		fclose(fp);
	}
	return err;
}

static int get_tiny_device(struct bat *bat, char *alsa_device,
		unsigned int *tiny_card, unsigned int *tiny_device)
{
	char *tmp1, *tmp2, *tmp3;

	if (alsa_device == NULL)
		goto fail;

	tmp1 = strchr(alsa_device, ':');
	if (tmp1 == NULL)
		goto fail;

	tmp3 = tmp1 + 1;
	tmp2 = strchr(tmp3, ',');
	if (tmp2 == NULL)
		goto fail;

	tmp1 = tmp2 + 1;
	*tiny_device = atoi(tmp1);
	*tmp2 = '\0';
	*tiny_card = atoi(tmp3);
	*tmp2 = ',';

	return 0;
fail:
	fprintf(bat->err, _("Invalid tiny device: %s\n"), alsa_device);
	return -EINVAL;
}

/**
 * Play
 */
void *playback_tinyalsa(struct bat *bat)
{
	int err = 0;
	struct pcm_config config;
	struct pcm *pcm = NULL;
	void *buffer = NULL;
	int bufbytes;

	fprintf(bat->log, _("Entering playback thread (tinyalsa).\n"));

	retval_play = 0;

	/* init device */
	err = get_tiny_device(bat, bat->playback.device,
			&bat->playback.card_tiny,
			&bat->playback.device_tiny);
	if (err < 0) {
		retval_play = err;
		goto exit1;
	}

	/* init config */
	err = init_config(bat, &config);
	if (err < 0) {
		retval_play = err;
		goto exit1;
	}

	/* check param before open device */
	err = check_playback_params(bat, &config);
	if (err < 0) {
		retval_play = err;
		goto exit1;
	}

	/* open device */
	pcm = pcm_open(bat->playback.card_tiny, bat->playback.device_tiny,
			PCM_OUT, &config);
	if (!pcm || !pcm_is_ready(pcm)) {
		fprintf(bat->err, _("Unable to open PCM device %u (%s)!\n"),
				bat->playback.device_tiny, pcm_get_error(pcm));
		retval_play = -EINVAL;
		goto exit1;
	}

	/* init buffer */
	bufbytes = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	buffer = malloc(bufbytes);
	if (!buffer) {
		retval_play = -ENOMEM;
		goto exit2;
	}

	/* init playback source */
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
		err = latencytest_process_output(bat, pcm, buffer, bufbytes);
	else
		err = play_sample(bat, pcm, buffer, bufbytes);
	if (err < 0) {
		retval_play = err;
		goto exit4;
	}

exit4:
	if (bat->playback.file)
		fclose(bat->fp);
exit3:
	free(buffer);
exit2:
	pcm_close(pcm);
exit1:
	pthread_exit(&retval_play);
}

/**
 * Capture sample
 */
static int capture_sample(struct bat *bat, struct pcm *pcm,
		void *buffer, unsigned int bytes)
{
	int err = 0;
	FILE *fp = NULL;
	unsigned int bytes_read = 0;
	unsigned int bytes_count = bat->frames * bat->frame_size;

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

	while (bytes_read < bytes_count && !pcm_read(pcm, buffer, bytes)) {
		if (fwrite(buffer, 1, bytes, fp) != bytes)
			break;

		bytes_read += bytes;

		bat->periods_played++;

		if (bat->period_is_limited
				&& bat->periods_played >= bat->periods_total)
			break;
	}

	err = update_wav_header(bat, fp, bytes_read);

	fclose(fp);
	return err;
}

/**
 * Process input data for latency test
 */
static int latencytest_process_input(struct bat *bat, struct pcm *pcm,
		void *buffer, unsigned int bytes)
{
	int err = 0;
	FILE *fp = NULL;
	unsigned int bytes_read = 0;
	unsigned int bytes_count = bat->frames * bat->frame_size;

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

	bat->latency.is_capturing = true;

	while (bytes_read < bytes_count && !pcm_read(pcm, buffer, bytes)) {
		if (fwrite(buffer, 1, bytes, fp) != bytes)
			break;

		err = handleinput(bat, buffer, bytes / bat->frame_size);
		if (err != 0)
			break;

		if (bat->latency.is_playing == false)
			break;

		bytes_read += bytes;
	}

	bat->latency.is_capturing = false;

	err = update_wav_header(bat, fp, bytes_read);

	fclose(fp);
	return err;
}

/**
 * Record
 */
void *record_tinyalsa(struct bat *bat)
{
	int err = 0;
	struct pcm_config config;
	struct pcm *pcm;
	void *buffer;
	unsigned int bufbytes;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	fprintf(bat->log, _("Entering capture thread (tinyalsa).\n"));

	retval_record = 0;

	/* init device */
	err = get_tiny_device(bat, bat->capture.device,
			&bat->capture.card_tiny,
			&bat->capture.device_tiny);
	if (err < 0) {
		retval_record = err;
		goto exit1;
	}

	/* init config */
	err = init_config(bat, &config);
	if (err < 0) {
		retval_record = err;
		goto exit1;
	}

	/* open device */
	pcm = pcm_open(bat->capture.card_tiny, bat->capture.device_tiny,
			PCM_IN, &config);
	if (!pcm || !pcm_is_ready(pcm)) {
		fprintf(bat->err, _("Unable to open PCM device (%s)!\n"),
				pcm_get_error(pcm));
		retval_record = -EINVAL;
		goto exit1;
	}

	/* init buffer */
	bufbytes = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	buffer = malloc(bufbytes);
	if (!buffer) {
		retval_record = -ENOMEM;
		goto exit2;
	}

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push(close_handle, pcm);
	pthread_cleanup_push(free, buffer);

	fprintf(bat->log, _("Recording ...\n"));
	if (bat->roundtriplatency)
		err = latencytest_process_input(bat, pcm, buffer, bufbytes);
	else
		err = capture_sample(bat, pcm, buffer, bufbytes);
	if (err != 0) {
		retval_record = err;
		goto exit3;
	}

	/* Normally we will never reach this part of code (unless error in
	 * previous call) (before exit3) as this thread will be cancelled
	 *  by end of play thread. Except in single line mode. */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	pthread_exit(&retval_record);

exit3:
	free(buffer);
exit2:
	pcm_close(pcm);
exit1:
	pthread_exit(&retval_record);
}
