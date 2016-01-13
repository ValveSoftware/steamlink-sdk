#include "testutils.h"
#include "camellia.h"

static void
test_invert(const struct tstring *key,
	    const struct tstring *cleartext,
	    const struct tstring *ciphertext)
{
  struct camellia_ctx encrypt;
  struct camellia_ctx decrypt;
  uint8_t *data;
  unsigned length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  data = xalloc(length);

  camellia_set_encrypt_key (&encrypt, key->length, key->data);
  camellia_crypt (&encrypt, length, data, cleartext->data);
  
  if (!MEMEQ(length, data, ciphertext->data))
    {
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }

  camellia_invert_key (&decrypt, &encrypt);
  camellia_crypt (&decrypt, length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "test_invert: Decrypt failed:\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }
  free (data);
}

void
test_main(void)
{
  /* Test vectors from RFC 3713 */
  /* 128 bit keys */
  test_cipher(&nettle_camellia128,
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("67 67 31 38 54 96 69 73 08 57 06 56 48 ea be 43"));

  /* 192 bit keys */
  test_cipher(&nettle_camellia192, 
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"
		   "00 11 22 33 44 55 66 77"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("b4 99 34 01 b3 e9 96 f8 4e e5 ce e7 d7 9b 09 b9"));

  /* 256 bit keys */
  test_cipher(&nettle_camellia256, 
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"
		   "00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("9a cc 23 7d ff 16 d7 6c 20 ef 7c 91 9e 3a 75 09"));

  /* Test camellia_invert_key with src != dst */
  test_invert(SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("67 67 31 38 54 96 69 73 08 57 06 56 48 ea be 43"));
  
  test_invert(SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"
		   "00 11 22 33 44 55 66 77"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("b4 99 34 01 b3 e9 96 f8 4e e5 ce e7 d7 9b 09 b9"));

  test_invert(SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"
		   "00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff"),
	      SHEX("01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10"),
	      SHEX("9a cc 23 7d ff 16 d7 6c 20 ef 7c 91 9e 3a 75 09"));
}
