// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/webcrypto/algorithm_implementation.h"
#include "components/webcrypto/algorithms/ec.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// TODO(eroman): Support the "raw" format for ECDH key import + export, as
// specified by WebCrypto spec.

// TODO(eroman): Allow id-ecDH in SPKI and PKCS#8 import
// (http://crbug.com/389400)

class EcdhImplementation : public EcAlgorithm {
 public:
  EcdhImplementation()
      : EcAlgorithm(0,
                    blink::WebCryptoKeyUsageDeriveKey |
                        blink::WebCryptoKeyUsageDeriveBits) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoNamedCurve curve) const override {
    // JWK import for ECDH does not enforce any required value for "alg".
    return "";
  }

  Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& base_key,
                    bool has_optional_length_bits,
                    unsigned int optional_length_bits,
                    std::vector<uint8_t>* derived_bytes) const override {
    if (base_key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    // Verify the "publicKey" parameter. The only guarantee from Blink is that
    // it is a valid WebCryptoKey, but it could be any type.
    const blink::WebCryptoKey& public_key =
        algorithm.ecdhKeyDeriveParams()->publicKey();

    if (public_key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorEcdhPublicKeyWrongType();

    // Make sure it is an EC key.
    if (!public_key.algorithm().ecParams())
      return Status::ErrorEcdhPublicKeyWrongType();

    // TODO(eroman): This is not described by the spec:
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=27404
    if (public_key.algorithm().id() != blink::WebCryptoAlgorithmIdEcdh)
      return Status::ErrorEcdhPublicKeyWrongAlgorithm();

    // The public and private keys come from different key pairs, however their
    // curves must match.
    if (public_key.algorithm().ecParams()->namedCurve() !=
        base_key.algorithm().ecParams()->namedCurve()) {
      return Status::ErrorEcdhCurveMismatch();
    }

    crypto::ScopedEC_KEY public_key_ec(
        EVP_PKEY_get1_EC_KEY(GetEVP_PKEY(public_key)));

    const EC_POINT* public_key_point =
        EC_KEY_get0_public_key(public_key_ec.get());

    crypto::ScopedEC_KEY private_key_ec(
        EVP_PKEY_get1_EC_KEY(GetEVP_PKEY(base_key)));

    // The size of the shared secret is the field size in bytes (rounded up).
    // Note that, if rounding was required, the most significant bits of the
    // secret are zero. So for P-521, the maximum length is 528 bits, not 521.
    int field_size_bytes = NumBitsToBytes(
        EC_GROUP_get_degree(EC_KEY_get0_group(private_key_ec.get())));

    // If a desired key length was not specified, default to the field size
    // (rounded up to nearest byte).
    unsigned int length_bits =
        has_optional_length_bits ? optional_length_bits : field_size_bytes * 8;

    // Short-circuit when deriving an empty key.
    // TODO(eroman): ECDH_compute_key() is not happy when given a NULL output.
    //               http://crbug.com/464194.
    if (length_bits == 0) {
      derived_bytes->clear();
      return Status::Success();
    }

    if (length_bits > static_cast<unsigned int>(field_size_bytes * 8))
      return Status::ErrorEcdhLengthTooBig(field_size_bytes * 8);

    // Resize to target length in bytes (BoringSSL can operate on a shorter
    // buffer than field_size_bytes).
    derived_bytes->resize(NumBitsToBytes(length_bits));

    int result = ECDH_compute_key(derived_bytes->data(), derived_bytes->size(),
                                  public_key_point, private_key_ec.get(), 0);
    if (result < 0 || static_cast<size_t>(result) != derived_bytes->size())
      return Status::OperationError();

    TruncateToBitLength(length_bits, derived_bytes);
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateEcdhImplementation() {
  return base::WrapUnique(new EcdhImplementation);
}

}  // namespace webcrypto
