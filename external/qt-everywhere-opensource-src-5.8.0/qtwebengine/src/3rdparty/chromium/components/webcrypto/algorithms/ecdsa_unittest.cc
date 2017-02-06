// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateEcdsaKeyGenAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdEcdsa,
      new blink::WebCryptoEcKeyGenParams(named_curve));
}

blink::WebCryptoAlgorithm CreateEcdsaImportAlgorithm(
    blink::WebCryptoNamedCurve named_curve) {
  return CreateEcImportAlgorithm(blink::WebCryptoAlgorithmIdEcdsa, named_curve);
}

blink::WebCryptoAlgorithm CreateEcdsaAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdEcdsa,
      new blink::WebCryptoEcdsaParams(CreateAlgorithm(hash_id)));
}

class WebCryptoEcdsaTest : public WebCryptoTestBase {};

// Generates some ECDSA key pairs. Validates basic properties on the keys, and
// ensures the serialized key (as JWK) is unique. This test does nothing to
// ensure that the keys are otherwise usable (by trying to sign/verify with
// them).
TEST_F(WebCryptoEcdsaTest, GenerateKeyIsRandom) {
  blink::WebCryptoNamedCurve named_curve = blink::WebCryptoNamedCurveP256;

  std::vector<std::vector<uint8_t>> serialized_keys;

  // Generate a small sample of keys.
  for (int j = 0; j < 4; ++j) {
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_EQ(Status::Success(),
              GenerateKeyPair(CreateEcdsaKeyGenAlgorithm(named_curve), true,
                              blink::WebCryptoKeyUsageSign, &public_key,
                              &private_key));

    // Basic sanity checks on the generated key pair.
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
    EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
    EXPECT_EQ(named_curve, public_key.algorithm().ecParams()->namedCurve());
    EXPECT_EQ(named_curve, private_key.algorithm().ecParams()->namedCurve());

    // Export the key pair to JWK.
    std::vector<uint8_t> key_bytes;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, public_key, &key_bytes));
    serialized_keys.push_back(key_bytes);

    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, private_key, &key_bytes));
    serialized_keys.push_back(key_bytes);
  }

  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(serialized_keys));
}

TEST_F(WebCryptoEcdsaTest, GenerateKeyEmptyUsage) {
  blink::WebCryptoNamedCurve named_curve = blink::WebCryptoNamedCurveP256;
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateKeyPair(CreateEcdsaKeyGenAlgorithm(named_curve), true, 0,
                            &public_key, &private_key));
}

// Verify that ECDSA signatures are probabilistic. Signing the same message two
// times should yield different signatures. However both signatures should
// verify correctly.
TEST_F(WebCryptoEcdsaTest, SignatureIsRandom) {
  // Import a public and private keypair from "ec_private_keys.json". It doesn't
  // really matter which one is used since they are all valid. In this case
  // using the first one.
  std::unique_ptr<base::ListValue> private_keys;
  ASSERT_TRUE(ReadJsonTestFileToList("ec_private_keys.json", &private_keys));
  const base::DictionaryValue* key_dict;
  ASSERT_TRUE(private_keys->GetDictionary(0, &key_dict));
  blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(key_dict);
  const base::DictionaryValue* key_jwk;
  ASSERT_TRUE(key_dict->GetDictionary("jwk", &key_jwk));

  blink::WebCryptoKey private_key;
  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(*key_jwk, CreateEcdsaImportAlgorithm(curve), true,
                           blink::WebCryptoKeyUsageSign, &private_key));

  // Erase the "d" member so the private key JWK can be used to import the
  // public key (WebCrypto doesn't provide a mechanism for importing a public
  // key given a private key).
  std::unique_ptr<base::DictionaryValue> key_jwk_copy(key_jwk->DeepCopy());
  key_jwk_copy->Remove("d", NULL);
  blink::WebCryptoKey public_key;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(*key_jwk_copy.get(),
                                 CreateEcdsaImportAlgorithm(curve), true,
                                 blink::WebCryptoKeyUsageVerify, &public_key));

  // Sign twice
  std::vector<uint8_t> message(10);
  blink::WebCryptoAlgorithm algorithm =
      CreateEcdsaAlgorithm(blink::WebCryptoAlgorithmIdSha1);

  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, CryptoData(message), &signature1));
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, CryptoData(message), &signature2));

  // The two signatures should be different.
  EXPECT_NE(CryptoData(signature1), CryptoData(signature2));

  // And both should be valid signatures which can be verified.
  bool signature_matches;
  ASSERT_EQ(Status::Success(),
            Verify(algorithm, public_key, CryptoData(signature1),
                   CryptoData(message), &signature_matches));
  EXPECT_TRUE(signature_matches);
  ASSERT_EQ(Status::Success(),
            Verify(algorithm, public_key, CryptoData(signature2),
                   CryptoData(message), &signature_matches));
  EXPECT_TRUE(signature_matches);
}

