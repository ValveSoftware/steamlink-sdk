/***
    This file is part of PulseAudio.

    Copyright 2010 Arun Raghavan <arun.raghavan@collabora.co.uk>

    The actual implementation is taken from the sources at
    http://andreadrian.de/intercom/ - for the license, look for
    adrian-license.txt in the same directory as this file.

    PulseAudio is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License,
    or (at your option) any later version.

    PulseAudio is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

/* Forward declarations */

typedef struct AEC AEC;

AEC* AEC_init(int RATE, int have_vector);
void AEC_done(AEC *a);
int AEC_doAEC(AEC *a, int d_, int x_);
