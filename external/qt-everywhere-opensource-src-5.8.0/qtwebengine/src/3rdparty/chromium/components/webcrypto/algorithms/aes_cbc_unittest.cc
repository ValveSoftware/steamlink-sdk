// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Creates an AES-CBC algorithm.
blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    const std::vector<uint8_t>& iv) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesCbcParams(iv.data(),
                                       static_cast<unsigned int>(iv.size())));
}

blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesCbc,
                                  key_length_bits);
}

blink::WebCryptoKey GetTestAesCbcKey() {
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      HexStringToBytes(key_hex),
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
  return key;
}

class WebCryptoAesCbcTest : public WebCryptoTestBase {};

TEST_F(WebCryptoAesCbcTest, InputTooLarge) {
  std::vector<uint8_t> output;

  std::vector<uint8_t> iv(16);

  // Give an input that is too large. It would cause integer overflow when
  // narrowing the ciphertext size to an int, since OpenSSL operates on signed
  // int lengths NOT unsigned.
  //
  // Pretend the input is large. Don't pass data pointer as NULL in case that
  // is special cased; the implementation shouldn't actually dereference the
  // data.
  CryptoData input(&iv[0], INT_MAX - 3);

  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Encrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Decrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
}

TEST_F(WebCryptoAesCbcTest, ExportKeyUnsupportedFormat) {
  std::vector<uint8_t> output;

  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::WebCryptoKeyFormatSpki, GetTestAesCbcKey(), &output));
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::WebCryptoKeyFormatPkcs8, GetTestAesCbcKey(), &output));
}

// Tests importing of keys (in a variety of formats), errors during import,
// encryption, and decryption, using known answers.
TEST_F(WebCryptoAesCbcTest, KnownAnswerEncryptDecrypt) {
  std::unique_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_cbc.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    std::string import_error = "Success";
    test->GetString("import_error", &import_error);

    // Import the key.
    blink::WebCryptoKey key;
    Status status = ImportKey(
        key_format, CryptoData(key_data),
        CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), true,
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
        &key);
    ASSERT_EQ(import_error, StatusToString(status));
    if (status.IsError())
      continue;

    // Test encryption.
    if (test->HasKey("plain_text")) {
      std::vector<uint8_t> test_plain_text =
          GetBytesFromHexString(test, "plain_text");

      std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");

      std::string encrypt_error = "Success";
      test->GetString("encrypt_error", &encrypt_error);

      std::vector<uint8_t> output;
      status = Encrypt(CreateAesCbcAlgorithm(test_iv), key,
                       CryptoData(test_plain_text), &output);
      ASSERT_EQ(encrypt_error, StatusToString(status));
      if (status.IsError())
        continue;

      std::vector<uint8_t> test_cipher_text =
          GetBytesFromHexString(test, "cipher_text");

      EXPECT_BYTES_EQ(test_cipher_text, output);
    }

    // Test decryption.
    if (test->HasKey("cipher_text")) {
      std::vector<uint8_t> test_cipher_text =
          GetBytesFromHexString(test, "cipher_text");

      std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");

      std::string decrypt_error = "Success";
      test->GetString("decrypt_error", &decrypt_error);

      std::vector<uint8_t> output;
      status = Decrypt(CreateAesCbcAlgorithm(test_iv), key,
                       CryptoData(test_cipher_text), &output);
      ASSERT_EQ(decrypt_error, StatusToString(status));
      if (status.IsError())
        continue;

      std::vector<uint8_t> test_plain_text =
          GetBytesFromHexString(test, "plain_text");

      EXPECT_BYTES_EQ(test_plain_text, output);
    }
  }
}

// TODO(eroman): Do this same test for AES-GCM, AES-KW, AES-CTR ?
TEST_F(WebCryptoAesCbcTest, GenerateKeyIsRandom) {
  // Check key generation for each allowed key length.
  std::vector<blink::WebCryptoAlgorithm> algorithm;
  const unsigned short kKeyLength[] = {128, 256};
  for (size_t key_length_i = 0; key_length_i < arraysize(kKeyLength);
       ++key_length_i) {
    blink::WebCryptoKey key;

    std::vector<std::vector<uint8_t>> keys;
    std::vector<uint8_t> key_bytes;

    // Generate a small sample of keys.
    for (int j = 0; j < 16; ++j) {
      ASSERT_EQ(Status::Success(),
                GenerateSecretKey(
                    CreateAesCbcKeyGenAlgorithm(kKeyLength[key_length_i]), true,
                    blink::WebCryptoKeyUsageEncrypt, &key));
      EXPECT_TRUE(key.handle());
      EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
      ASSERT_EQ(Status::Success(),
                ExportKey(blink::WebCryptoKeyFormatRaw, key, &key_bytes));
      EXPECT_EQ(key_bytes.size() * 8,
                key.algorithm().aesParams()->lengthBits());
      keys.push_back(key_bytes);
    }
    // Ensure all entries in the key sample set are unique. This is a simplistic
    // estimate of whether the generated keys appear random.
    EXPECT_FALSE(CopiesExist(keys));
  }
}

