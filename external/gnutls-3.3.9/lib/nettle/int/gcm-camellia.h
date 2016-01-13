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

#include <nettle/camellia.h>

struct _gcm_camellia_ctx GCM_CTX(struct camellia_ctx);

void _gcm_camellia_set_key(struct _gcm_camellia_ctx *ctx, unsigned length,
			   const uint8_t * key);
void _gcm_camellia_set_iv(struct _gcm_camellia_ctx *ctx, unsigned length,
			  const uint8_t * iv);
void _gcm_camellia_update(struct _gcm_camellia_ctx *ctx, unsigned length,
			  const uint8_t * data);
void _gcm_camellia_encrypt(struct _gcm_camellia_ctx *ctx, unsigned length,
			   uint8_t * dst, const uint8_t * src);
void _gcm_camellia_decrypt(struct _gcm_camellia_ctx *ctx, unsigned length,
			   uint8_t * dst, const uint8_t * src);
void _gcm_camellia_digest(struct _gcm_camellia_ctx *ctx, unsigned length,
			  uint8_t * digest);
