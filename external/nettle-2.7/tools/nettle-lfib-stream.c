/* lfib-stream.c
 *
 * Generates a pseudorandom stream, using the Knuth lfib
 * (non-cryptographic) pseudorandom generator.
 *
 */
 
/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2003 Niels Möller
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

#include "knuth-lfib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#define BUFSIZE 500

static void
usage(void)
{
  fprintf(stderr, "Usage: lfib-stream [SEED]\n");
}

int
main(int argc, char **argv)
{
  struct knuth_lfib_ctx ctx;
  uint32_t seed;

  if (argc == 1)
    seed = time(NULL);

  else if (argc == 2)
    {
      seed = atoi(argv[1]);
      if (!seed)
	{
	  usage();
	  return EXIT_FAILURE;
	}
    }
  else
    {
      usage();
      return EXIT_FAILURE;
    }

  knuth_lfib_init(&ctx, seed);

  for (;;)
    {
      char buffer[BUFSIZE];
      knuth_lfib_random(&ctx, BUFSIZE, buffer);

      if (fwrite(buffer, 1, BUFSIZE, stdout) < BUFSIZE
	  || fflush(stdout) < 0)
	return EXIT_FAILURE;
    }

  /* Not reached. This program is usually terminated by SIGPIPE */
}
