/* output.h */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002, 2003 Niels Möller
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

#ifndef NETTLE_TOOLS_OUTPUT_H_INCLUDED
#define NETTLE_TOOLS_OUTPUT_H_INCLUDED

#include "misc.h"

#include "base64.h"
#include "buffer.h"
#include "nettle-meta.h"

#include <stdio.h>

struct sexp_output
{
  FILE *f;

  unsigned line_width;
  
  const struct nettle_armor *coding;
  unsigned coding_indent;

  int prefer_hex;
  
  const struct nettle_hash *hash;
  void *ctx;
  
  /* NOTE: There's no context for hex encoding, the state argument to
     encode_update is ignored */
  struct base64_decode_ctx base64;
  
  unsigned pos;
  int soft_newline;
};

void
sexp_output_init(struct sexp_output *output, FILE *f,
		 unsigned width, int prefer_hex);

void
sexp_output_hash_init(struct sexp_output *output,
		      const struct nettle_hash *hash, void *ctx);

void 
sexp_put_newline(struct sexp_output *output,
		 unsigned indent);

void
sexp_put_soft_newline(struct sexp_output *output,
		      unsigned indent);

void
sexp_put_char(struct sexp_output *output, uint8_t c);

void
sexp_put_data(struct sexp_output *output,
	      unsigned length, const uint8_t *data);

void
sexp_put_code_start(struct sexp_output *output,
		    const struct nettle_armor *coding);

void
sexp_put_code_end(struct sexp_output *output);

void
sexp_put_string(struct sexp_output *output, enum sexp_mode mode,
		struct nettle_buffer *string);

void
sexp_put_digest(struct sexp_output *output);

#endif /* NETTLE_TOOLS_OUTPUT_H_INCLUDED */
