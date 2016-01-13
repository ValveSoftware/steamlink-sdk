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
      mp_limb_t *scratch = xalloc_limbs (ecc_mul_a_itch (ecc));
      unsigned j;
      
      mpn_zero (n, size);

      n[0] = 1;
      ecc_mul_a (ecc, 1, p, n, ecc->g, scratch);
      ecc_j_to_a (ecc, 1, p, p, scratch);

      if (mpn_cmp (p, ecc->g, 2*size != 0))
	die ("curve %d: ecc_mul_a with n = 1 failed.\n", ecc->bit_size);

      if (ecc->use_redc)
	{
	  ecc_mul_a (ecc, 0, p, n, ecc->redc_g, scratch);
	  ecc_j_to_a (ecc, 1, p, p, scratch);

	  if (mpn_cmp (p, ecc->g, 2*size != 0))
	    die ("curve %d: ecc_mul_a with n = 1 and redc failed.\n", ecc->bit_size);
	}
      for (n[0] = 2; n[0] <= 4; n[0]++)
	{
	  ecc_mul_a (ecc, 1, p, n, ecc->g, scratch);
	  test_ecc_mul_j (i, n[0], p);
	  if (ecc->use_redc)
	    {
	      ecc_mul_a (ecc, 0, p, n, ecc->redc_g, scratch);
	      test_ecc_mul_j (i, n[0], p);
	    }
	}

      /* (order - 1) * g = - g */
      mpn_sub_1 (n, ecc->q, size, 1);
      ecc_mul_a (ecc, 1, p, n, ecc->g, scratch);
      ecc_j_to_a (ecc, 1, p, p, scratch);
      mpn_sub_n (p + size, ecc->p, p + size, size);
      if (mpn_cmp (p, ecc->g, 2*size) != 0)
	{
	  fprintf (stderr, "ecc_mul_a with n = order - 1 failed.\n");
	  abort ();
	}

      mpn_zero (n, size);

      for (j = 0; j < 100; j++)
	{
	  if (j & 1)
	    mpz_rrandomb (r, state, size * GMP_NUMB_BITS);
	  else
	    mpz_urandomb (r, state, size * GMP_NUMB_BITS);

	  /* Reduce so that (almost surely) n < q */
	  mpz_limbs_copy (n, r, size);
	  n[size - 1] %= ecc->q[size - 1];

	  ecc_mul_a (ecc, 1, p, n, ecc->g, scratch);
	  ecc_j_to_a (ecc, 1, p, p, scratch);

	  ecc_mul_g (ecc, q, n, scratch);
	  ecc_j_to_a (ecc, 1, q, q, scratch);

	  if (mpn_cmp (p, q, 2*size))
	    {
	      gmp_fprintf (stderr,
			   "Different results from ecc_mul_a and ecc_mul_g.\n"
			   " bits = %u\n"
			   " n = %Nx\n",
			   ecc->bit_size, n, size);
	      gmp_fprintf (stderr, "p = %Nx,\n    %Nx\n",
			   p, size, p + size, size);
	      gmp_fprintf (stderr, "q = %Nx,\n    %Nx\n",
			   q, size, q + size, size);
	      abort ();
	    }
	}
      free (n);
      free (p);
      free (q);
      free (scratch);
    }
  mpz_clear (r); 
  gmp_randclear (state);
}
