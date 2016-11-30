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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "bat-signal.h"
#include "gettext.h"

/* How one measurement step works:
   - Listen and measure the average loudness of the environment for 1 second.
   - Create a threshold value 16 decibels higher than the average loudness.
   - Begin playing a ~1000 Hz sine wave and start counting the samples elapsed.
   - Stop counting and playing if the input's loudness is higher than the
     threshold, as the output wave is probably coming back.
   - Calculate the round trip audio latency value in milliseconds. */

static float sumaudio(struct bat *bat, short int *buffer, int frames)
{
	float sum = 0;
	int n = 0;

	while (frames) {
		frames--;

		for (n = 0; n < bat->channels; n++) {
			sum += abs(buffer[0]);
			buffer++;
		}
	}

	sum = sum / bat->channels;

	return sum;
}

static void play_and_listen(struct bat *bat, void *buffer, int frames)
{
	int averageinput;
	int n = 0;
	float sum = 0;
	float max = 0;
	float min = 100000.0f;
	short int *input;
	int num = bat->latency.number;

	averageinput = (int) (sumaudio(bat, buffer, frames) / frames);

	/* The signal is above threshold
	   So our sine wave comes back on the input */
	if (averageinput > bat->latency.threshold) {
		input = buffer;

		/* Check the location when it became loud enough */
		while (n < frames) {
			if (*input++ > bat->latency.threshold)
				break;
			*input += bat->channels;
			n++;
		}

		/* Now we get the total round trip latency*/
		bat->latency.samples += n;

		/* Expect at least 1 buffer of round trip latency. */
		if (bat->latency.samples > frames) {
			bat->latency.result[num - 1] =
				(float) bat->latency.samples * 1000 / bat->rate;
			fprintf(bat->log,
					 _("Test%d, round trip latency %dms\n"),
					num,
					(int) bat->latency.result[num - 1]);

			for (n = 0; n < num; n++) {
				if (bat->latency.result[n] > max)
					max = bat->latency.result[n];
				if (bat->latency.result[n] < min)
					min = bat->latency.result[n];
				sum += bat->latency.result[n];
			}

			/* The maximum is higher than the minimum's double */
			if (max / min > 2.0f) {
				bat->latency.state =
					LATENCY_STATE_COMPLETE_FAILURE;
				bat->latency.is_capturing = false;
				return;

			/* Final results */
			} else if (num == LATENCY_TEST_NUMBER) {
				bat->latency.final_result =
					(int) (sum / LATENCY_TEST_NUMBER);
				fprintf(bat->log,
					_("Final round trip latency: %dms\n"),
					bat->latency.final_result);

				bat->latency.state =
					LATENCY_STATE_COMPLETE_SUCCESS;
				bat->latency.is_capturing = false;
				return;

			/* Next step */
			} else
				bat->latency.state = LATENCY_STATE_WAITING;

			bat->latency.number++;

		} else
			/* Happens when an early noise comes in */
			bat->latency.state = LATENCY_STATE_WAITING;

	} else {
		/* Still listening */
		bat->latency.samples += frames;

		/* Do not listen to more than a second
		   Maybe too much background noise */
		if (bat->latency.samples > bat->rate) {
			bat->latency.error++;

			if (bat->latency.error > LATENCY_TEST_NUMBER) {
				fprintf(bat->err,
					_("Could not detect signal."));
				fprintf(bat->err,
					_("Too much background noise?\n"));
				bat->latency.state =
					LATENCY_STATE_COMPLETE_FAILURE;
				bat->latency.is_capturing = false;
				return;
			}

			/* let's start over */
			bat->latency.state = LATENCY_STATE_WAITING;
		}
	}

	return;
}

static void calculate_threshold(struct bat *bat)
{
	float average;
	float reference;

	/* Calculate the average loudness of the environment and create
	   a threshold value 16 decibels higher than the average loudness */
	average = bat->latency.sum / bat->latency.samples / 32767.0f;
	reference = 20.0f * log10f(average) + 16.0f;
	bat->latency.threshold = (int) (powf(10.0f, reference / 20.0f)
						* 32767.0f);
}

void roundtrip_latency_init(struct bat *bat)
{
	bat->latency.number = 1;
	bat->latency.state = LATENCY_STATE_MEASURE_FOR_1_SECOND;
	bat->latency.final_result = 0;
	bat->latency.samples = 0;
	bat->latency.sum = 0;
	bat->latency.threshold = 0;
	bat->latency.is_capturing = false;
	bat->latency.is_playing = false;
	bat->latency.error = 0;
	bat->latency.xrun_error = false;
	bat->format = BAT_PCM_FORMAT_S16_LE;
	bat->frames = LATENCY_TEST_TIME_LIMIT * bat->rate;
	bat->periods_played = 0;
}

int handleinput(struct bat *bat, void *buffer, int frames)
{
	switch (bat->latency.state) {
	/* Measuring average loudness for 1 second */
	case LATENCY_STATE_MEASURE_FOR_1_SECOND:
		bat->latency.sum += sumaudio(bat, buffer, frames);
		bat->latency.samples += frames;

		/* 1 second elapsed */
		if (bat->latency.samples >= bat->rate) {
			calculate_threshold(bat);
			bat->latency.state = LATENCY_STATE_PLAY_AND_LISTEN;
			bat->latency.samples = 0;
			bat->latency.sum = 0;
		}
		break;

	/* Playing sine wave and listening if it comes back */
	case LATENCY_STATE_PLAY_AND_LISTEN:
		play_and_listen(bat, buffer, frames);
		break;

	/* Waiting 1 second */
	case LATENCY_STATE_WAITING:
		bat->latency.samples += frames;

		if (bat->latency.samples > bat->rate) {
			/* 1 second elapsed, start over */
			bat->latency.samples = 0;
			bat->latency.state = LATENCY_STATE_MEASURE_FOR_1_SECOND;
		}
		break;

	default:
		return 0;
	}

	return 0;
}

int handleoutput(struct bat *bat, void *buffer, int bytes, int frames)
{
	int err = 0;

	/* If capture completed, terminate the playback */
	if (bat->periods_played * frames > 2 * bat->rate
			&& bat->latency.is_capturing == false)
		return bat->latency.state;

	if (bat->latency.state == LATENCY_STATE_PLAY_AND_LISTEN)
		err = generate_sine_wave(bat, frames, buffer);
	else
		/* Output silence */
		memset(buffer, 0, bytes);

	return err;
}
