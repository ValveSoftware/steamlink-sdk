/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2014  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "connman.h"

#define URANDOM "/dev/urandom"

int f = -1;

int __connman_util_get_random(uint64_t *val)
{
	int r = 0;

	if (!val)
		return -EINVAL;

	if (read(f, val, sizeof(uint64_t)) < 0) {
		r = -errno;
		connman_warn_once("Could not read from "URANDOM);
		*val = random();
	}

	return r;
}

int __connman_util_init(void)
{
	int r = 0;

	if (f > 0)
		return 0;

	f = open(URANDOM, O_RDONLY);
	if (f < 0) {
		r = -errno;
		connman_warn("Could not open "URANDOM);
		srandom(time(NULL));
	} else {
		uint64_t val;

		r = __connman_util_get_random(&val);
		if (r < 0)
			srandom(time(NULL));
		else
			srandom(val);
	}

	return r;
}

void __connman_util_cleanup(void)
{
	if (f > 0)
		close(f);

	f = -1;
}
