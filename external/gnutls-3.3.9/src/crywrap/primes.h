/* primes.h -- initial DH primes for CryWrap
 *
 * Copyright (C) 2003 Gergely Nagy <algernon@bonehunter.rulez.org>
 * Copyright (C) 2011 Nikos Mavrogiannopoulos
 *
 * This file is part of CryWrap.
 *
 * CryWrap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CryWrap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @file primes.h
 * Initial DH primes for CryWrap.
 *
 * In order to speed up the startup time, CryWrap does not generate a
 * new DH prime upon every startup, but only when it receives a
 * SIGHUP.
 */

#ifndef _CRYWRAP_PRIMES_H
#define _CRYWRAP_PRIMES_H

/** Initial DH primes, 1024 bits.
 */
static char _crywrap_prime_dh_1024[] = "-----BEGIN DH PARAMETERS-----\n"
    "MIGHAoGBAO6vCrmts43WnDP4CvqPxehgcmGHdf88C56iMUycJWV21nTfdJbqgdM4\n"
    "O0gT1pLG4ODV2OJQuYvkjklcHWCJ2tFdx9e0YVTWts6O9K1psV1JglWbKXvPGIXF\n"
    "KfVmZg5X7GjtvDwFcmzAL9TL9Jduqpr9UTj+g3ZDW5/GHS/A6wbjAgEC\n"
    "-----END DH PARAMETERS-----\n";

#endif
