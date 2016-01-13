// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <nss.h>
#include <pk11pub.h>
#include <secitem.h>
#include <secoid.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "net/cert/signed_tree_head.h"

namespace net {

namespace {

SECOidTag GetNSSSigAlg(ct::DigitallySigned::SignatureAlgorithm alg) {
  switch (alg) {
    case ct::DigitallySigned::SIG_ALGO_RSA:
      return SEC_OID_PKCS1_RSA_ENCRYPTION;
    case ct::DigitallySigned::SIG_ALGO_DSA:
      return SEC_OID_ANSIX9_DSA_SIGNATURE;
    case ct::DigitallySigned::SIG_ALGO_ECDSA:
      return SEC_OID_ANSIX962_EC_PUBLIC_KEY;
    case ct::DigitallySigned::SIG_ALGO_ANONYMOUS:
    default:
      NOTREACHED();
      return SEC_OID_UNKNOWN;
  }
}

SECOidTag GetNSSHashAlg(ct::DigitallySigned::HashAlgorithm alg) {
  switch (alg) {
    case ct::DigitallySigned::HASH_ALGO_MD5:
      return SEC_OID_MD5;
    case ct::DigitallySigned::HASH_ALGO_SHA1:
      return SEC_OID_SHA1;
    case ct::DigitallySigned::HASH_ALGO_SHA224:
      return SEC_OID_SHA224;
    case ct::DigitallySigned::HASH_ALGO_SHA256:
      return SEC_OID_SHA256;
    case ct::DigitallySigned::HASH_ALGO_SHA384:
      return SEC_OID_SHA384;
    case ct::DigitallySigned::HASH_ALGO_SHA512:
      return SEC_OID_SHA512;
    case ct::DigitallySigned::HASH_ALGO_NONE:
    default:
      NOTREACHED();
      return SEC_OID_UNKNOWN;
  }
}

}  // namespace

CTLogVerifier::~CTLogVerifier() {
  if (public_key_)
    SECKEY_DestroyPublicKey(public_key_);
}

CTLogVerifier::CTLogVerifier()
    : hash_algorithm_(ct::DigitallySigned::HASH_ALGO_NONE),
      signature_algorithm_(ct::DigitallySigned::SIG_ALGO_ANONYMOUS),
      public_key_(NULL) {}

bool CTLogVerifier::Init(const base::StringPiece& public_key,
                         const base::StringPiece& description) {
  SECItem key_data;

  crypto::EnsureNSSInit();

  key_data.data = reinterpret_cast<unsigned char*>(
      const_cast<char*>(public_key.data()));
  key_data.len = public_key.size();

  CERTSubjectPublicKeyInfo* public_key_info =
      SECKEY_DecodeDERSubjectPublicKeyInfo(&key_data);
  if (!public_key_info) {
    DVLOG(1) << "Failed decoding public key.";
    return false;
  }

  public_key_ = SECKEY_ExtractPublicKey(public_key_info);
  SECKEY_DestroySubjectPublicKeyInfo(public_key_info);

  if (!public_key_) {
    DVLOG(1) << "Failed extracting public key.";
    return false;
  }

  key_id_ = crypto::SHA256HashString(public_key);
  description_ = description.as_string();

  // Right now, only RSASSA-PKCS1v15 with SHA-256 and ECDSA with SHA-256 are
  // supported.
  switch (SECKEY_GetPublicKeyType(public_key_)) {
    case rsaKey:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_RSA;
      break;
    case ecKey:
      hash_algorithm_ = ct::DigitallySigned::HASH_ALGO_SHA256;
      signature_algorithm_ = ct::DigitallySigned::SIG_ALGO_ECDSA;
      break;
    default:
      DVLOG(1) << "Unsupported key type: "
               << SECKEY_GetPublicKeyType(public_key_);
      return false;
  }

  // Extra sanity check: Require RSA keys of at least 2048 bits.
  if (signature_algorithm_ == ct::DigitallySigned::SIG_ALGO_RSA &&
      SECKEY_PublicKeyStrengthInBits(public_key_) < 2048) {
    DVLOG(1) << "Too small a public key.";
    return false;
  }

  return true;
}

bool CTLogVerifier::VerifySignature(const base::StringPiece& data_to_sign,
                                    const base::StringPiece& signature) {
  SECItem sig_data;
  sig_data.data = reinterpret_cast<unsigned char*>(const_cast<char*>(
      signature.data()));
  sig_data.len = signature.size();

  SECStatus rv = VFY_VerifyDataDirect(
      reinterpret_cast<const unsigned char*>(data_to_sign.data()),
      data_to_sign.size(), public_key_, &sig_data,
      GetNSSSigAlg(signature_algorithm_), GetNSSHashAlg(hash_algorithm_),
      NULL, NULL);
  DVLOG(1) << "Signature verification result: " << (rv == SECSuccess);
  return rv == SECSuccess;
}

}  // namespace net
