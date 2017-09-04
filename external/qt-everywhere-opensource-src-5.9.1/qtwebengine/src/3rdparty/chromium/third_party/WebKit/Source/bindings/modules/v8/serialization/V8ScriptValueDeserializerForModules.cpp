// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"

#include "bindings/modules/v8/serialization/WebCryptoSubTags.h"
#include "modules/crypto/CryptoKey.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/peerconnection/RTCCertificate.h"
#include "platform/FileSystemType.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"
#include "public/platform/WebRTCCertificateGenerator.h"

namespace blink {

ScriptWrappable* V8ScriptValueDeserializerForModules::readDOMObject(
    SerializationTag tag) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (ScriptWrappable* wrappable =
          V8ScriptValueDeserializer::readDOMObject(tag))
    return wrappable;

  switch (tag) {
    case CryptoKeyTag:
      return readCryptoKey();
    case DOMFileSystemTag: {
      uint32_t rawType;
      String name;
      String rootURL;
      if (!readUint32(&rawType) || rawType > FileSystemTypeLast ||
          !readUTF8String(&name) || !readUTF8String(&rootURL))
        return nullptr;
      return DOMFileSystem::create(getScriptState()->getExecutionContext(),
                                   name, static_cast<FileSystemType>(rawType),
                                   KURL(ParsedURLString, rootURL));
    }
    case RTCCertificateTag: {
      String pemPrivateKey;
      String pemCertificate;
      if (!readUTF8String(&pemPrivateKey) || !readUTF8String(&pemCertificate))
        return nullptr;
      std::unique_ptr<WebRTCCertificateGenerator> certificateGenerator(
          Platform::current()->createRTCCertificateGenerator());
      std::unique_ptr<WebRTCCertificate> certificate =
          certificateGenerator->fromPEM(pemPrivateKey, pemCertificate);
      if (!certificate)
        return nullptr;
      return new RTCCertificate(std::move(certificate));
    }
    default:
      break;
  }
  return nullptr;
}

namespace {

bool algorithmIdFromWireFormat(uint32_t rawId, WebCryptoAlgorithmId* id) {
  switch (static_cast<CryptoKeyAlgorithmTag>(rawId)) {
    case AesCbcTag:
      *id = WebCryptoAlgorithmIdAesCbc;
      return true;
    case HmacTag:
      *id = WebCryptoAlgorithmIdHmac;
      return true;
    case RsaSsaPkcs1v1_5Tag:
      *id = WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
      return true;
    case Sha1Tag:
      *id = WebCryptoAlgorithmIdSha1;
      return true;
    case Sha256Tag:
      *id = WebCryptoAlgorithmIdSha256;
      return true;
    case Sha384Tag:
      *id = WebCryptoAlgorithmIdSha384;
      return true;
    case Sha512Tag:
      *id = WebCryptoAlgorithmIdSha512;
      return true;
    case AesGcmTag:
      *id = WebCryptoAlgorithmIdAesGcm;
      return true;
    case RsaOaepTag:
      *id = WebCryptoAlgorithmIdRsaOaep;
      return true;
    case AesCtrTag:
      *id = WebCryptoAlgorithmIdAesCtr;
      return true;
    case AesKwTag:
      *id = WebCryptoAlgorithmIdAesKw;
      return true;
    case RsaPssTag:
      *id = WebCryptoAlgorithmIdRsaPss;
      return true;
    case EcdsaTag:
      *id = WebCryptoAlgorithmIdEcdsa;
      return true;
    case EcdhTag:
      *id = WebCryptoAlgorithmIdEcdh;
      return true;
    case HkdfTag:
      *id = WebCryptoAlgorithmIdHkdf;
      return true;
    case Pbkdf2Tag:
      *id = WebCryptoAlgorithmIdPbkdf2;
      return true;
  }
  return false;
}

bool asymmetricKeyTypeFromWireFormat(uint32_t rawKeyType,
                                     WebCryptoKeyType* keyType) {
  switch (static_cast<AsymmetricCryptoKeyType>(rawKeyType)) {
    case PublicKeyType:
      *keyType = WebCryptoKeyTypePublic;
      return true;
    case PrivateKeyType:
      *keyType = WebCryptoKeyTypePrivate;
      return true;
  }
  return false;
}

bool namedCurveFromWireFormat(uint32_t rawNamedCurve,
                              WebCryptoNamedCurve* namedCurve) {
  switch (static_cast<NamedCurveTag>(rawNamedCurve)) {
    case P256Tag:
      *namedCurve = WebCryptoNamedCurveP256;
      return true;
    case P384Tag:
      *namedCurve = WebCryptoNamedCurveP384;
      return true;
    case P521Tag:
      *namedCurve = WebCryptoNamedCurveP521;
      return true;
  }
  return false;
}

bool keyUsagesFromWireFormat(uint32_t rawUsages,
                             WebCryptoKeyUsageMask* usages,
                             bool* extractable) {
  // Reminder to update this when adding new key usages.
  static_assert(EndOfWebCryptoKeyUsage == (1 << 7) + 1,
                "update required when adding new key usages");
  const uint32_t allPossibleUsages =
      ExtractableUsage | EncryptUsage | DecryptUsage | SignUsage | VerifyUsage |
      DeriveKeyUsage | WrapKeyUsage | UnwrapKeyUsage | DeriveBitsUsage;
  if (rawUsages & ~allPossibleUsages)
    return false;

  *usages = 0;
  *extractable = rawUsages & ExtractableUsage;
  if (rawUsages & EncryptUsage)
    *usages |= WebCryptoKeyUsageEncrypt;
  if (rawUsages & DecryptUsage)
    *usages |= WebCryptoKeyUsageDecrypt;
  if (rawUsages & SignUsage)
    *usages |= WebCryptoKeyUsageSign;
  if (rawUsages & VerifyUsage)
    *usages |= WebCryptoKeyUsageVerify;
  if (rawUsages & DeriveKeyUsage)
    *usages |= WebCryptoKeyUsageDeriveKey;
  if (rawUsages & WrapKeyUsage)
    *usages |= WebCryptoKeyUsageWrapKey;
  if (rawUsages & UnwrapKeyUsage)
    *usages |= WebCryptoKeyUsageUnwrapKey;
  if (rawUsages & DeriveBitsUsage)
    *usages |= WebCryptoKeyUsageDeriveBits;
  return true;
}

}  // namespace

