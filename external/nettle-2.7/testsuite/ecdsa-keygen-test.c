#include "testutils.h"
#include "knuth-lfib.h"

/* Check if y^2 = x^3 - 3x + b */
static int
ecc_valid_p (struct ecc_point *pub)
{
  mpz_t t, x, y;
  mpz_t lhs, rhs;
  int res;
  mp_size_t size;

  size = pub->ecc->size;

  /* First check range */
  if (mpn_cmp (pub->p, pub->ecc->p, size) >= 0
      || mpn_cmp (pub->p + size, pub->ecc->p, size) >= 0)
    return 0;

  mpz_init (lhs);
  mpz_init (rhs);

  mpz_roinit_n (x, pub->p, size);
  mpz_roinit_n (y, pub->p + size, size);

  mpz_mul (lhs, y, y);
  mpz_mul (rhs, x, x);
  mpz_sub_ui (rhs, rhs, 3);
  mpz_mul (rhs, rhs, x);
  mpz_add (rhs, rhs, mpz_roinit_n (t, pub->ecc->b, size));

  res = mpz_congruent_p (lhs, rhs, mpz_roinit_n (t, pub->ecc->p, size));
  
  mpz_clear (lhs);
  mpz_clear (rhs);

  return res;
}

void
test_main (void)
{
  unsigned i;
  struct knuth_lfib_ctx rctx;
  struct dsa_signature signature;

  struct tstring *digest;

  knuth_lfib_init (&rctx, 4711);
  dsa_signature_init (&signature);

  digest = SHEX (/* sha256("abc") */
		 "BA7816BF 8F01CFEA 414140DE 5DAE2223"
		 "B00361A3 96177A9C B410FF61 F20015AD");

  for (i = 0; ecc_curves[i]; i++)
    {
      const struct ecc_curve *ecc = ecc_curves[i];
      struct ecc_point pub;
      struct ecc_scalar key;

      if (verbose)
	fprintf (stderr, "Curve %d\n", ecc->bit_size);

      ecc_point_init (&pub, ecc);
      ecc_scalar_init (&key, ecc);

      ecdsa_generate_keypair (&pub, &key,
			      &rctx,
			      (nettle_random_func *) knuth_lfib_random);

      if (verbose)
	{
	  gmp_fprintf (stderr,
		       "Public key:\nx = %Nx\ny = %Nx\n",
		       pub.p, ecc->size, pub.p + ecc->size, ecc->size);
	  gmp_fprintf (stderr,
		       "Private key: %Nx\n", key.p, ecc->size);
	}
      if (!ecc_valid_p (&pub))
	die ("ecdsa_generate_keypair produced an invalid point.\n");

      ecdsa_sign (&key,
		  &rctx, (nettle_random_func *) knuth_lfib_random,
		  digest->length, digest->data,
		  &signature);

      if (!ecdsa_verify (&pub, digest->length, digest->data,
			  &signature))
	die ("ecdsa_verify failed.\n");

      digest->data[3] ^= 17;
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid digest.\n");
      digest->data[3] ^= 17;

      mpz_combit (signature.r, 117);
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid signature.r.\n");

      mpz_combit (signature.r, 117);
      mpz_combit (signature.s, 93);
      if (ecdsa_verify (&pub, digest->length, digest->data,
			 &signature))
	die ("ecdsa_verify  returned success with invalid signature.s.\n");

      ecc_point_clear (&pub);
      ecc_scalar_clear (&key);
    }
  dsa_signature_clear (&signature);
}
