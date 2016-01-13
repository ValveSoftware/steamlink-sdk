/* rsa-sign.c
 *
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* string.h must be included before gmp.h */
#include "rsa.h"
#include "io.h"

int
main(int argc, char **argv)
{
  struct rsa_private_key key;
  struct sha1_ctx hash;
  mpz_t s;
  
  if (argc != 2)
    {
      werror("Usage: rsa-sign PRIVATE-KEY < file\n");
      return EXIT_FAILURE;
    }

  rsa_private_key_init(&key);
  
  if (!read_rsa_key(argv[1], NULL, &key))
    {
      werror("Invalid key\n");
      return EXIT_FAILURE;
    }

  sha1_init(&hash);
  if (!hash_file(&nettle_sha1, &hash, stdin))
    {
      werror("Failed reading stdin: %s\n",
	      strerror(errno));
      return 0;
    }

  mpz_init(s);
  if (!rsa_sha1_sign(&key, &hash, s))
    {
      werror("RSA key too small\n");
      return 0;
    }

  if (!mpz_out_str(stdout, 16, s))
    {
      werror("Failed writing signature: %s\n",
	      strerror(errno));
      return 0;
    }

  putchar('\n');
  
  mpz_clear(s);
  rsa_private_key_clear(&key);

  return EXIT_SUCCESS;
}
