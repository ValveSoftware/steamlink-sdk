// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/util.h"

#include <openssl/aead.h>
#include <openssl/bn.h>
#include <openssl/digest.h>

#include "base/logging.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace webcrypto {

const EVP_MD* GetDigest(const blink::WebCryptoAlgorithm& hash_algorithm) {
  return GetDigest(hash_algorithm.id());
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::WebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::WebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::WebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::WebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return NULL;
  }
}

void TruncateToBitLength(size_t length_bits, std::vector<uint8_t>* bytes) {
  size_t length_bytes = NumBitsToBytes(length_bits);

  if (bytes->size() != length_bytes) {
    CHECK_LT(length_bytes, bytes->size());
    bytes->resize(length_bytes);
  }

  size_t remainder_bits = length_bits % 8;

  // Zero any "unused bits" in the final byte.
  if (remainder_bits)
    (*bytes)[bytes->size() - 1] &= ~((0xFF) >> remainder_bits);
}

Status CheckKeyCreationUsages(blink::WebCryptoKeyUsageMask all_possible_usages,
                              blink::WebCryptoKeyUsageMask actual_usages,
                              EmptyUsagePolicy empty_usage_policy) {
  if (actual_usages == 0 &&
      empty_usage_policy == EmptyUsagePolicy::REJECT_EMPTY) {
    return Status::ErrorCreateKeyEmptyUsages();
  }

  if (!ContainsKeyUsages(all_possible_usages, actual_usages))
    return Status::ErrorCreateKeyBadUsages();
  return Status::Success();
}

bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b) {
  return (a & b) == b;
}

BIGNUM* CreateBIGNUM(const std::string& n) {
  return BN_bin2bn(reinterpret_cast<const uint8_t*>(n.data()), n.size(), NULL);
}

Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          const std::vector<uint8_t>& raw_key,
                          const CryptoData& data,
                          unsigned int tag_length_bytes,
                          const CryptoData& iv,
                          const CryptoData& additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  EVP_AEAD_CTX ctx;

  if (!aead_alg)
    return Status::ErrorUnexpected();

  if (!EVP_AEAD_CTX_init(&ctx, aead_alg, raw_key.data(), raw_key.size(),
                         tag_length_bytes, NULL)) {
    return Status::OperationError();
  }

  crypto::ScopedOpenSSL<EVP_AEAD_CTX, EVP_AEAD_CTX_cleanup> ctx_cleanup(&ctx);

  size_t len;
  int ok;

  if (mode == DECRYPT) {
    if (data.byte_length() < tag_length_bytes)
      return Status::ErrorDataTooSmall();

    buffer->resize(data.byte_length() - tag_length_bytes);

    ok = EVP_AEAD_CTX_open(&ctx, buffer->data(), &len, buffer->size(),
                           iv.bytes(), iv.byte_length(), data.bytes(),
                           data.byte_length(), additional_data.bytes(),
                           additional_data.byte_length());
  } else {
    // No need to check for unsigned integer overflow here (seal fails if
    // the output buffer is too small).
    buffer->resize(data.byte_length() + EVP_AEAD_max_overhead(aead_alg));

    ok = EVP_AEAD_CTX_seal(&ctx, buffer->data(), &len, buffer->size(),
                           iv.bytes(), iv.byte_length(), data.bytes(),
                           data.byte_length(), additional_data.bytes(),
                           additional_data.byte_length());
  }

  if (!ok)
    return Status::OperationError();
  buffer->resize(len);
  return Status::Success();
}

}  // namespace webcrypto