// Tests verify() for ECDSA using an assortment of keys, curves and hashes.
// These tests also include expected failures for bad signatures and keys.
TEST_F(WebCryptoEcdsaTest, VerifyKnownAnswer) {
  std::unique_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("ecdsa.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    const base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(test);
    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);

    // If the test didn't specify an error, that implies it expects success.
    std::string expected_error = "Success";
    test->GetString("error", &expected_error);

    // Import the public key.
    blink::WebCryptoKey key;
    Status status = ImportKey(key_format, CryptoData(key_data),
                              CreateEcdsaImportAlgorithm(curve), true,
                              blink::WebCryptoKeyUsageVerify, &key);
    ASSERT_EQ(expected_error, StatusToString(status));
    if (status.IsError())
      continue;

    // Basic sanity checks on the imported public key.
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());
    EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
    EXPECT_EQ(curve, key.algorithm().ecParams()->namedCurve());

    // Now try to verify the given message and signature.
    std::vector<uint8_t> message = GetBytesFromHexString(test, "msg");
    std::vector<uint8_t> signature = GetBytesFromHexString(test, "sig");
    blink::WebCryptoAlgorithm hash = GetDigestAlgorithm(test, "hash");

    bool verify_result;
    status = Verify(CreateEcdsaAlgorithm(hash.id()), key, CryptoData(signature),
                    CryptoData(message), &verify_result);
    ASSERT_EQ(expected_error, StatusToString(status));
    if (status.IsError())
      continue;

    // If no error was expected, the verification's boolean must match
    // "verify_result" for the test.
    bool expected_result = false;
    ASSERT_TRUE(test->GetBoolean("verify_result", &expected_result));
    EXPECT_EQ(expected_result, verify_result);
  }
}

// The test file may include either public or private keys. In order to import
// them successfully, the correct usages need to be specified. This function
// determines what usages to use for the key.
blink::WebCryptoKeyUsageMask GetExpectedUsagesForKeyImport(
    blink::WebCryptoKeyFormat key_format,
    const base::DictionaryValue* test) {
  blink::WebCryptoKeyUsageMask kPublicUsages = blink::WebCryptoKeyUsageVerify;
  blink::WebCryptoKeyUsageMask kPrivateUsages = blink::WebCryptoKeyUsageSign;

  switch (key_format) {
    case blink::WebCryptoKeyFormatRaw:
    case blink::WebCryptoKeyFormatSpki:
      return kPublicUsages;
    case blink::WebCryptoKeyFormatPkcs8:
      return kPrivateUsages;
      break;
    case blink::WebCryptoKeyFormatJwk: {
      const base::DictionaryValue* key = NULL;
      if (!test->GetDictionary("key", &key))
        ADD_FAILURE() << "Missing key property";
      return key->HasKey("d") ? kPrivateUsages : kPublicUsages;
    }
  }

  // Appease compiler.
  return kPrivateUsages;
}

