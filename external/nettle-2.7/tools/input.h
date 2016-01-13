/* input.h */

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

#ifndef NETTLE_TOOLS_INPUT_H_INCLUDED
#define NETTLE_TOOLS_INPUT_H_INCLUDED

#include "misc.h"

#include "base16.h"
#include "base64.h"
#include "buffer.h"
#include "nettle-meta.h"

#include <stdio.h>

/* Special marks in the input stream */
enum sexp_char_type
  {
    SEXP_NORMAL_CHAR = 0,
    SEXP_EOF_CHAR, SEXP_END_CHAR,
  };

struct sexp_input
{
  FILE *f;

  /* Character stream, consisting of ordinary characters,
   * SEXP_EOF_CHAR, and SEXP_END_CHAR. */
  enum sexp_char_type ctype;
  uint8_t c;
  
  const struct nettle_armor *coding;

  union {
    struct base64_decode_ctx base64;
    struct base16_decode_ctx hex;
  } state;

  /* Terminator for current coding */
  uint8_t terminator;
  
  /* Type of current token */
  enum sexp_token token;
};

void
sexp_input_init(struct sexp_input *input, FILE *f);

void
sexp_get_char(struct sexp_input *input);

void
sexp_get_token(struct sexp_input *input, enum sexp_mode mode,
	       struct nettle_buffer *string);


#endif /* NETTLE_TOOLS_INPUT_H_INCLUDED */
