// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCryptoSubTags_h
#define WebCryptoSubTags_h

#include <cstdint>

namespace blink {

enum CryptoKeyAlgorithmTag : uint32_t {
  AesCbcTag = 1,
  HmacTag = 2,
  RsaSsaPkcs1v1_5Tag = 3,
  // ID 4 was used by RsaEs, while still behind experimental flag.
  Sha1Tag = 5,
  Sha256Tag = 6,
  Sha384Tag = 7,
  Sha512Tag = 8,
  AesGcmTag = 9,
  RsaOaepTag = 10,
  AesCtrTag = 11,
  AesKwTag = 12,
  RsaPssTag = 13,
  EcdsaTag = 14,
  EcdhTag = 15,
  HkdfTag = 16,
  Pbkdf2Tag = 17,
  // Maximum allowed value is 2^32-1
};

enum NamedCurveTag : uint32_t {
  P256Tag = 1,
  P384Tag = 2,
  P521Tag = 3,
  // Maximum allowed value is 2^32-1
};

enum CryptoKeyUsage : uint32_t {
  // Extractability is not a "usage" in the WebCryptoKeyUsages sense, however
  // it fits conveniently into this bitfield.
  ExtractableUsage = 1 << 0,

  EncryptUsage = 1 << 1,
  DecryptUsage = 1 << 2,
  SignUsage = 1 << 3,
  VerifyUsage = 1 << 4,
  DeriveKeyUsage = 1 << 5,
  WrapKeyUsage = 1 << 6,
  UnwrapKeyUsage = 1 << 7,
  DeriveBitsUsage = 1 << 8,
  // Maximum allowed value is 1 << 31
};

enum CryptoKeySubTag : uint8_t {
  AesKeyTag = 1,
  HmacKeyTag = 2,
  // ID 3 was used by RsaKeyTag, while still behind experimental flag.
  RsaHashedKeyTag = 4,
  EcKeyTag = 5,
  NoParamsKeyTag = 6,
  // Maximum allowed value is 255
};

enum AsymmetricCryptoKeyType : uint32_t {
  PublicKeyType = 1,
  PrivateKeyType = 2,
  // Maximum allowed value is 2^32-1
};

}  // namespace blink

#endif  // WebCryptoSubTags_h
