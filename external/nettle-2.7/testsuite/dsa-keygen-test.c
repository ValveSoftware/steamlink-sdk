#include "testutils.h"

#include "knuth-lfib.h"

static void
progress(void *ctx UNUSED, int c)
{
  fputc(c, stderr);
}

void
test_main(void)
{
  struct dsa_public_key pub;
  struct dsa_private_key key;
  
  struct knuth_lfib_ctx lfib;
  
  dsa_private_key_init(&key);
  dsa_public_key_init(&pub);

  knuth_lfib_init(&lfib, 13);

  ASSERT (dsa_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       1024, 160));

  test_dsa_key(&pub, &key, 160);
  test_dsa160(&pub, &key, NULL);

  ASSERT (dsa_generate_keypair(&pub, &key,
			       &lfib,
			       (nettle_random_func *) knuth_lfib_random,
			       NULL, verbose ? progress : NULL,
			       2048, 256));

  test_dsa_key(&pub, &key, 256);
  test_dsa256(&pub, &key, NULL);
  
  dsa_public_key_clear(&pub);
  dsa_private_key_clear(&key);
}
