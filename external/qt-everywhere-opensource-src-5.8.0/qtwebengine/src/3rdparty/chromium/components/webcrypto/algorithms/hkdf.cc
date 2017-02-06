// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/secret_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

const blink::WebCryptoKeyUsageMask kValidUsages =
    blink::WebCryptoKeyUsageDeriveKey | blink::WebCryptoKeyUsageDeriveBits;

class HkdfImplementation : public AlgorithmImplementation {
 public:
  HkdfImplementation() {}

  Status VerifyKeyUsagesBeforeImportKey(
      blink::WebCryptoKeyFormat format,
      blink::WebCryptoKeyUsageMask usages) const override {
    if (format != blink::WebCryptoKeyFormatRaw)
      return Status::ErrorUnsupportedImportKeyFormat();
    return CheckSecretKeyCreationUsages(kValidUsages, usages);
  }

  Status ImportKeyRaw(const CryptoData& key_data,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usages,
                      blink::WebCryptoKey* key) const override {
    return CreateWebCryptoSecretKey(
        key_data, blink::WebCryptoKeyAlgorithm::createWithoutParams(
                      blink::WebCryptoAlgorithmIdHkdf),
        extractable, usages, key);
  }

  Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& base_key,
                    bool has_optional_length_bits,
                    unsigned int optional_length_bits,
                    std::vector<uint8_t>* derived_bytes) const override {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    if (!has_optional_length_bits)
      return Status::ErrorHkdfDeriveBitsLengthNotSpecified();

    const blink::WebCryptoHkdfParams* params = algorithm.hkdfParams();

    const EVP_MD* digest_algorithm = GetDigest(params->hash());
    if (!digest_algorithm)
      return Status::ErrorUnsupported();

    // Size output to fit length
    unsigned int derived_bytes_len = NumBitsToBytes(optional_length_bits);
    derived_bytes->resize(derived_bytes_len);

    // Algorithm dispatch checks that the algorithm in |base_key| matches
    // |algorithm|.
    const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(base_key);
    if (!HKDF(derived_bytes->data(), derived_bytes_len, digest_algorithm,
              raw_key.data(), raw_key.size(), params->salt().data(),
              params->salt().size(), params->info().data(),
              params->info().size())) {
      uint32_t error = ERR_get_error();
      if (ERR_GET_LIB(error) == ERR_LIB_HKDF &&
          ERR_GET_REASON(error) == HKDF_R_OUTPUT_TOO_LARGE) {
        return Status::ErrorHkdfLengthTooLong();
      }
      return Status::OperationError();
    }

    TruncateToBitLength(optional_length_bits, derived_bytes);
    return Status::Success();
  }

  Status DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                                blink::WebCryptoKeyType type,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                const CryptoData& key_data,
                                blink::WebCryptoKey* key) const override {
    return CreateWebCryptoSecretKey(key_data, algorithm, extractable, usages,
                                    key);
  }

  Status GetKeyLength(const blink::WebCryptoAlgorithm& key_length_algorithm,
                      bool* has_length_bits,
                      unsigned int* length_bits) const override {
    *has_length_bits = false;
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateHkdfImplementation() {
  return base::WrapUnique(new HkdfImplementation);
}

}  // namespace webcrypto
