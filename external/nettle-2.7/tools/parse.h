/* parse.h */

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

#ifndef NETTLE_TOOLS_PARSE_H_INCLUDED
#define NETTLE_TOOLS_PARSE_H_INCLUDED

#include "misc.h"
#include "buffer.h"

struct sexp_compound_token
{
  enum sexp_token type;
  struct nettle_buffer display;
  struct nettle_buffer string;  
};

void
sexp_compound_token_init(struct sexp_compound_token *token);

void
sexp_compound_token_clear(struct sexp_compound_token *token);

struct sexp_parser
{
  struct sexp_input *input;
  enum sexp_mode mode;

  /* Nesting level of lists. Transport encoding counts as one
   * level of nesting. */
  unsigned level;

  /* The nesting level where the transport encoding occured.
   * Zero if we're not currently using tranport encoding. */
  unsigned transport;
};

void
sexp_parse_init(struct sexp_parser *parser,
		struct sexp_input *input,
		enum sexp_mode mode);

void
sexp_parse(struct sexp_parser *parser,
	   struct sexp_compound_token *token);

#endif /* NETTLE_TOOLS_PARSE_H_INCLUDED */