// Tests importing bad public/private keys in a variety of formats.
TEST_F(WebCryptoEcdsaTest, ImportBadKeys) {
  std::unique_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("bad_ec_keys.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    const base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(test);
    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    std::string expected_error;
    ASSERT_TRUE(test->GetString("error", &expected_error));

    blink::WebCryptoKey key;
    Status status = ImportKey(
        key_format, CryptoData(key_data), CreateEcdsaImportAlgorithm(curve),
        true, GetExpectedUsagesForKeyImport(key_format, test), &key);
    ASSERT_EQ(expected_error, StatusToString(status));
  }
}

// Tests importing and exporting of EC private keys, using both JWK and PKCS8
// formats.
//
// The test imports a key first using JWK, and then exporting it to JWK and
// PKCS8. It does the same thing using PKCS8 as the original source of truth.
TEST_F(WebCryptoEcdsaTest, ImportExportPrivateKey) {
  std::unique_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("ec_private_keys.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    const base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoNamedCurve curve = GetCurveNameFromDictionary(test);
    const base::DictionaryValue* jwk_dict;
    EXPECT_TRUE(test->GetDictionary("jwk", &jwk_dict));
    std::vector<uint8_t> jwk_bytes = MakeJsonVector(*jwk_dict);
    std::vector<uint8_t> pkcs8_bytes = GetBytesFromHexString(
        test, test->HasKey("exported_pkcs8") ? "exported_pkcs8" : "pkcs8");

    // -------------------------------------------------
    // Test from JWK, and then export to {JWK, PKCS8}
    // -------------------------------------------------

    // Import the key using JWK
    blink::WebCryptoKey key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(jwk_bytes),
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::WebCryptoKeyUsageSign, &key));

    // Export the key as JWK
    std::vector<uint8_t> exported_bytes;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, key, &exported_bytes));

    // NOTE: The exported bytes can't be directly compared to jwk_bytes because
    // the exported JWK differs from the imported one. In particular it contains
    // extra properties for extractability and key_ops.
    //
    // Verification is instead done by using the first exported JWK bytes as the
    // expectation.
    jwk_bytes = exported_bytes;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(jwk_bytes),
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::WebCryptoKeyUsageSign, &key));

    // Export the key as JWK (again)
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, key, &exported_bytes));
    EXPECT_EQ(CryptoData(jwk_bytes), CryptoData(exported_bytes));

    // Export the key as PKCS8
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatPkcs8, key, &exported_bytes));
    EXPECT_EQ(CryptoData(pkcs8_bytes), CryptoData(exported_bytes));

    // -------------------------------------------------
    // Test from PKCS8, and then export to {JWK, PKCS8}
    // -------------------------------------------------

    // The imported PKCS8 bytes may differ from the exported bytes (in the case
    // where the publicKey was missing, it will be synthesized and written back
    // during export).
    std::vector<uint8_t> pkcs8_input_bytes = GetBytesFromHexString(
        test, test->HasKey("original_pkcs8") ? "original_pkcs8" : "pkcs8");
    CryptoData pkcs8_input_data(pkcs8_input_bytes.empty() ? pkcs8_bytes
                                                          : pkcs8_input_bytes);

    // Import the key using PKCS8
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatPkcs8, pkcs8_input_data,
                        CreateEcdsaImportAlgorithm(curve), true,
                        blink::WebCryptoKeyUsageSign, &key));

    // Export the key as PKCS8
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatPkcs8, key, &exported_bytes));
    EXPECT_EQ(CryptoData(pkcs8_bytes), CryptoData(exported_bytes));

    // Export the key as JWK
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, key, &exported_bytes));
    EXPECT_EQ(CryptoData(jwk_bytes), CryptoData(exported_bytes));
  }
}

}  // namespace

}  // namespace webcrypto
