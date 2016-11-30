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
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <math.h>
#include <fftw3.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"
#include "bat-signal.h"

static void check_amplitude(struct bat *bat, float *buf)
{
	float sum, average, amplitude;
	int i, percent;

	/* calculate average value */
	for (i = 0, sum = 0.0; i < bat->frames; i++)
		sum += buf[i];
	average = sum / bat->frames;

	/* calculate peak-to-average amplitude */
	for (i = 0, sum = 0.0; i < bat->frames; i++)
		sum += fabsf(buf[i] - average);
	amplitude = sum / bat->frames * M_PI / 2.0;

	/* calculate amplitude percentage against full range */
	percent = amplitude * 100 / ((1 << ((bat->sample_size << 3) - 1)) - 1);

	fprintf(bat->log, _("Amplitude: %.1f; Percentage: [%d]\n"),
			amplitude, percent);
	if (percent < 0)
		fprintf(bat->err, _("ERROR: Amplitude can't be negative!\n"));
	else if (percent < 1)
		fprintf(bat->err, _("WARNING: Signal too weak!\n"));
	else if (percent > 100)
		fprintf(bat->err, _("WARNING: Signal overflow!\n"));
}

/**
 *
 * @return 0 if peak detected at right frequency,
 *         1 if peak detected somewhere else
 *         2 if DC detected
 */
int check_peak(struct bat *bat, struct analyze *a, int end, int peak, float hz,
		float mean, float p, int channel, int start)
{
	int err;
	float hz_peak = (float) (peak) * hz;
	float delta_rate = DELTA_RATE * bat->target_freq[channel];
	float delta_HZ = DELTA_HZ;
	float tolerance = (delta_rate > delta_HZ) ? delta_rate : delta_HZ;

	fprintf(bat->log, _("Detected peak at %2.2f Hz of %2.2f dB\n"), hz_peak,
			10.0 * log10f(a->mag[peak] / mean));
	fprintf(bat->log, _(" Total %3.1f dB from %2.2f to %2.2f Hz\n"),
			10.0 * log10f(p / mean), start * hz, end * hz);

	if (hz_peak < DC_THRESHOLD) {
		fprintf(bat->err, _(" WARNING: Found low peak %2.2f Hz,"),
				hz_peak);
		fprintf(bat->err, _(" very close to DC\n"));
		err = FOUND_DC;
	} else if (hz_peak < bat->target_freq[channel] - tolerance) {
		fprintf(bat->err, _(" FAIL: Peak freq too low %2.2f Hz\n"),
				hz_peak);
		err = FOUND_WRONG_PEAK;
	} else if (hz_peak > bat->target_freq[channel] + tolerance) {
		fprintf(bat->err, _(" FAIL: Peak freq too high %2.2f Hz\n"),
				hz_peak);
		err = FOUND_WRONG_PEAK;
	} else {
		fprintf(bat->log, _(" PASS: Peak detected"));
		fprintf(bat->log, _(" at target frequency\n"));
		err = 0;
	}

	return err;
}

/**
 * Search for main frequencies in fft results and compare it to target
 */
static int check(struct bat *bat, struct analyze *a, int channel)
{
	float hz = 1.0 / ((float) bat->frames / (float) bat->rate);
	float mean = 0.0, t, sigma = 0.0, p = 0.0;
	int i, start = -1, end = -1, peak = 0, signals = 0;
	int err = 0, N = bat->frames / 2;

	/* calculate mean */
	for (i = 0; i < N; i++)
		mean += a->mag[i];
	mean /= (float) N;

	/* calculate standard deviation */
	for (i = 0; i < N; i++) {
		t = a->mag[i] - mean;
		t *= t;
		sigma += t;
	}
	sigma /= (float) N;
	sigma = sqrtf(sigma);

	/* clip any data less than k sigma + mean */
	for (i = 0; i < N; i++) {
		if (a->mag[i] > mean + bat->sigma_k * sigma) {

			/* find peak start points */
			if (start == -1) {
				start = peak = end = i;
				signals++;
			} else {
				if (a->mag[i] > a->mag[peak])
					peak = i;
				end = i;
			}
			p += a->mag[i];
		} else if (start != -1) {
			/* Check if peak is as expected */
			err |= check_peak(bat, a, end, peak, hz, mean,
					p, channel, start);
			end = start = -1;
			if (signals == MAX_PEAKS)
				break;
		}
	}
	if (signals == 0)
		err = -ENOPEAK; /* No peak detected */
	else if ((err == FOUND_DC) && (signals == 1))
		err = -EONLYDC; /* Only DC detected */
	else if ((err & FOUND_WRONG_PEAK) == FOUND_WRONG_PEAK)
		err = -EBADPEAK; /* Bad peak detected */
	else
		err = 0; /* Correct peak detected */

	fprintf(bat->log, _("Detected at least %d signal(s) in total\n"),
			signals);

	return err;
}

static void calc_magnitude(struct bat *bat, struct analyze *a, int N)
{
	float r2, i2;
	int i;

	for (i = 1; i < N / 2; i++) {
		r2 = a->out[i] * a->out[i];
		i2 = a->out[N - i] * a->out[N - i];

		a->mag[i] = sqrtf(r2 + i2);
	}
	a->mag[0] = 0.0;
}

