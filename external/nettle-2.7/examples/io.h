/* io.c
 *
 * Miscellaneous functions used by the example programs.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002 Niels Möller
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02111-1301, USA.
 */

#ifndef NETTLE_EXAMPLES_IO_H_INCLUDED
#define NETTLE_EXAMPLES_IO_H_INCLUDED

#include "nettle-meta.h"
#include "yarrow.h"

#include <stdio.h>

extern int quiet_flag;

void *
xalloc(size_t size);

void
werror(const char *format, ...) PRINTF_STYLE(1, 2);

/* If size is > 0, read at most that many bytes. If size == 0, read
 * until EOF. Allocates the buffer dynamically. An empty file is
 * treated as an error; return value is zero, and no space is
 * allocated. The returned data is NUL-terminated, for convenience. */

unsigned
read_file(const char *name, unsigned size, char **buffer);

int
write_file(const char *name, unsigned size, const char *buffer);

int
write_string(FILE *f, unsigned size, const char *buffer);

int
simple_random(struct yarrow256_ctx *ctx, const char *name);

int
hash_file(const struct nettle_hash *hash, void *ctx, FILE *f);

#if WITH_HOGWEED
struct rsa_public_key;
struct rsa_private_key;

int
read_rsa_key(const char *name,
	     struct rsa_public_key *pub,
	     struct rsa_private_key *priv);
#endif /* WITH_HOGWEED */

#endif /* NETTLE_EXAMPLES_IO_H_INCLUDED */
