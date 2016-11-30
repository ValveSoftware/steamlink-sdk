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
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

void convert_uint8_to_float(void *buf, float *val, int samples)
{
	int i;

	for (i = 0; i < samples; i++)
		val[i] = ((uint8_t *) buf)[i];
}

void convert_int16_to_float(void *buf, float *val, int samples)
{
	int i;

	for (i = 0; i < samples; i++)
		val[i] = ((int16_t *) buf)[i];
}

void convert_int24_to_float(void *buf, float *val, int samples)
{
	int i;
	int32_t tmp;

	for (i = 0; i < samples; i++) {
		tmp = ((uint8_t *) buf)[i * 3 + 2] << 24;
		tmp |= ((uint8_t *) buf)[i * 3 + 1] << 16;
		tmp |= ((uint8_t *) buf)[i * 3] << 8;
		tmp >>= 8;
		val[i] = tmp;
	}
}

void convert_int32_to_float(void *buf, float *val, int samples)
{
	int i;

	for (i = 0; i < samples; i++)
		val[i] = ((int32_t *) buf)[i];
}

void convert_float_to_uint8(float *val, void *buf, int samples, int channels)
{
	int i, c, idx;

	for (i = 0; i < samples; i++) {
		for (c = 0; c < channels; c++) {
			idx = i * channels + c;
			((uint8_t *) buf)[idx] = (uint8_t) val[idx];
		}
	}
}

void convert_float_to_int16(float *val, void *buf, int samples, int channels)
{
	int i, c, idx;

	for (i = 0; i < samples; i++) {
		for (c = 0; c < channels; c++) {
			idx = i * channels + c;
			((int16_t *) buf)[idx] = (int16_t) val[idx];
		}
	}
}

void convert_float_to_int24(float *val, void *buf, int samples, int channels)
{
	int i, c, idx_f, idx_i;
	int32_t val_f_i;

	for (i = 0; i < samples; i++) {
		for (c = 0; c < channels; c++) {
			idx_f = i * channels + c;
			idx_i = 3 * idx_f;
			val_f_i = (int32_t) val[idx_f];
			((int8_t *) buf)[idx_i + 0] =
				(int8_t) (val_f_i & 0xff);
			((int8_t *) buf)[idx_i + 1] =
				(int8_t) ((val_f_i >> 8) & 0xff);
			((int8_t *) buf)[idx_i + 2] =
				(int8_t) ((val_f_i >> 16) & 0xff);
		}
	}
}

void convert_float_to_int32(float *val, void *buf, int samples, int channels)
{
	int i, c, idx;

	for (i = 0; i < samples; i++) {
		for (c = 0; c < channels; c++) {
			idx = i * channels + c;
			((int32_t *) buf)[idx] = (int32_t) val[idx];
		}
	}
}
