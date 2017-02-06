// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "components/webcrypto/algorithms/aes.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace webcrypto {

namespace {

const EVP_AEAD* GetAesKwAlgorithmFromKeySize(size_t key_size_bytes) {
  switch (key_size_bytes) {
    case 16:
      return EVP_aead_aes_128_key_wrap();
    case 32:
      return EVP_aead_aes_256_key_wrap();
    default:
      return NULL;
  }
}

Status AesKwEncryptDecrypt(EncryptOrDecrypt mode,
                           const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           std::vector<uint8_t>* buffer) {
  // These length checks are done in order to give a more specific error. These
  // are not required for correctness.
  if ((mode == ENCRYPT && data.byte_length() < 16) ||
      (mode == DECRYPT && data.byte_length() < 24)) {
    return Status::ErrorDataTooSmall();
  }
  if (data.byte_length() % 8)
    return Status::ErrorInvalidAesKwDataLength();

  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  return AeadEncryptDecrypt(mode, raw_key, data,
                            8,             // tag_length_bytes
                            CryptoData(),  // iv
                            CryptoData(),  // additional_data
                            GetAesKwAlgorithmFromKeySize(raw_key.size()),
                            buffer);
}

class AesKwImplementation : public AesAlgorithm {
 public:
  AesKwImplementation()
      : AesAlgorithm(
            blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey,
            "KW") {}

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    return AesKwEncryptDecrypt(ENCRYPT, algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    return AesKwEncryptDecrypt(DECRYPT, algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateAesKwImplementation() {
  return base::WrapUnique(new AesKwImplementation);
}

}  // namespace webcrypto
