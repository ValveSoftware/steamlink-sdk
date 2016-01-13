/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

/* gcm_camellia.c
 *
 * Galois counter mode using Camellia as the underlying cipher.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <nettle/gcm.h>
#include <nettle/camellia.h>
#include <gcm-camellia.h>

void
_gcm_camellia_set_key(struct _gcm_camellia_ctx *ctx, unsigned length,
		      const uint8_t * key)
{
	GCM_SET_KEY(ctx, camellia_set_encrypt_key, camellia_crypt, length,
		    key);
}

void
_gcm_camellia_set_iv(struct _gcm_camellia_ctx *ctx,
		     unsigned length, const uint8_t * iv)
{
	GCM_SET_IV(ctx, length, iv);
}

void
_gcm_camellia_update(struct _gcm_camellia_ctx *ctx, unsigned length,
		     const uint8_t * data)
{
	GCM_UPDATE(ctx, length, data);
}

void
_gcm_camellia_encrypt(struct _gcm_camellia_ctx *ctx,
		      unsigned length, uint8_t * dst, const uint8_t * src)
{
	GCM_ENCRYPT(ctx, camellia_crypt, length, dst, src);
}

void
_gcm_camellia_decrypt(struct _gcm_camellia_ctx *ctx,
		      unsigned length, uint8_t * dst, const uint8_t * src)
{
	GCM_DECRYPT(ctx, camellia_crypt, length, dst, src);
}

void
_gcm_camellia_digest(struct _gcm_camellia_ctx *ctx,
		     unsigned length, uint8_t * digest)
{
	GCM_DIGEST(ctx, camellia_crypt, length, digest);

}
