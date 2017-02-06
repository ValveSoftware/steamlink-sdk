// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithms/rsa_sign.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Extracts the OpenSSL key and digest from a WebCrypto key. The returned
// pointers will remain valid as long as |key| is alive.
Status GetPKeyAndDigest(const blink::WebCryptoKey& key,
                        EVP_PKEY** pkey,
                        const EVP_MD** digest) {
  *pkey = GetEVP_PKEY(key);

  *digest = GetDigest(key.algorithm().rsaHashedParams()->hash());
  if (!*digest)
    return Status::ErrorUnsupported();

  return Status::Success();
}

// Sets the PSS parameters on |pctx| if the key is for RSA-PSS.
//
// Otherwise returns Success without doing anything.
Status ApplyRsaPssOptions(const blink::WebCryptoKey& key,
                          const EVP_MD* const mgf_digest,
                          unsigned int salt_length_bytes,
                          EVP_PKEY_CTX* pctx) {
  // Only apply RSA-PSS options if the key is for RSA-PSS.
  if (key.algorithm().id() != blink::WebCryptoAlgorithmIdRsaPss) {
    DCHECK_EQ(0u, salt_length_bytes);
    DCHECK_EQ(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, key.algorithm().id());
    return Status::Success();
  }

  // BoringSSL takes a signed int for the salt length, and interprets
  // negative values in a special manner. Make sure not to silently underflow.
  base::CheckedNumeric<int> salt_length_bytes_int(salt_length_bytes);
  if (!salt_length_bytes_int.IsValid()) {
    // TODO(eroman): Give a better error message.
    return Status::OperationError();
  }

  if (1 != EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) ||
      1 != EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, mgf_digest) ||
      1 != EVP_PKEY_CTX_set_rsa_pss_saltlen(
               pctx, salt_length_bytes_int.ValueOrDie())) {
    return Status::OperationError();
  }

  return Status::Success();
}

}  // namespace

Status RsaSign(const blink::WebCryptoKey& key,
               unsigned int pss_salt_length_bytes,
               const CryptoData& data,
               std::vector<uint8_t>* buffer) {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());
  EVP_PKEY_CTX* pctx = NULL;  // Owned by |ctx|.

  EVP_PKEY* private_key = NULL;
  const EVP_MD* digest = NULL;
  Status status = GetPKeyAndDigest(key, &private_key, &digest);
  if (status.IsError())
    return status;

  // NOTE: A call to EVP_DigestSignFinal() with a NULL second parameter
  // returns a maximum allocation size, while the call without a NULL returns
  // the real one, which may be smaller.
  size_t sig_len = 0;
  if (!ctx.get() ||
      !EVP_DigestSignInit(ctx.get(), &pctx, digest, NULL, private_key)) {
    return Status::OperationError();
  }

  // Set PSS-specific options (if applicable).
  status = ApplyRsaPssOptions(key, digest, pss_salt_length_bytes, pctx);
  if (status.IsError())
    return status;

  if (!EVP_DigestSignUpdate(ctx.get(), data.bytes(), data.byte_length()) ||
      !EVP_DigestSignFinal(ctx.get(), NULL, &sig_len)) {
    return Status::OperationError();
  }

  buffer->resize(sig_len);
  if (!EVP_DigestSignFinal(ctx.get(), buffer->data(), &sig_len))
    return Status::OperationError();

  buffer->resize(sig_len);
  return Status::Success();
}

Status RsaVerify(const blink::WebCryptoKey& key,
                 unsigned int pss_salt_length_bytes,
                 const CryptoData& signature,
                 const CryptoData& data,
                 bool* signature_match) {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());
  EVP_PKEY_CTX* pctx = NULL;  // Owned by |ctx|.

  EVP_PKEY* public_key = NULL;
  const EVP_MD* digest = NULL;
  Status status = GetPKeyAndDigest(key, &public_key, &digest);
  if (status.IsError())
    return status;

  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, digest, NULL, public_key))
    return Status::OperationError();

  // Set PSS-specific options (if applicable).
  status = ApplyRsaPssOptions(key, digest, pss_salt_length_bytes, pctx);
  if (status.IsError())
    return status;

  if (!EVP_DigestVerifyUpdate(ctx.get(), data.bytes(), data.byte_length()))
    return Status::OperationError();

  *signature_match = 1 == EVP_DigestVerifyFinal(ctx.get(), signature.bytes(),
                                                signature.byte_length());
  return Status::Success();
}

}  // namespace webcrypto
