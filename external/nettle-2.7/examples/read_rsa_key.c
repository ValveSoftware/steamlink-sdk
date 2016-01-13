/* Used by the rsa example programs. */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002, 2007 Niels Möller
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "io.h"
#include "rsa.h"

/* Split out from io.c, since it depends on hogweed. */
int
read_rsa_key(const char *name,
	     struct rsa_public_key *pub,
	     struct rsa_private_key *priv)
{
  unsigned length;
  char *buffer;
  int res;
  
  length = read_file(name, 0, &buffer);
  if (!length)
    return 0;

  res = rsa_keypair_from_sexp(pub, priv, 0, length, buffer);
  free(buffer);

  return res;
}
