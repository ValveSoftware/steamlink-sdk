/*
 * Copyright (C) 2015 Caleb Crome
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

/*
 * This is a general purpose sine wave generator that will stay stable
 * for a long time, and with a little renormalization, could stay stay
 * stable indefinitely
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "gettext.h"
#include "common.h"
#include "signal.h"

/*
 * Initialize the sine wave generator.
 * sin_generator:  gets initialized by this call.
 * frequency:      the frequency for the sine wave.  must be < 0.5*sample_rate
 * sample_rate:    the sample rate...
 * returns 0 on success, -1 on error.
 */
int sin_generator_init(struct sin_generator *sg, float magnitude,
		float frequency, float sample_rate)
{
	/* angular frequency:  cycles/sec / (samp/sec) * rad/cycle = rad/samp */
	float w = frequency / sample_rate * 2 * M_PI;
	if (frequency >= sample_rate / 2)
		return -1;
	sg->phasor_real = cos(w);
	sg->phasor_imag = sin(w);
	sg->magnitude   = magnitude;
	sg->state_real  = 0.0;
	sg->state_imag  = magnitude;
	sg->frequency = frequency;
	sg->sample_rate = sample_rate;
	return 0;
}

/*
 * Generates the next sample in the sine wave.
 * should be much faster than calling a sin function
 * if it's inlined and optimized.
 *
 * returns the next value.  no possibility of error.
 */
float sin_generator_next_sample(struct sin_generator *sg)
{
	/* get shorthand to pointers */
	const double pr = sg->phasor_real;
	const double pi = sg->phasor_imag;
	const double sr = sg->state_real;
	const double si = sg->state_imag;
	/* step the phasor -- complex multiply */
	sg->state_real = sr * pr - si * pi;
	sg->state_imag = sr * pi + pr * si;
	/* return the input value so sine wave starts at exactly 0.0 */
	return (float)sr;
}

/* fills a vector with a sine wave */
void sin_generator_vfill(struct sin_generator *sg, float *buf, int n)
{
	int i;
	for (i = 0; i < n; i++)
		*buf++ = sin_generator_next_sample(sg);
}

static int reorder(struct bat *bat, float *val, int frames)
{
	float *new_buf = NULL;
	int i, c, bytes;

	bytes = frames * bat->channels * sizeof(float);

	new_buf = (float *) malloc(bytes);
	if (new_buf == NULL) {
		fprintf(bat->err, _("Not enough memory.\n"));
		return -ENOMEM;
	}

	memcpy(new_buf, val, bytes);
	for (i = 0; i < frames; i++)
		for (c = 0; c < bat->channels; c++)
			val[i * bat->channels + c] =
				new_buf[c * frames + i];
	free(new_buf);

	return 0;
}

static int adjust_waveform(struct bat *bat, float *val, int frames,
		int channels)
{
	int i, nsamples, max;
	float factor, offset = 0.0;

	switch (bat->format) {
	case BAT_PCM_FORMAT_U8:
		max = INT8_MAX;
		offset = max;	/* shift for unsigned format */
		break;
	case BAT_PCM_FORMAT_S16_LE:
		max  = INT16_MAX;
		break;
	case BAT_PCM_FORMAT_S24_3LE:
		max = (1 << 23) - 1;
		break;
	case BAT_PCM_FORMAT_S32_LE:
		max = INT32_MAX;
		break;
	default:
		fprintf(bat->err, _("Invalid PCM format: %d\n"), bat->format);
		return -EINVAL;
	}

	factor = max * RANGE_FACTOR;
	nsamples = channels * frames;

	for (i = 0; i < nsamples; i++)
		val[i] = val[i] * factor + offset;

	return 0;
}

int generate_sine_wave(struct bat *bat, int frames, void *buf)
{
	int err = 0;
	int c, nsamples;
	float *sinus_f = NULL;
	static struct sin_generator sg[MAX_CHANNELS];

	nsamples = bat->channels * frames;
	sinus_f = (float *) malloc(nsamples * sizeof(float));
	if (sinus_f == NULL) {
		fprintf(bat->err, _("Not enough memory.\n"));
		return -ENOMEM;
	}

	for (c = 0; c < bat->channels; c++) {
		/* initialize static struct at the first time */
		if (sg[c].frequency != bat->target_freq[c])
			sin_generator_init(&sg[c], 1.0, bat->target_freq[c],
					bat->rate);
		/* fill buffer for each channel */
		sin_generator_vfill(&sg[c], sinus_f + c * frames, frames);
	}

	/* reorder samples to interleaved mode */
	err = reorder(bat, sinus_f, frames);
	if (err != 0)
		goto exit;

	/* adjust amplitude and offset of waveform */
	err = adjust_waveform(bat, sinus_f, frames, bat->channels);
	if (err != 0)
		goto exit;

	bat->convert_float_to_sample(sinus_f, buf, frames, bat->channels);

exit:
	free(sinus_f);

	return err;
}

/* generate single channel sine waveform without sample conversion */
int generate_sine_wave_raw_mono(struct bat *bat, float *buf,
		float freq, int nsamples)
{
	int err = 0;
	struct sin_generator sg;

	err = sin_generator_init(&sg, 1.0, freq, bat->rate);
	if (err < 0)
		return err;
	sin_generator_vfill(&sg, buf, nsamples);

	/* adjust amplitude and offset of waveform */
	err = adjust_waveform(bat, buf, nsamples, 1);

	return err;
}
