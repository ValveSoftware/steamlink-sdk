/* next-prime.c
 *
 * Command line tool for prime search.
 *
 */
 
/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2007 Niels Möller
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bignum.h"

#include "getopt.h"

static void
usage(void)
{
  fprintf(stderr, "Usage: next-prime [OPTIONS] number\n\n"
	  "Options:\n"
	  "      --help         Display this message.\n"
	  "  -v, --verbose      Display timing information.\n"
	  "      --factorial    Use factorial of input number.\n"
	  "  -s  --sieve-limit  Number of primes to use for sieving.\n");
}

int
main(int argc, char **argv)
{
  mpz_t n;
  mpz_t p;

  int c;
  int verbose = 0;  
  int factorial = 0;
  int prime_limit = 200;

  clock_t start;
  clock_t end;
  
  enum { OPT_HELP = 300 };

  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "verbose", no_argument, NULL, 'v' },
      { "factorial", no_argument, NULL, 'f' },
      { "sieve-limit", required_argument, NULL, 's' },
      { NULL, 0, NULL, 0}
    };

  while ( (c = getopt_long(argc, argv, "vs:", options, NULL)) != -1)
    switch (c)
      {
      case 'v':
	verbose = 1;
	break;
      case OPT_HELP:
	usage();
	return EXIT_SUCCESS;
      case 'f':
	factorial = 1;
	break;
      case 's':
	prime_limit = atoi(optarg);
	if (prime_limit < 0)
	  {
	    usage();
	    return EXIT_FAILURE;
	  }
	break;
      case '?':
	return EXIT_FAILURE;
      default:
	abort();
	
      }

  argc -= optind;
  argv += optind;

  if (argc != 1)
    {
      usage();
      return EXIT_FAILURE;
    }

  mpz_init(n);

  if (factorial)
    {
      long arg;
      char *end;
      arg = strtol(argv[0], &end, 0);
      if (*end || arg < 0)
	{
	  fprintf(stderr, "Invalid number.\n");
	  return EXIT_FAILURE;
	}
      mpz_fac_ui(n, arg);
    }
  else if (mpz_set_str(n, argv[0], 0))
    {
      fprintf(stderr, "Invalid number.\n");
      return EXIT_FAILURE;
    }

  if (mpz_cmp_ui(n, 2) <= 0)
    {
      printf("2\n");
      return EXIT_SUCCESS;
    }

  mpz_init(p);

  start = clock();
  nettle_next_prime(p, n, 25, prime_limit, NULL, NULL);
  end = clock();
  
  mpz_out_str(stdout, 10, p);
  printf("\n");

  if (verbose)
    {
      mpz_t d;
      
      mpz_init(d);
      mpz_sub(d, p, n);

      /* Avoid using gmp_fprintf, to stay compatible with gmp-3.1. */
      fprintf(stderr, "bit size: %lu, diff: ", (unsigned long) mpz_sizeinbase(p, 2));
      mpz_out_str(stderr, 10, d);
      fprintf(stderr, ", total time: %.3g s\n",
	      (double)(end - start) / CLOCKS_PER_SEC);
    }
  return EXIT_SUCCESS;
}