TEST_F(WebCryptoAesCbcTest, GenerateKeyBadLength) {
  const unsigned short kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key;
  for (size_t i = 0; i < arraysize(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Status::ErrorGenerateAesKeyLength(),
              GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(kKeyLen[i]), true,
                                blink::WebCryptoKeyUsageEncrypt, &key));
  }
}

TEST_F(WebCryptoAesCbcTest, ImportKeyEmptyUsage) {
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::WebCryptoKeyFormatRaw,
                      CryptoData(std::vector<uint8_t>(16)),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), true,
                      0, &key));
}

// If key_ops is specified but empty, no key usages are allowed for the key.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkEmptyKeyOps) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  dict.Set("key_ops", new base::ListValue);  // Takes ownership.

  // The JWK does not contain encrypt usages.
  EXPECT_EQ(Status::ErrorJwkKeyopsInconsistent(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageEncrypt, &key));

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageSign, &key));
}

// If key_ops is missing, then any key usages can be specified.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkNoKeyOps) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageEncrypt, &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageVerify, &key));
}

TEST_F(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsEncryptDecrypt) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  key_ops->AppendString("encrypt");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageEncrypt, &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  key_ops->AppendString("decrypt");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageDecrypt, &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageDecrypt, key.usages());

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
          blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageEncrypt,
          &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
            key.usages());
}

// Test failure if input usage is NOT a strict subset of the JWK usage.
TEST_F(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsNotSuperset) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  key_ops->AppendString("encrypt");

  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
          blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
          &key));
}

TEST_F(WebCryptoAesCbcTest, ImportKeyJwkUseEnc) {
  blink::WebCryptoKey key;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  // Test JWK composite use 'enc' usage
  dict.SetString("alg", "A128CBC");
  dict.SetString("use", "enc");
  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
          blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageEncrypt |
              blink::WebCryptoKeyUsageWrapKey |
              blink::WebCryptoKeyUsageUnwrapKey,
          &key));
  EXPECT_EQ(blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageEncrypt |
                blink::WebCryptoKeyUsageWrapKey |
                blink::WebCryptoKeyUsageUnwrapKey,
            key.usages());
}

TEST_F(WebCryptoAesCbcTest, ImportJwkInvalidJson) {
  blink::WebCryptoKey key;
  // Fail on empty JSON.
  EXPECT_EQ(
      Status::ErrorJwkNotDictionary(),
      ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(MakeJsonVector("")),
                CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageEncrypt, &key));

  // Fail on invalid JSON.
  const std::vector<uint8_t> bad_json_vec = MakeJsonVector(
      "{"
      "\"kty\"         : \"oct\","
      "\"alg\"         : \"HS256\","
      "\"use\"         : ");
  EXPECT_EQ(Status::ErrorJwkNotDictionary(),
            ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(bad_json_vec),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                      blink::WebCryptoKeyUsageEncrypt, &key));
}

// Fail on inconsistent key_ops - asking for "encrypt" however JWK contains
// only "foo".
TEST_F(WebCryptoAesCbcTest, ImportJwkKeyOpsLacksUsages) {
  blink::WebCryptoKey key;

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg");

  base::ListValue* key_ops = new base::ListValue;
  // Note: the following call makes dict assume ownership of key_ops.
  dict.Set("key_ops", key_ops);
  key_ops->AppendString("foo");
  EXPECT_EQ(Status::ErrorJwkKeyopsInconsistent(),
            ImportKeyJwkFromDict(
                dict, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), false,
                blink::WebCryptoKeyUsageEncrypt, &key));
}

TEST_F(WebCryptoAesCbcTest, ImportExportJwk) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  // AES-CBC 128
  ImportExportJwkSymmetricKey(
      128, algorithm,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      "A128CBC");

  // AES-CBC 256
  ImportExportJwkSymmetricKey(256, algorithm, blink::WebCryptoKeyUsageDecrypt,
                              "A256CBC");

  // Large usage value
  ImportExportJwkSymmetricKey(
      256, algorithm,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
          blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey,
      "A256CBC");
}

// 192-bit AES is intentionally unsupported (http://crbug.com/533699).
TEST_F(WebCryptoAesCbcTest, GenerateAesCbc192) {
  blink::WebCryptoKey key;
  Status status = GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(192), true,
                                    blink::WebCryptoKeyUsageEncrypt, &key);
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(), status);
}

