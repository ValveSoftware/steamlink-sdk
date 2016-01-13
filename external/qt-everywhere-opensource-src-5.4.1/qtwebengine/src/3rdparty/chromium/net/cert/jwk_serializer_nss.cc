// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/jwk_serializer.h"

#include <cert.h>
#include <keyhi.h>
#include <nss.h>

#include "base/base64.h"
#include "base/values.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"

namespace net {

namespace JwkSerializer {

namespace {

bool ConvertEcPrime256v1PublicKeyInfoToJwk(
    CERTSubjectPublicKeyInfo* spki,
    base::DictionaryValue* public_key_jwk) {
  static const int kUncompressedEncodingType = 4;
  static const int kPrime256v1PublicKeyLength = 64;
  // The public key value is encoded as 0x04 + 64 bytes of public key.
  // NSS gives the length as the bit length.
  if (spki->subjectPublicKey.len != (kPrime256v1PublicKeyLength + 1) * 8 ||
      spki->subjectPublicKey.data[0] != kUncompressedEncodingType)
    return false;

  public_key_jwk->SetString("kty", "EC");
  public_key_jwk->SetString("crv", "P-256");

  base::StringPiece x(
      reinterpret_cast<char*>(spki->subjectPublicKey.data + 1),
      kPrime256v1PublicKeyLength / 2);
  std::string x_b64;
  base::Base64Encode(x, &x_b64);
  public_key_jwk->SetString("x", x_b64);

  base::StringPiece y(
      reinterpret_cast<char*>(spki->subjectPublicKey.data + 1 +
                              kPrime256v1PublicKeyLength / 2),
      kPrime256v1PublicKeyLength / 2);
  std::string y_b64;
  base::Base64Encode(y, &y_b64);
  public_key_jwk->SetString("y", y_b64);
  return true;
}

bool ConvertEcPublicKeyInfoToJwk(
    CERTSubjectPublicKeyInfo* spki,
    base::DictionaryValue* public_key_jwk) {
  // 1.2.840.10045.3.1.7
  // (iso.member-body.us.ansi-x9-62.ellipticCurve.primeCurve.prime256v1)
  // (This includes the DER-encoded type (OID) and length: parameters can be
  // anything, so the DER type isn't implied, and NSS includes it.)
  static const unsigned char kPrime256v1[] = {
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
  };
  if (spki->algorithm.parameters.len == sizeof(kPrime256v1) &&
      !memcmp(spki->algorithm.parameters.data, kPrime256v1,
              sizeof(kPrime256v1))) {
    return ConvertEcPrime256v1PublicKeyInfoToJwk(spki, public_key_jwk);
  }
  // TODO(juanlang): other curves
  return false;
}

typedef scoped_ptr<
    CERTSubjectPublicKeyInfo,
    crypto::NSSDestroyer<CERTSubjectPublicKeyInfo,
                         SECKEY_DestroySubjectPublicKeyInfo> >
    ScopedCERTSubjectPublicKeyInfo;

}  // namespace

bool ConvertSpkiFromDerToJwk(
    const base::StringPiece& spki_der,
    base::DictionaryValue* public_key_jwk) {
  public_key_jwk->Clear();

  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return false;

  SECItem sec_item;
  sec_item.data = const_cast<unsigned char*>(
      reinterpret_cast<const unsigned char*>(spki_der.data()));
  sec_item.len = spki_der.size();
  ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&sec_item));
  if (!spki)
    return false;

  // 1.2.840.10045.2
  // (iso.member-body.us.ansi-x9-62.id-ecPublicKey)
  // (This omits the ASN.1 encoding of the type (OID) and length: the fact that
  // this is an OID is already clear, and NSS omits it here.)
  static const unsigned char kIdEcPublicKey[] = {
    0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01
  };
  bool rv = false;
  if (spki->algorithm.algorithm.len == sizeof(kIdEcPublicKey) &&
      !memcmp(spki->algorithm.algorithm.data, kIdEcPublicKey,
              sizeof(kIdEcPublicKey))) {
    rv = ConvertEcPublicKeyInfoToJwk(spki.get(), public_key_jwk);
  }
  // TODO(juanlang): other algorithms
  return rv;
}

}  // namespace JwkSerializer

}  // namespace net