static int find_and_check_harmonics(struct bat *bat, struct analyze *a,
		int channel)
{
	fftwf_plan p;
	int err = -ENOMEM, N = bat->frames;

	/* Allocate FFT buffers */
	a->in = (float *) fftwf_malloc(sizeof(float) * bat->frames);
	if (a->in == NULL)
		goto out1;

	a->out = (float *) fftwf_malloc(sizeof(float) * bat->frames);
	if (a->out == NULL)
		goto out2;

	a->mag = (float *) fftwf_malloc(sizeof(float) * bat->frames);
	if (a->mag == NULL)
		goto out3;

	/* create FFT plan */
	p = fftwf_plan_r2r_1d(N, a->in, a->out, FFTW_R2HC,
			FFTW_MEASURE | FFTW_PRESERVE_INPUT);
	if (p == NULL)
		goto out4;

	/* convert source PCM to floats */
	bat->convert_sample_to_float(a->buf, a->in, bat->frames);

	/* check amplitude */
	check_amplitude(bat, a->in);

	/* run FFT */
	fftwf_execute(p);

	/* FFT out is real and imaginary numbers - calc magnitude for each */
	calc_magnitude(bat, a, N);

	/* check data */
	err = check(bat, a, channel);

	fftwf_destroy_plan(p);

out4:
	fftwf_free(a->mag);
out3:
	fftwf_free(a->out);
out2:
	fftwf_free(a->in);
out1:
	return err;
}

static int calculate_noise_one_period(struct bat *bat,
		struct noise_analyzer *na, float *src,
		int length, int channel)
{
	int i, shift = 0;
	float tmp, rms, gain, residual;
	float a = 0.0, b = 1.0;

	/* step 1. phase compensation */

	if (length < 2 * na->nsamples)
		return -EINVAL;

	/* search for the beginning of a sine period */
	for (i = 0, tmp = 0.0, shift = -1; i < na->nsamples; i++) {
		/* find i where src[i] >= 0 && src[i+1] < 0 */
		if (src[i] < 0.0)
			continue;
		if (src[i + 1] < 0.0) {
			tmp = src[i] - src[i + 1];
			a = src[i] / tmp;
			b = -src[i + 1] / tmp;
			shift = i;
			break;
		}
	}

	/* didn't find the beginning of a sine period */
	if (shift == -1)
		return -EINVAL;

	/* shift sine waveform to source[0] = 0.0 */
	for (i = 0; i < na->nsamples; i++)
		na->source[i] = a * src[i + shift + 1] + b * src[i + shift];

	/* step 2. gain compensation */

	/* calculate rms of signal amplitude */
	for (i = 0, tmp = 0.0; i < na->nsamples; i++)
		tmp += na->source[i] * na->source[i];
	rms = sqrtf(tmp / na->nsamples);

	gain = na->rms_tgt / rms;

	for (i = 0; i < na->nsamples; i++)
		na->source[i] *= gain;

	/* step 3. calculate snr in dB */

	for (i = 0, tmp = 0.0, residual = 0.0; i < na->nsamples; i++) {
		tmp = fabsf(na->target[i] - na->source[i]);
		residual += tmp * tmp;
	}

	tmp = na->rms_tgt / sqrtf(residual / na->nsamples);
	na->snr_db = 20.0 * log10f(tmp);

	return 0;
}

static int calculate_noise(struct bat *bat, float *src, int channel)
{
	int err = 0;
	struct noise_analyzer na;
	float freq = bat->target_freq[channel];
	float tmp, sum_snr_pc, avg_snr_pc, avg_snr_db;
	int offset, i, cnt_noise, cnt_clean;
	/* num of samples in each sine period */
	int nsamples = (int) ceilf(bat->rate / freq);
	/* each section has 2 sine periods, the first one for locating
	 * and the second one for noise calculating */
	int nsamples_per_section = nsamples * 2;
	/* all sine periods will be calculated except the first one */
	int nsection = bat->frames / nsamples - 1;

	fprintf(bat->log, _("samples per period: %d\n"), nsamples);
	fprintf(bat->log, _("total sections to detect: %d\n"), nsection);
	na.source = (float *)malloc(sizeof(float) * nsamples);
	if (!na.source) {
		err = -ENOMEM;
		goto out1;
	}

	na.target = (float *)malloc(sizeof(float) * nsamples);
	if (!na.target) {
		err = -ENOMEM;
		goto out2;
	}

	/* generate standard single-tone signal */
	err = generate_sine_wave_raw_mono(bat, na.target, freq, nsamples);
	if (err < 0)
		goto out3;

	na.nsamples = nsamples;

	/* calculate rms of standard signal */
	for (i = 0, tmp = 0.0; i < nsamples; i++)
		tmp += na.target[i] * na.target[i];
	na.rms_tgt = sqrtf(tmp / nsamples);

	/* calculate average noise level */
	sum_snr_pc = 0.0;
	cnt_clean = cnt_noise = 0;
	for (i = 0, offset = 0; i < nsection; i++) {
		na.snr_db = SNR_DB_INVALID;

		err = calculate_noise_one_period(bat, &na, src + offset,
				nsamples_per_section, channel);
		if (err < 0)
			goto out3;

		if (na.snr_db > bat->snr_thd_db) {
			cnt_clean++;
			sum_snr_pc += 100.0 / powf(10.0, na.snr_db / 20.0);
		} else {
			cnt_noise++;
		}
		offset += nsamples;
	}

	if (cnt_noise > 0) {
		fprintf(bat->err, _("Noise detected at %d points.\n"),
				cnt_noise);
		err = -cnt_noise;
		if (cnt_clean == 0)
			goto out3;
	} else {
		fprintf(bat->log, _("No noise detected.\n"));
	}

	avg_snr_pc = sum_snr_pc / cnt_clean;
	avg_snr_db = 20.0 * log10f(100.0 / avg_snr_pc);
	fprintf(bat->log, _("Average SNR is %.2f dB (%.2f %%) at %d points.\n"),
			avg_snr_db, avg_snr_pc, cnt_clean);

out3:
	free(na.target);
out2:
	free(na.source);
out1:
	return err;
}

