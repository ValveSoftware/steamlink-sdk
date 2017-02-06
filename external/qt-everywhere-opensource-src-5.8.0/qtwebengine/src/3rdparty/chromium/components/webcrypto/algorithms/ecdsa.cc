// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdsa.h>
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

// Extracts the OpenSSL key and digest from a WebCrypto key + algorithm. The
// returned pkey pointer will remain valid as long as |key| is alive.
Status GetPKeyAndDigest(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        EVP_PKEY** pkey,
                        const EVP_MD** digest) {
  *pkey = GetEVP_PKEY(key);
  *digest = GetDigest(algorithm.ecdsaParams()->hash());
  if (!*digest)
    return Status::ErrorUnsupported();
  return Status::Success();
}

// Gets the EC key's order size in bytes.
Status GetEcGroupOrderSize(EVP_PKEY* pkey, size_t* order_size_bytes) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedEC_KEY ec(EVP_PKEY_get1_EC_KEY(pkey));
  if (!ec.get())
    return Status::ErrorUnexpected();

  const EC_GROUP* group = EC_KEY_get0_group(ec.get());

  crypto::ScopedBIGNUM order(BN_new());
  if (!EC_GROUP_get_order(group, order.get(), NULL))
    return Status::OperationError();

  *order_size_bytes = BN_num_bytes(order.get());
  return Status::Success();
}

// Formats a DER-encoded signature (ECDSA-Sig-Value as specified in RFC 3279) to
// the signature format expected by WebCrypto (raw concatenated "r" and "s").
//
// TODO(eroman): Where is the specification for WebCrypto's signature format?
Status ConvertDerSignatureToWebCryptoSignature(
    EVP_PKEY* key,
    std::vector<uint8_t>* signature) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedECDSA_SIG ecdsa_sig(
      ECDSA_SIG_from_bytes(signature->data(), signature->size()));
  if (!ecdsa_sig.get())
    return Status::ErrorUnexpected();

  // Determine the maximum length of r and s.
  size_t order_size_bytes;
  Status status = GetEcGroupOrderSize(key, &order_size_bytes);
  if (status.IsError())
    return status;

  signature->resize(order_size_bytes * 2);

  if (!BN_bn2bin_padded(signature->data(), order_size_bytes,
                        ecdsa_sig.get()->r)) {
    return Status::ErrorUnexpected();
  }

  if (!BN_bn2bin_padded(&(*signature)[order_size_bytes], order_size_bytes,
                        ecdsa_sig.get()->s)) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

// Formats a WebCrypto ECDSA signature to a DER-encoded signature
// (ECDSA-Sig-Value as specified in RFC 3279).
//
// TODO(eroman): What is the specification for WebCrypto's signature format?
//
// If the signature length is incorrect (not 2 * order_size), then
// Status::Success() is returned and |*incorrect_length| is set to true;
//
// Otherwise on success, der_signature is filled with a ASN.1 encoded
// ECDSA-Sig-Value.
Status ConvertWebCryptoSignatureToDerSignature(
    EVP_PKEY* key,
    const CryptoData& signature,
    std::vector<uint8_t>* der_signature,
    bool* incorrect_length) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Determine the length of r and s.
  size_t order_size_bytes;
  Status status = GetEcGroupOrderSize(key, &order_size_bytes);
  if (status.IsError())
    return status;

  // If the size of the signature is incorrect, verification must fail. Success
  // is returned here rather than an error, so that the caller can fail
  // verification with a boolean, rather than reject the promise with an
  // exception.
  if (signature.byte_length() != 2 * order_size_bytes) {
    *incorrect_length = true;
    return Status::Success();
  }

  *incorrect_length = false;

  // Construct an ECDSA_SIG from |signature|.
  crypto::ScopedECDSA_SIG ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig)
    return Status::OperationError();

  if (!BN_bin2bn(signature.bytes(), order_size_bytes, ecdsa_sig->r) ||
      !BN_bin2bn(signature.bytes() + order_size_bytes, order_size_bytes,
                 ecdsa_sig->s)) {
    return Status::ErrorUnexpected();
  }

  // Encode the signature.
  uint8_t* der;
  size_t der_len;
  if (!ECDSA_SIG_to_bytes(&der, &der_len, ecdsa_sig.get()))
    return Status::OperationError();
  der_signature->assign(der, der + der_len);
  OPENSSL_free(der);

  return Status::Success();
}

class EcdsaImplementation : public EcAlgorithm {
 public:
  EcdsaImplementation()
      : EcAlgorithm(blink::WebCryptoKeyUsageVerify,
                    blink::WebCryptoKeyUsageSign) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoNamedCurve curve) const override {
    switch (curve) {
      case blink::WebCryptoNamedCurveP256:
        return "ES256";
      case blink::WebCryptoNamedCurveP384:
        return "ES384";
      case blink::WebCryptoNamedCurveP521:
        // This is not a typo! ES512 means P-521 with SHA-512.
        return "ES512";
      default:
        return NULL;
    }
  }

  Status Sign(const blink::WebCryptoAlgorithm& algorithm,
              const blink::WebCryptoKey& key,
              const CryptoData& data,
              std::vector<uint8_t>* buffer) const override {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());

    EVP_PKEY* private_key = NULL;
    const EVP_MD* digest = NULL;
    Status status = GetPKeyAndDigest(algorithm, key, &private_key, &digest);
    if (status.IsError())
      return status;

    // NOTE: A call to EVP_DigestSignFinal() with a NULL second parameter
    // returns a maximum allocation size, while the call without a NULL returns
    // the real one, which may be smaller.
    size_t sig_len = 0;
    if (!ctx.get() ||
        !EVP_DigestSignInit(ctx.get(), NULL, digest, NULL, private_key) ||
        !EVP_DigestSignUpdate(ctx.get(), data.bytes(), data.byte_length()) ||
        !EVP_DigestSignFinal(ctx.get(), NULL, &sig_len)) {
      return Status::OperationError();
    }

    buffer->resize(sig_len);
    if (!EVP_DigestSignFinal(ctx.get(), buffer->data(), &sig_len))
      return Status::OperationError();
    buffer->resize(sig_len);

    // ECDSA signing in BoringSSL outputs a DER-encoded (r,s). WebCrypto however
    // expects a padded bitstring that is r concatenated to s. Convert to the
    // expected format.
    return ConvertDerSignatureToWebCryptoSignature(private_key, buffer);
  }

  Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& signature,
                const CryptoData& data,
                bool* signature_match) const override {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());

    EVP_PKEY* public_key = NULL;
    const EVP_MD* digest = NULL;
    Status status = GetPKeyAndDigest(algorithm, key, &public_key, &digest);
    if (status.IsError())
      return status;

    std::vector<uint8_t> der_signature;
    bool incorrect_length_signature = false;
    status = ConvertWebCryptoSignatureToDerSignature(
        public_key, signature, &der_signature, &incorrect_length_signature);
    if (status.IsError())
      return status;

    if (incorrect_length_signature) {
      *signature_match = false;
      return Status::Success();
    }

    if (!EVP_DigestVerifyInit(ctx.get(), NULL, digest, NULL, public_key) ||
        !EVP_DigestVerifyUpdate(ctx.get(), data.bytes(), data.byte_length())) {
      return Status::OperationError();
    }

    *signature_match =
        1 == EVP_DigestVerifyFinal(ctx.get(), der_signature.data(),
                                   der_signature.size());
    return Status::Success();
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateEcdsaImplementation() {
  return base::WrapUnique(new EcdsaImplementation);
}

}  // namespace webcrypto
