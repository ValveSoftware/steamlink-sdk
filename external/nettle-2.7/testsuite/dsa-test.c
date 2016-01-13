#include "testutils.h"

void
test_main(void)
{
  struct dsa_public_key pub;
  struct dsa_private_key key;
  struct dsa_signature expected;
  
  dsa_public_key_init(&pub);
  dsa_private_key_init(&key);
  dsa_signature_init(&expected);

  mpz_set_str(pub.p,
	      "83d9a7c2ce2a9179f43cdb3bffe7de0f0eef26dd5dfae44d"
	      "531bc0de45634d2c07cb929b0dbe10da580070e6abfbb841"
	      "5c44bff570b8ad779df653aad97dc7bdeb815d7e88103e61"
	      "606ed3d8a295fbfd340d2d49e220833ebace5511e22c4f02"
	      "97ed351e9948fa848e9c8fadb7b47bcc47def4255b5e1d5e"
	      "10215b3b55a0b85f", 16);
  mpz_set_str(pub.q,
	      "8266e0deaf46020ba48d410ca580f3a978629b5d", 16);
  mpz_set_str(pub.g,
	      "30d34bb9f376bec947154afe4076bc7d359c9d32f5471ddb"
	      "be8d6a941c47fa9dc4f32573151dbb4aa59eb989b74ac36b"
	      "b6310a5e8b580501655d91f393daa193ae1303049b87febb"
	      "093dc0404b53b4c5da2463300f9c5b156d788c4ace8ecbb9"
	      "dd00c18d99537f255ac025d074d894a607cbe3023a1276ef"
	      "556916a33f7de543", 16);
  mpz_set_str(pub.y,
	      "64402048b27f39f404a546a84909c9c0e9e2dd153a849946"
	      "1062892598d30af27ae3cefc2b700fb6d077390a83bdcad7"
	      "8a1299487c9623bb62af0c85a3df9ef1ee2c0d66658e1fd3"
	      "283b5407f6cd30ee7e6154fad41a6a8b0f5c86c5accc1127"
	      "bf7c9a5d6badcb012180cb62a55c5e17d6d3528cdbe002cc"
	      "ee131c1b86867f7a", 16);
  mpz_set_str(key.x,
	      "56c6efaf878d06eef21dc070fab71da6ec1e30a6", 16);

  test_dsa_key(&pub, &key, 160);

  mpz_set_str(expected.r, "180342f8d4fb5bd0311ebf205bdee6e556014eaf", 16);
  mpz_set_str(expected.s, "392dc6566b2735531a8460966171464ef7ddfe12", 16);

  test_dsa160(&pub, &key, &expected);

  mpz_set_str(pub.p,
	      "fda45d8f1df8f2b84fb3cf9ae69f93b087d98bea282f643e"
	      "23472c5b57605952010e4c846d711f2783e8ad4e1447698e"
	      "2e328fdb1d411ccb0f3caef5b8fc0b9dcecfadf022ecc7de"
	      "5c153c8f10fe88d63abf7d296ad485dfd6eead595fc1c36b"
	      "8bd42e8668b55b2bb0f1a6aecbe678df504880de2481a5e4"
	      "97d1b7d92ee48ffeb083a1833094a0418ec0d914409c720c"
	      "87ea63c164ec448c471b574a8f88073ebeb44dc6d6b98260"
	      "46126f03022ff04dcb6a2381a09b0a227d3c57cfbfd48e4a"
	      "19cbb0a35242c9e234ebe105ae26cab01ede40aa2869fad8"
	      "6bff57a19ec87b8de294ca03269c268c10813f18169beac5"
	      "ac97c0e748ccb244282c50c670e1bccb", 16);
  mpz_set_str(pub.q,
	      "bd612630da4d930779a32546dc413efd299111b443c7355d"
	      "65d991163cc3cd9d", 16);
  mpz_set_str(pub.g,
	      "050c56e14adb03e47d3902852f5b21c96c28a2aa89619c8b"
	      "78a98aa5083700994f99184588d2cefaf2a3ea213dd2d084"
	      "0e682a52357d5fefaef44520622f021855744d638e792f21"
	      "89543f9770aa1960da4d7b325a37a2922b035c8da3d71543"
	      "5d7a6ddefc62e84fe76fecbbf9667c6a1781a84aa434548b"
	      "bdc315f2fb0a420d65c1f72911845b148e994660138052a1"
	      "fce1c6f933be155b2af8c0177277cd3b75d9477ddbcb77bc"
	      "f5cccea915a2f3750ba41f337edd44f768cb3d24b17e299d"
	      "5cebe5e78cbaf5ad41e815edfc71df3131bd5359c653a224"
	      "bd3ac6a27bad7efff11b24fad0109ee26e4df76fc99e150d"
	      "666a9294bab8a03f113d228bfad349f4", 16);
  mpz_set_str(pub.y,
	      "da7f9abb0b554afaa926c9cffa897239bfdbc58ed9981748"
	      "edb1e38f42dea0560a407a48b509a5cb460bf31dee9057a0"
	      "b41d468698fa82ff03c47e8f3f6564c74d6f1daa5f84ad25"
	      "b937317f861fa68c19e20d6b855e85cd94d5af95b968416e"
	      "6d43711f24d5497f018b7627d2bed25dc793ddb897fdcc34"
	      "5d183e43a80205483dea7a12185be3b185a7d84d3385b962"
	      "4485882722d177ccd8f49c5b519fb96b9b59fcfc63422f25"
	      "88fb8ff00bce46acb7c80d105c31414ecf5be0a0dad975bd"
	      "dcd83d6f063f9bce562fdd5b68e18fc2159dbb2457adc7a7"
	      "ee5bc0796eff370908f866a41b9a8873f89e1904925141f8"
	      "e574df25bd869f43a184a804e8ce5fcc", 16);
  mpz_set_str(key.x,
	      "39f84f88569da55c6bee7e18175b539ea9b7ee24fabd85a7"
	      "1fa8c93b7181545b", 16);

  test_dsa_key(&pub, &key, 256); 

  mpz_set_str(expected.r,
	      "03fe95c9dbbe1be019d7914e45c37c70"
	      "0f499f559312a59f3bc5037f51d3f74c", 16);
  mpz_set_str(expected.s,
	      "839dbee8d30e6b0cc349528f900f30ee"
	      "6d4ce9864332d07c87b5a98bd75dbdbb", 16);

  test_dsa256(&pub, &key, &expected);

  dsa_public_key_clear(&pub);
  dsa_private_key_clear(&key);
  dsa_signature_clear(&expected);
}