static int find_and_check_noise(struct bat *bat, void *buf, int channel)
{
	int err = 0;
	float *source;

	source = (float *)malloc(sizeof(float) * bat->frames);
	if (!source)
		return -ENOMEM;

	/* convert source PCM to floats */
	bat->convert_sample_to_float(buf, source, bat->frames);

	/* adjust waveform and calculate noise */
	err = calculate_noise(bat, source, channel);

	free(source);
	return err;
}

/**
 * Convert interleaved samples from channels in samples from a single channel
 */
static int reorder_data(struct bat *bat)
{
	char *p, *new_bat_buf;
	int ch, i, j;

	if (bat->channels == 1)
		return 0; /* No need for reordering */

	p = malloc(bat->frames * bat->frame_size);
	new_bat_buf = p;
	if (p == NULL)
		return -ENOMEM;

	for (ch = 0; ch < bat->channels; ch++) {
		for (j = 0; j < bat->frames; j++) {
			for (i = 0; i < bat->sample_size; i++) {
				*p++ = ((char *) (bat->buf))[j * bat->frame_size
						+ ch * bat->sample_size + i];
			}
		}
	}

	free(bat->buf);
	bat->buf = new_bat_buf;

	return 0;
}

/* truncate sample frames for faster FFT analysis process */
static int truncate_frames(struct bat *bat)
{
	int shift = SHIFT_MAX;

	for (; shift > SHIFT_MIN; shift--)
		if (bat->frames & (1 << shift)) {
			bat->frames = 1 << shift;
			return 0;
		}

	return -EINVAL;
}

int analyze_capture(struct bat *bat)
{
	int err = 0;
	size_t items;
	int c;
	struct analyze a;

	err = truncate_frames(bat);
	if (err < 0) {
		fprintf(bat->err, _("Invalid frame number for analysis: %d\n"),
				bat->frames);
		return err;
	}

	fprintf(bat->log, _("\nBAT analysis: signal has %d frames at %d Hz,"),
			bat->frames, bat->rate);
	fprintf(bat->log, _(" %d channels, %d bytes per sample.\n"),
			bat->channels, bat->sample_size);

	bat->buf = malloc(bat->frames * bat->frame_size);
	if (bat->buf == NULL)
		return -ENOMEM;

	bat->fp = fopen(bat->capture.file, "rb");
	err = -errno;
	if (bat->fp == NULL) {
		fprintf(bat->err, _("Cannot open file: %s %d\n"),
				bat->capture.file, err);
		goto exit1;
	}

	/* Skip header */
	err = read_wav_header(bat, bat->capture.file, bat->fp, true);
	if (err != 0)
		goto exit2;

	items = fread(bat->buf, bat->frame_size, bat->frames, bat->fp);
	if (items != bat->frames) {
		err = -EIO;
		goto exit2;
	}

	err = reorder_data(bat);
	if (err != 0)
		goto exit2;

	for (c = 0; c < bat->channels; c++) {
		fprintf(bat->log, _("\nChannel %i - "), c + 1);
		fprintf(bat->log, _("Checking for target frequency %2.2f Hz\n"),
				bat->target_freq[c]);
		a.buf = bat->buf +
				c * bat->frames * bat->frame_size
				/ bat->channels;
		if (!bat->standalone) {
			err = find_and_check_harmonics(bat, &a, c);
			if (err != 0)
				goto exit2;
		}

		if (snr_is_valid(bat->snr_thd_db)) {
			fprintf(bat->log, _("\nChecking for SNR: "));
			fprintf(bat->log, _("Threshold is %.2f dB (%.2f%%)\n"),
					bat->snr_thd_db, 100.0
					/ powf(10.0, bat->snr_thd_db / 20.0));
			err = find_and_check_noise(bat, a.buf, c);
			if (err != 0)
				goto exit2;
		}
	}

exit2:
	fclose(bat->fp);
exit1:
	free(bat->buf);

	return err;
}
