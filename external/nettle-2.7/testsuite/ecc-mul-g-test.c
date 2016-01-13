#include "testutils.h"

void
test_main (void)
{
  gmp_randstate_t state;
  mpz_t r;
  unsigned i;

  gmp_randinit_default (state);
  mpz_init (r);

  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      mp_size_t size = ecc_size (ecc);
      mp_limb_t *p = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *q = xalloc_limbs (ecc_size_j (ecc));
      mp_limb_t *n = xalloc_limbs (size);
      mp_limb_t *scratch = xalloc_limbs (ecc_mul_g_itch (ecc));

      mpn_zero (n, size);

      n[0] = 1;
      ecc_mul_g (ecc, p, n, scratch);
      ecc_j_to_a (ecc, 1, p, p, scratch);

      if (mpn_cmp (p, ecc->g, 2*size != 0))
	{
	  fprintf (stderr, "ecc_mul_g with n = 1 failed.\n");
	  abort ();
	}

      for (n[0] = 2; n[0] <= 4; n[0]++)
	{
	  ecc_mul_g (ecc, p, n, scratch);
	  test_ecc_mul_j (i, n[0], p);
	}

      /* (order - 1) * g = - g */
      mpn_sub_1 (n, ecc->q, size, 1);
      ecc_mul_g (ecc, p, n, scratch);
      ecc_j_to_a (ecc, 1, p, p, scratch);
      mpn_sub_n (p + size, ecc->p, p + size, size);
      if (mpn_cmp (p, ecc->g, 2*size) != 0)
	{
	  fprintf (stderr, "ecc_mul_g with n = order - 1 failed.\n");
	  abort ();
	}

      free (n);
      free (p);
      free (q);
      free (scratch);
    }
  mpz_clear (r);
  gmp_randclear (state);
}
