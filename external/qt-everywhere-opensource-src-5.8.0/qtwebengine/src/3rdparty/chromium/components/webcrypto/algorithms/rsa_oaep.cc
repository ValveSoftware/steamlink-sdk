// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/memory/ptr_util.h"
#include "components/webcrypto/algorithms/rsa.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

typedef int (*InitFunc)(EVP_PKEY_CTX* ctx);
typedef int (*EncryptDecryptFunc)(EVP_PKEY_CTX* ctx,
                                  unsigned char* out,
                                  size_t* outlen,
                                  const unsigned char* in,
                                  size_t inlen);

// Helper for doing either RSA-OAEP encryption or decryption.
//
// To encrypt call with:
//   init_func=EVP_PKEY_encrypt_init, encrypt_decrypt_func=EVP_PKEY_encrypt
//
// To decrypt call with:
//   init_func=EVP_PKEY_decrypt_init, encrypt_decrypt_func=EVP_PKEY_decrypt
Status CommonEncryptDecrypt(InitFunc init_func,
                            EncryptDecryptFunc encrypt_decrypt_func,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);
  const EVP_MD* digest = GetDigest(key.algorithm().rsaHashedParams()->hash());
  if (!digest)
    return Status::ErrorUnsupported();

  crypto::ScopedEVP_PKEY_CTX ctx(EVP_PKEY_CTX_new(pkey, NULL));

  if (!init_func(ctx.get()) ||
      !EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) ||
      !EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), digest) ||
      !EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), digest)) {
    return Status::OperationError();
  }

  const blink::WebVector<uint8_t>& label =
      algorithm.rsaOaepParams()->optionalLabel();

  if (label.size()) {
    // Make a copy of the label, since the ctx takes ownership of it when
    // calling set0_rsa_oaep_label().
    crypto::ScopedOpenSSLBytes label_copy;
    label_copy.reset(static_cast<uint8_t*>(OPENSSL_malloc(label.size())));
    memcpy(label_copy.get(), label.data(), label.size());

    if (1 != EVP_PKEY_CTX_set0_rsa_oaep_label(ctx.get(), label_copy.release(),
                                              label.size())) {
      return Status::OperationError();
    }
  }

  // Determine the maximum length of the output.
  size_t outlen = 0;
  if (!encrypt_decrypt_func(ctx.get(), NULL, &outlen, data.bytes(),
                            data.byte_length())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  // Do the actual encryption/decryption.
  if (!encrypt_decrypt_func(ctx.get(), buffer->data(), &outlen, data.bytes(),
                            data.byte_length())) {
    return Status::OperationError();
  }
  buffer->resize(outlen);

  return Status::Success();
}

class RsaOaepImplementation : public RsaHashedAlgorithm {
 public:
  RsaOaepImplementation()
      : RsaHashedAlgorithm(
            blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageWrapKey,
            blink::WebCryptoKeyUsageDecrypt |
                blink::WebCryptoKeyUsageUnwrapKey) {}

  const char* GetJwkAlgorithm(
      const blink::WebCryptoAlgorithmId hash) const override {
    switch (hash) {
      case blink::WebCryptoAlgorithmIdSha1:
        return "RSA-OAEP";
      case blink::WebCryptoAlgorithmIdSha256:
        return "RSA-OAEP-256";
      case blink::WebCryptoAlgorithmIdSha384:
        return "RSA-OAEP-384";
      case blink::WebCryptoAlgorithmIdSha512:
        return "RSA-OAEP-512";
      default:
        return NULL;
    }
  }

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    if (key.type() != blink::WebCryptoKeyTypePublic)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(EVP_PKEY_encrypt_init, EVP_PKEY_encrypt,
                                algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 const CryptoData& data,
                 std::vector<uint8_t>* buffer) const override {
    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return Status::ErrorUnexpectedKeyType();

    return CommonEncryptDecrypt(EVP_PKEY_decrypt_init, EVP_PKEY_decrypt,
                                algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateRsaOaepImplementation() {
  return base::WrapUnique(new RsaOaepImplementation);
}

}  // namespace webcrypto
