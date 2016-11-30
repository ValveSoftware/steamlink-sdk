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

void convert_uint8_to_float(void *, float *, int);
void convert_int16_to_float(void *, float *, int);
void convert_int24_to_float(void *, float *, int);
void convert_int32_to_float(void *, float *, int);
void convert_float_to_uint8(float *, void *, int, int);
void convert_float_to_int16(float *, void *, int, int);
void convert_float_to_int24(float *, void *, int, int);
void convert_float_to_int32(float *, void *, int, int);