CryptoKey* V8ScriptValueDeserializerForModules::readCryptoKey() {
  // Read params.
  uint8_t rawKeyType;
  if (!readOneByte(&rawKeyType))
    return nullptr;
  WebCryptoKeyAlgorithm algorithm;
  WebCryptoKeyType keyType = WebCryptoKeyTypeSecret;
  switch (rawKeyType) {
    case AesKeyTag: {
      uint32_t rawId;
      WebCryptoAlgorithmId id;
      uint32_t lengthBytes;
      if (!readUint32(&rawId) || !algorithmIdFromWireFormat(rawId, &id) ||
          !readUint32(&lengthBytes) ||
          lengthBytes > std::numeric_limits<unsigned short>::max() / 8u)
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::createAes(id, lengthBytes * 8);
      keyType = WebCryptoKeyTypeSecret;
      break;
    }
    case HmacKeyTag: {
      uint32_t lengthBytes;
      uint32_t rawHash;
      WebCryptoAlgorithmId hash;
      if (!readUint32(&lengthBytes) ||
          lengthBytes > std::numeric_limits<unsigned>::max() / 8 ||
          !readUint32(&rawHash) || !algorithmIdFromWireFormat(rawHash, &hash))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::createHmac(hash, lengthBytes * 8);
      keyType = WebCryptoKeyTypeSecret;
      break;
    }
    case RsaHashedKeyTag: {
      uint32_t rawId;
      WebCryptoAlgorithmId id;
      uint32_t rawKeyType;
      uint32_t modulusLengthBits;
      uint32_t publicExponentSize;
      const void* publicExponentBytes;
      uint32_t rawHash;
      WebCryptoAlgorithmId hash;
      if (!readUint32(&rawId) || !algorithmIdFromWireFormat(rawId, &id) ||
          !readUint32(&rawKeyType) ||
          !asymmetricKeyTypeFromWireFormat(rawKeyType, &keyType) ||
          !readUint32(&modulusLengthBits) || !readUint32(&publicExponentSize) ||
          !readRawBytes(publicExponentSize, &publicExponentBytes) ||
          !readUint32(&rawHash) || !algorithmIdFromWireFormat(rawHash, &hash))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::createRsaHashed(
          id, modulusLengthBits,
          reinterpret_cast<const unsigned char*>(publicExponentBytes),
          publicExponentSize, hash);
      break;
    }
    case EcKeyTag: {
      uint32_t rawId;
      WebCryptoAlgorithmId id;
      uint32_t rawKeyType;
      uint32_t rawNamedCurve;
      WebCryptoNamedCurve namedCurve;
      if (!readUint32(&rawId) || !algorithmIdFromWireFormat(rawId, &id) ||
          !readUint32(&rawKeyType) ||
          !asymmetricKeyTypeFromWireFormat(rawKeyType, &keyType) ||
          !readUint32(&rawNamedCurve) ||
          !namedCurveFromWireFormat(rawNamedCurve, &namedCurve))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::createEc(id, namedCurve);
      break;
    }
    case NoParamsKeyTag: {
      uint32_t rawId;
      WebCryptoAlgorithmId id;
      if (!readUint32(&rawId) || !algorithmIdFromWireFormat(rawId, &id))
        return nullptr;
      algorithm = WebCryptoKeyAlgorithm::createWithoutParams(id);
      break;
    }
  }
  if (algorithm.isNull())
    return nullptr;

  // Read key usages.
  uint32_t rawUsages;
  WebCryptoKeyUsageMask usages;
  bool extractable;
  if (!readUint32(&rawUsages) ||
      !keyUsagesFromWireFormat(rawUsages, &usages, &extractable))
    return nullptr;

  // Read key data.
  uint32_t keyDataLength;
  const void* keyData;
  if (!readUint32(&keyDataLength) || !readRawBytes(keyDataLength, &keyData))
    return nullptr;

  WebCryptoKey key = WebCryptoKey::createNull();
  if (!Platform::current()->crypto()->deserializeKeyForClone(
          algorithm, keyType, extractable, usages,
          reinterpret_cast<const unsigned char*>(keyData), keyDataLength, key))
    return nullptr;

  return CryptoKey::create(key);
}

}  // namespace blink