// 192-bit AES is intentionally unsupported (http://crbug.com/533699).
TEST_F(WebCryptoAesCbcTest, UnwrapAesCbc192) {
  std::vector<uint8_t> wrapping_key_data(16, 0);
  std::vector<uint8_t> wrapped_key = HexStringToBytes(
      "1A07ACAB6C906E50883173C29441DB1DE91D34F45C435B5F99C822867FB3956F");

  blink::WebCryptoKey wrapping_key = ImportSecretKeyFromRaw(
      wrapping_key_data, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw),
      blink::WebCryptoKeyUsageUnwrapKey);

  blink::WebCryptoKey unwrapped_key;
  ASSERT_EQ(
      Status::ErrorAes192BitUnsupported(),
      UnwrapKey(blink::WebCryptoKeyFormatRaw, CryptoData(wrapped_key),
                wrapping_key, CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw),
                CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), true,
                blink::WebCryptoKeyUsageEncrypt, &unwrapped_key));
}

// Try importing an AES-CBC key with unsupported key usages using raw
// format. AES-CBC keys support the following usages:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST_F(WebCryptoAesCbcTest, ImportKeyBadUsage_Raw) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageDecrypt,
      blink::WebCryptoKeyUsageDeriveBits,
      blink::WebCryptoKeyUsageUnwrapKey | blink::WebCryptoKeyUsageVerify,
  };

  std::vector<uint8_t> key_bytes(16);

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKey(blink::WebCryptoKeyFormatRaw, CryptoData(key_bytes),
                        algorithm, true, bad_usages[i], &key));
  }
}

// Generate an AES-CBC key with invalid usages. AES-CBC supports:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST_F(WebCryptoAesCbcTest, GenerateKeyBadUsages) {
  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign,
      blink::WebCryptoKeyUsageVerify,
      blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageVerify,
  };

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey key;

    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true,
                                bad_usages[i], &key));
  }
}

// Generate an AES-CBC key with no usages.
TEST_F(WebCryptoAesCbcTest, GenerateKeyEmptyUsages) {
  blink::WebCryptoKey key;

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true, 0, &key));
}

// Generate an AES-CBC key and an RSA key pair. Use the AES-CBC key to wrap the
// key pair (using SPKI format for public key, PKCS8 format for private key).
// Then unwrap the wrapped key pair and verify that the key data is the same.
TEST_F(WebCryptoAesCbcTest, WrapUnwrapRoundtripSpkiPkcs8) {
  // Generate the wrapping key.
  blink::WebCryptoKey wrapping_key;
  ASSERT_EQ(Status::Success(),
            GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128), true,
                              blink::WebCryptoKeyUsageWrapKey |
                                  blink::WebCryptoKeyUsageUnwrapKey,
                              &wrapping_key));

  // Generate an RSA key pair to be wrapped.
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, blink::WebCryptoKeyUsageSign, &public_key,
                            &private_key));

  // Export key pair as SPKI + PKCS8
  std::vector<uint8_t> public_key_spki;
  ASSERT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatSpki,
                                         public_key, &public_key_spki));

  std::vector<uint8_t> private_key_pkcs8;
  ASSERT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatPkcs8,
                                         private_key, &private_key_pkcs8));

  // Wrap the key pair.
  blink::WebCryptoAlgorithm wrap_algorithm =
      CreateAesCbcAlgorithm(std::vector<uint8_t>(16, 0));

  std::vector<uint8_t> wrapped_public_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatSpki, public_key, wrapping_key,
                    wrap_algorithm, &wrapped_public_key));

  std::vector<uint8_t> wrapped_private_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatPkcs8, private_key, wrapping_key,
                    wrap_algorithm, &wrapped_private_key));

  // Unwrap the key pair.
  blink::WebCryptoAlgorithm rsa_import_algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256);

  blink::WebCryptoKey unwrapped_public_key;

  ASSERT_EQ(
      Status::Success(),
      UnwrapKey(blink::WebCryptoKeyFormatSpki, CryptoData(wrapped_public_key),
                wrapping_key, wrap_algorithm, rsa_import_algorithm, true,
                blink::WebCryptoKeyUsageVerify, &unwrapped_public_key));

  blink::WebCryptoKey unwrapped_private_key;

  ASSERT_EQ(
      Status::Success(),
      UnwrapKey(blink::WebCryptoKeyFormatPkcs8, CryptoData(wrapped_private_key),
                wrapping_key, wrap_algorithm, rsa_import_algorithm, true,
                blink::WebCryptoKeyUsageSign, &unwrapped_private_key));

  // Export unwrapped key pair as SPKI + PKCS8
  std::vector<uint8_t> unwrapped_public_key_spki;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatSpki, unwrapped_public_key,
                      &unwrapped_public_key_spki));

  std::vector<uint8_t> unwrapped_private_key_pkcs8;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatPkcs8, unwrapped_private_key,
                      &unwrapped_private_key_pkcs8));

  EXPECT_EQ(public_key_spki, unwrapped_public_key_spki);
  EXPECT_EQ(private_key_pkcs8, unwrapped_private_key_pkcs8);

  EXPECT_NE(public_key_spki, wrapped_public_key);
  EXPECT_NE(private_key_pkcs8, wrapped_private_key);
}

}  // namespace

}  // namespace webcrypto
