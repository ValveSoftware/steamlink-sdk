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
 * Here's a generic sine wave generator that will work indefinitely
 * for any frequency.
 *
 * Note:  the state & phasor are stored as doubles (and updated as
 * doubles) because after a million samples the magnitude drifts a
 * bit.  If we really need floats, it can be done with periodic
 * renormalization of the state_real+state_imag magnitudes.
 */

int sin_generator_init(struct sin_generator *, float, float, float);
float sin_generator_next_sample(struct sin_generator *);
void sin_generator_vfill(struct sin_generator *, float *, int);
int generate_sine_wave(struct bat *, int, void *);
int generate_sine_wave_raw_mono(struct bat *, float *, float, int);
