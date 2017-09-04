// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/V8CryptoKey.h"
#include "bindings/modules/v8/V8DOMFileSystem.h"
#include "bindings/modules/v8/V8RTCCertificate.h"
#include "bindings/modules/v8/serialization/WebCryptoSubTags.h"
#include "platform/FileSystemType.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoKey.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"

namespace blink {

bool V8ScriptValueSerializerForModules::writeDOMObject(
    ScriptWrappable* wrappable,
    ExceptionState& exceptionState) {
  // Give the core/ implementation a chance to try first.
  // If it didn't recognize the kind of wrapper, try the modules types.
  if (V8ScriptValueSerializer::writeDOMObject(wrappable, exceptionState))
    return true;
  if (exceptionState.hadException())
    return false;

  const WrapperTypeInfo* wrapperTypeInfo = wrappable->wrapperTypeInfo();
  if (wrapperTypeInfo == &V8CryptoKey::wrapperTypeInfo) {
    return writeCryptoKey(wrappable->toImpl<CryptoKey>()->key(),
                          exceptionState);
  }
  if (wrapperTypeInfo == &V8DOMFileSystem::wrapperTypeInfo) {
    DOMFileSystem* fs = wrappable->toImpl<DOMFileSystem>();
    if (!fs->clonable()) {
      exceptionState.throwDOMException(
          DataCloneError, "A FileSystem object could not be cloned.");
      return false;
    }
    writeTag(DOMFileSystemTag);
    // This locks in the values of the FileSystemType enumerators.
    writeUint32(static_cast<uint32_t>(fs->type()));
    writeUTF8String(fs->name());
    writeUTF8String(fs->rootURL().getString());
    return true;
  }
  if (wrapperTypeInfo == &V8RTCCertificate::wrapperTypeInfo) {
    RTCCertificate* certificate = wrappable->toImpl<RTCCertificate>();
    WebRTCCertificatePEM pem = certificate->certificate().toPEM();
    writeTag(RTCCertificateTag);
    writeUTF8String(pem.privateKey());
    writeUTF8String(pem.certificate());
    return true;
  }
  return false;
}

namespace {

uint32_t algorithmIdForWireFormat(WebCryptoAlgorithmId id) {
  switch (id) {
    case WebCryptoAlgorithmIdAesCbc:
      return AesCbcTag;
    case WebCryptoAlgorithmIdHmac:
      return HmacTag;
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return RsaSsaPkcs1v1_5Tag;
    case WebCryptoAlgorithmIdSha1:
      return Sha1Tag;
    case WebCryptoAlgorithmIdSha256:
      return Sha256Tag;
    case WebCryptoAlgorithmIdSha384:
      return Sha384Tag;
    case WebCryptoAlgorithmIdSha512:
      return Sha512Tag;
    case WebCryptoAlgorithmIdAesGcm:
      return AesGcmTag;
    case WebCryptoAlgorithmIdRsaOaep:
      return RsaOaepTag;
    case WebCryptoAlgorithmIdAesCtr:
      return AesCtrTag;
    case WebCryptoAlgorithmIdAesKw:
      return AesKwTag;
    case WebCryptoAlgorithmIdRsaPss:
      return RsaPssTag;
    case WebCryptoAlgorithmIdEcdsa:
      return EcdsaTag;
    case WebCryptoAlgorithmIdEcdh:
      return EcdhTag;
    case WebCryptoAlgorithmIdHkdf:
      return HkdfTag;
    case WebCryptoAlgorithmIdPbkdf2:
      return Pbkdf2Tag;
  }
  NOTREACHED() << "Unknown algorithm ID " << id;
  return 0;
}

uint32_t asymmetricKeyTypeForWireFormat(WebCryptoKeyType keyType) {
  switch (keyType) {
    case WebCryptoKeyTypePublic:
      return PublicKeyType;
    case WebCryptoKeyTypePrivate:
      return PrivateKeyType;
    case WebCryptoKeyTypeSecret:
      break;
  }
  NOTREACHED() << "Unknown asymmetric key type " << keyType;
  return 0;
}

uint32_t namedCurveForWireFormat(WebCryptoNamedCurve namedCurve) {
  switch (namedCurve) {
    case WebCryptoNamedCurveP256:
      return P256Tag;
    case WebCryptoNamedCurveP384:
      return P384Tag;
    case WebCryptoNamedCurveP521:
      return P521Tag;
  }
  NOTREACHED() << "Unknown named curve " << namedCurve;
  return 0;
}

uint32_t keyUsagesForWireFormat(WebCryptoKeyUsageMask usages,
                                bool extractable) {
  // Reminder to update this when adding new key usages.
  static_assert(EndOfWebCryptoKeyUsage == (1 << 7) + 1,
                "update required when adding new key usages");
  uint32_t value = 0;
  if (extractable)
    value |= ExtractableUsage;
  if (usages & WebCryptoKeyUsageEncrypt)
    value |= EncryptUsage;
  if (usages & WebCryptoKeyUsageDecrypt)
    value |= DecryptUsage;
  if (usages & WebCryptoKeyUsageSign)
    value |= SignUsage;
  if (usages & WebCryptoKeyUsageVerify)
    value |= VerifyUsage;
  if (usages & WebCryptoKeyUsageDeriveKey)
    value |= DeriveKeyUsage;
  if (usages & WebCryptoKeyUsageWrapKey)
    value |= WrapKeyUsage;
  if (usages & WebCryptoKeyUsageUnwrapKey)
    value |= UnwrapKeyUsage;
  if (usages & WebCryptoKeyUsageDeriveBits)
    value |= DeriveBitsUsage;
  return value;
}

}  // namespace

bool V8ScriptValueSerializerForModules::writeCryptoKey(
    const WebCryptoKey& key,
    ExceptionState& exceptionState) {
  writeTag(CryptoKeyTag);

  // Write params.
  const WebCryptoKeyAlgorithm& algorithm = key.algorithm();
  switch (algorithm.paramsType()) {
    case WebCryptoKeyAlgorithmParamsTypeAes: {
      const auto& params = *algorithm.aesParams();
      writeOneByte(AesKeyTag);
      writeUint32(algorithmIdForWireFormat(algorithm.id()));
      DCHECK_EQ(0, params.lengthBits() % 8);
      writeUint32(params.lengthBits() / 8);
      break;
    }
    case WebCryptoKeyAlgorithmParamsTypeHmac: {
      const auto& params = *algorithm.hmacParams();
      writeOneByte(HmacKeyTag);
      DCHECK_EQ(0u, params.lengthBits() % 8);
      writeUint32(params.lengthBits() / 8);
      writeUint32(algorithmIdForWireFormat(params.hash().id()));
      break;
    }
    case WebCryptoKeyAlgorithmParamsTypeRsaHashed: {
      const auto& params = *algorithm.rsaHashedParams();
      writeOneByte(RsaHashedKeyTag);
      writeUint32(algorithmIdForWireFormat(algorithm.id()));
      writeUint32(asymmetricKeyTypeForWireFormat(key.type()));
      writeUint32(params.modulusLengthBits());
      writeUint32(params.publicExponent().size());
      writeRawBytes(params.publicExponent().data(),
                    params.publicExponent().size());
      writeUint32(algorithmIdForWireFormat(params.hash().id()));
      break;
    }
    case WebCryptoKeyAlgorithmParamsTypeEc: {
      const auto& params = *algorithm.ecParams();
      writeOneByte(EcKeyTag);
      writeUint32(algorithmIdForWireFormat(algorithm.id()));
      writeUint32(asymmetricKeyTypeForWireFormat(key.type()));
      writeUint32(namedCurveForWireFormat(params.namedCurve()));
      break;
    }
    case WebCryptoKeyAlgorithmParamsTypeNone:
      DCHECK(WebCryptoAlgorithm::isKdf(algorithm.id()));
      writeOneByte(NoParamsKeyTag);
      writeUint32(algorithmIdForWireFormat(algorithm.id()));
      break;
  }

  // Write key usages.
  writeUint32(keyUsagesForWireFormat(key.usages(), key.extractable()));

  // Write key data.
  WebVector<uint8_t> keyData;
  if (!Platform::current()->crypto()->serializeKeyForClone(key, keyData) ||
      keyData.size() > std::numeric_limits<uint32_t>::max()) {
    exceptionState.throwDOMException(DataCloneError,
                                     "A CryptoKey object could not be cloned.");
    return false;
  }
  writeUint32(keyData.size());
  writeRawBytes(keyData.data(), keyData.size());

  return true;
}

}  // namespace blink
