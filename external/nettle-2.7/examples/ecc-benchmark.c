/* ecc-benchmark.c */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2013 Niels Möller
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

/* Development of Nettle's ECC support was funded by the .SE Internet Fund. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <time.h>

#include "timing.h"

#include "../ecc.h"
#include "../ecc-internal.h"
#include "../gmp-glue.h"

#define BENCH_INTERVAL 0.1

static void NORETURN PRINTF_STYLE(1,2)
die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(EXIT_FAILURE);
}

static void *
xalloc (size_t size)
{
  void *p = malloc (size);
  if (!p)
    {
      fprintf (stderr, "Virtual memory exhausted\n");
      abort ();
    }
  return p;
}

static mp_limb_t *
xalloc_limbs (mp_size_t size)
{
  return xalloc (size * sizeof(mp_limb_t));
}

/* Returns second per function call */
static double
time_function(void (*f)(void *arg), void *arg)
{
  unsigned ncalls;
  double elapsed;

  /* Warm up */
  f(arg);
  for (ncalls = 10 ;;)
    {
      unsigned i;

      time_start();
      for (i = 0; i < ncalls; i++)
	f(arg);
      elapsed = time_end();
      if (elapsed > BENCH_INTERVAL)
	break;
      else if (elapsed < BENCH_INTERVAL / 10)
	ncalls *= 10;
      else
	ncalls *= 2;
    }
  return elapsed / ncalls;
}

static int
modinv_gcd (const struct ecc_curve *ecc,
	    mp_limb_t *rp, mp_limb_t *ap, mp_limb_t *tp)
{
  mp_size_t size = ecc->size;
  mp_limb_t *up = tp;
  mp_limb_t *vp = tp + size+1;
  mp_limb_t *gp = tp + 2*(size+1);
  mp_limb_t *sp = tp + 3*(size+1);
  mp_size_t gn, sn;

  mpn_copyi (up, ap, size);
  mpn_copyi (vp, ecc->p, size);
  gn = mpn_gcdext (gp, sp, &sn, up, size, vp, size);
  if (gn != 1 || gp[0] != 1)
    return 0;
  
  if (sn < 0)
    mpn_sub (sp, ecc->p, size, sp, -sn);
  else if (sn < size)
    /* Zero-pad. */
    mpn_zero (sp + sn, size - sn);

  mpn_copyi (rp, sp, size);
  return 1;
}

struct ecc_ctx {
  const struct ecc_curve *ecc;
  mp_limb_t *rp;
  mp_limb_t *ap;
  mp_limb_t *bp;
  mp_limb_t *tp;
};

static void
bench_modp (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->size);
  ctx->ecc->modp (ctx->ecc, ctx->rp);
}

static void
bench_redc (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->size);
  ctx->ecc->redc (ctx->ecc, ctx->rp);
}

static void
bench_modq (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp, ctx->ap, 2*ctx->ecc->size);
  ctx->ecc->modq (ctx->ecc, ctx->rp);
}

static void
bench_modinv (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp + ctx->ecc->size, ctx->ap, ctx->ecc->size);
  ecc_modp_inv (ctx->ecc, ctx->rp, ctx->rp + ctx->ecc->size, ctx->tp);
}

static void
bench_modinv_gcd (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  mpn_copyi (ctx->rp + ctx->ecc->size, ctx->ap, ctx->ecc->size);
  modinv_gcd (ctx->ecc, ctx->rp, ctx->rp + ctx->ecc->size, ctx->tp);  
}

static void
bench_dup_jj (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_dup_jj (ctx->ecc, ctx->rp, ctx->ap, ctx->tp);
}

static void
bench_add_jja (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_add_jja (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_add_jjj (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_add_jjj (ctx->ecc, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_mul_g (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_mul_g (ctx->ecc, ctx->rp, ctx->ap, ctx->tp);
}

static void
bench_mul_a (void *p)
{
  struct ecc_ctx *ctx = (struct ecc_ctx *) p;
  ecc_mul_a (ctx->ecc, 1, ctx->rp, ctx->ap, ctx->bp, ctx->tp);
}

static void
bench_curve (const struct ecc_curve *ecc)
{
  struct ecc_ctx ctx;  
  double modp, redc, modq, modinv, modinv_gcd,
    dup_jj, add_jja, add_jjj,
    mul_g, mul_a;

  mp_limb_t mask;

  ctx.ecc = ecc;
  ctx.rp = xalloc_limbs (3*ecc->size);
  ctx.ap = xalloc_limbs (3*ecc->size);
  ctx.bp = xalloc_limbs (3*ecc->size);
  ctx.tp = xalloc_limbs (ECC_MUL_A_ITCH (ecc->size));

  mpn_random (ctx.ap, 3*ecc->size);
  mpn_random (ctx.bp, 3*ecc->size);

  mask = (~(mp_limb_t) 0) >> (ecc->size * GMP_NUMB_BITS - ecc->bit_size);
  ctx.ap[ecc->size - 1] &= mask;
  ctx.ap[2*ecc->size - 1] &= mask;
  ctx.ap[3*ecc->size - 1] &= mask;
  ctx.bp[ecc->size - 1] &= mask;
  ctx.bp[2*ecc->size - 1] &= mask;
  ctx.bp[3*ecc->size - 1] &= mask;

  modp = time_function (bench_modp, &ctx);
  redc = ecc->redc ? time_function (bench_redc, &ctx) : 0;

  modq = time_function (bench_modq, &ctx);

  modinv = time_function (bench_modinv, &ctx);
  modinv_gcd = time_function (bench_modinv_gcd, &ctx);
  dup_jj = time_function (bench_dup_jj, &ctx);
  add_jja = time_function (bench_add_jja, &ctx);
  add_jjj = time_function (bench_add_jjj, &ctx);
  mul_g = time_function (bench_mul_g, &ctx);
  mul_a = time_function (bench_mul_a, &ctx);

  free (ctx.rp);
  free (ctx.ap);
  free (ctx.bp);
  free (ctx.tp);

  printf ("%4d %6.4f %6.4f %6.4f %6.2f %6.3f %6.3f %6.3f %6.3f %6.1f %6.1f\n",
	  ecc->bit_size, 1e6 * modp, 1e6 * redc, 1e6 * modq,
	  1e6 * modinv, 1e6 * modinv_gcd,
	  1e6 * dup_jj, 1e6 * add_jja, 1e6 * add_jjj,
	  1e6 * mul_g, 1e6 * mul_a);
}

const struct ecc_curve * const curves[] = {
  &nettle_secp_192r1,
  &nettle_secp_224r1,
  &nettle_secp_256r1,
  &nettle_secp_384r1,
  &nettle_secp_521r1,
};

#define numberof(x)  (sizeof (x) / sizeof ((x)[0]))

int
main (int argc UNUSED, char **argv UNUSED)
{
  unsigned i;

  time_init();
  printf ("%4s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s (us)\n",
	  "size", "modp", "redc", "modq", "modinv", "mi_gcd",
	  "dup_jj", "ad_jja", "ad_jjj",
	  "mul_g", "mul_a");
  for (i = 0; i < numberof (curves); i++)
    bench_curve (curves[i]);

  return EXIT_SUCCESS;
}
