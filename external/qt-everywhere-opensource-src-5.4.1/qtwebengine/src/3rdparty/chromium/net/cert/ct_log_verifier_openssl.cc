// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "crypto/sha2.h"
#include "net/cert/signed_tree_head.h"

namespace net {

namespace {

const EVP_MD* GetEvpAlg(ct::DigitallySigned::HashAlgorithm alg) {
  switch (alg) {
    case ct::DigitallySigned::HASH_ALGO_MD5:
      return EVP_md5();
    case ct::DigitallySigned::HASH_ALGO_SHA1:
      return EVP_sha1();
    case ct::DigitallySigned::HASH_ALGO_SHA224:
      return EVP_sha224();
    case ct::DigitallySigned::HASH_ALGO_SHA256:
      return EVP_sha256();
    case ct::DigitallySigned::HASH_ALGO_SHA384:
      return EVP_sha384();
    case ct::DigitallySigned::HASH_ALGO_SHA512:
      return EVP_sha512();
    case ct::DigitallySigned::HASH_ALGO_NONE:
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace

CTLogVerifier::~CTLogVerifier() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (public_key_)
    EVP_PKEY_free(public_key_);
}

CTLogVerifier::CTLogVerifier()
    : hash_algorithm_(ct::DigitallySigned::HASH_ALGO_NONE),
      signature_algorithm_(ct::DigitallySigned::SIG_ALGO_ANONYMOUS),
      public_key_(NULL) {}

bool CTLogVerifier::Init(const base::StringPiece& public_key,
                         const base::StringPiece& description) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedOpenSSL<BIO, BIO_free_all> bio(
      BIO_new_mem_buf(const_cast<char*>(public_key.data()), public_key.size()));
  if (!bio.get())
    return false;

  public_key_ = d2i_PUBKEY_bio(bio.get(), NULL);
  if (!public_key_)
    return false;

  key_id_ = crypto::SHA256HashString(public_key);
  description_ = description.as_string();

  // Right now, only RSASSA-PKCS1v15 with SHA-256 and ECDSA with SHA-256 are
  // supported.
  switch (EVP_PKEY_type(public_key_->type)) {
    case EVP_PKEY_RSA:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_RSA;
      break;
    case EVP_PKEY_EC:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_ECDSA;
      break;
    default:
      DVLOG(1) << "Unsupported key type: " << EVP_PKEY_type(public_key_->type);
      return false;
  }

  // Extra sanity check: Require RSA keys of at least 2048 bits.
  // EVP_PKEY_size returns the size in bytes. 256 = 2048-bit RSA key.
  if (signature_algorithm_ == ct::DigitallySigned::SIG_ALGO_RSA &&
      EVP_PKEY_size(public_key_) < 256) {
    DVLOG(1) << "Too small a public key.";
    return false;
  }

  return true;
}

bool CTLogVerifier::VerifySignature(const base::StringPiece& data_to_sign,
                                    const base::StringPiece& signature) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* hash_alg = GetEvpAlg(hash_algorithm_);
  if (hash_alg == NULL)
    return false;

  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  bool ok = (
      1 == EVP_DigestVerifyInit(&ctx, NULL, hash_alg, NULL, public_key_) &&
      1 == EVP_DigestVerifyUpdate(
          &ctx, data_to_sign.data(), data_to_sign.size()) &&
      1 == EVP_DigestVerifyFinal(
          &ctx,
          reinterpret_cast<unsigned char*>(const_cast<char*>(signature.data())),
          signature.size()));

  EVP_MD_CTX_cleanup(&ctx);
  return ok;
}

}  // namespace net
