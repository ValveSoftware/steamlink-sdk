// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/ScriptValueSerializerForModules.h"

#include "bindings/core/v8/SerializationTag.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8CryptoKey.h"
#include "bindings/modules/v8/V8DOMFileSystem.h"
#include "bindings/modules/v8/V8RTCCertificate.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/peerconnection/RTCCertificate.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRTCCertificate.h"
#include "public/platform/WebRTCCertificateGenerator.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

enum CryptoKeyAlgorithmTag {
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

enum NamedCurveTag {
    P256Tag = 1,
    P384Tag = 2,
    P521Tag = 3,
};

enum CryptoKeyUsage {
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

enum CryptoKeySubTag {
    AesKeyTag = 1,
    HmacKeyTag = 2,
    // ID 3 was used by RsaKeyTag, while still behind experimental flag.
    RsaHashedKeyTag = 4,
    EcKeyTag = 5,
    NoParamsKeyTag = 6,
    // Maximum allowed value is 255
};

enum AssymetricCryptoKeyType {
    PublicKeyType = 1,
    PrivateKeyType = 2,
    // Maximum allowed value is 2^32-1
};


ScriptValueSerializerForModules::ScriptValueSerializerForModules(SerializedScriptValueWriter& writer, WebBlobInfoArray* blobInfo, ScriptState* scriptState)
    : ScriptValueSerializer(writer, blobInfo, scriptState)
{
}

ScriptValueSerializer::StateBase* ScriptValueSerializerForModules::writeDOMFileSystem(v8::Local<v8::Value> value, StateBase* next)
{
    DOMFileSystem* fs = V8DOMFileSystem::toImpl(value.As<v8::Object>());
    if (!fs)
        return 0;
    if (!fs->clonable())
        return handleError(Status::DataCloneError, "A FileSystem object could not be cloned.", next);

    toSerializedScriptValueWriterForModules(writer()).writeDOMFileSystem(fs->type(), fs->name(), fs->rootURL().getString());
    return 0;
}

bool ScriptValueSerializerForModules::writeCryptoKey(v8::Local<v8::Value> value)
{
    CryptoKey* key = V8CryptoKey::toImpl(value.As<v8::Object>());
    if (!key)
        return false;
    return toSerializedScriptValueWriterForModules(writer()).writeCryptoKey(key->key());
}

ScriptValueSerializer::StateBase* ScriptValueSerializerForModules::writeRTCCertificate(v8::Local<v8::Value> value, StateBase* next)
{
    RTCCertificate* certificate = V8RTCCertificate::toImpl(value.As<v8::Object>());
    if (!certificate)
        return handleError(Status::DataCloneError, "An RTCCertificate object could not be cloned.", next);
    toSerializedScriptValueWriterForModules(writer()).writeRTCCertificate(*certificate);
    return nullptr;
}

void SerializedScriptValueWriterForModules::writeDOMFileSystem(int type, const String& name, const String& url)
{
    append(DOMFileSystemTag);
    doWriteUint32(type);
    doWriteWebCoreString(name);
    doWriteWebCoreString(url);
}

bool SerializedScriptValueWriterForModules::writeCryptoKey(const WebCryptoKey& key)
{
    append(static_cast<uint8_t>(CryptoKeyTag));

    switch (key.algorithm().paramsType()) {
    case WebCryptoKeyAlgorithmParamsTypeAes:
        doWriteAesKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeHmac:
        doWriteHmacKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeRsaHashed:
        doWriteRsaHashedKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeEc:
        doWriteEcKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeNone:
        doWriteKeyWithoutParams(key);
        break;
    }

    doWriteKeyUsages(key.usages(), key.extractable());

    WebVector<uint8_t> keyData;
    if (!Platform::current()->crypto()->serializeKeyForClone(key, keyData))
        return false;

    doWriteUint32(keyData.size());
    append(keyData.data(), keyData.size());
    return true;
}

void SerializedScriptValueWriterForModules::writeRTCCertificate(const RTCCertificate& certificate)
{
    append(RTCCertificateTag);

    WebRTCCertificatePEM pem = certificate.certificateShallowCopy()->toPEM();
    doWriteWebCoreString(String(pem.privateKey().c_str()));
    doWriteWebCoreString(String(pem.certificate().c_str()));
}

void SerializedScriptValueWriterForModules::doWriteHmacKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().paramsType() == WebCryptoKeyAlgorithmParamsTypeHmac);

    append(static_cast<uint8_t>(HmacKeyTag));
    ASSERT(!(key.algorithm().hmacParams()->lengthBits() % 8));
    doWriteUint32(key.algorithm().hmacParams()->lengthBits() / 8);
    doWriteAlgorithmId(key.algorithm().hmacParams()->hash().id());
}

void SerializedScriptValueWriterForModules::doWriteAesKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().paramsType() == WebCryptoKeyAlgorithmParamsTypeAes);

    append(static_cast<uint8_t>(AesKeyTag));
    doWriteAlgorithmId(key.algorithm().id());
    // Converting the key length from bits to bytes is lossless and makes
    // it fit in 1 byte.
    ASSERT(!(key.algorithm().aesParams()->lengthBits() % 8));
    doWriteUint32(key.algorithm().aesParams()->lengthBits() / 8);
}

void SerializedScriptValueWriterForModules::doWriteRsaHashedKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().rsaHashedParams());
    append(static_cast<uint8_t>(RsaHashedKeyTag));

    doWriteAlgorithmId(key.algorithm().id());
    doWriteAsymmetricKeyType(key.type());

    const WebCryptoRsaHashedKeyAlgorithmParams* params = key.algorithm().rsaHashedParams();
    doWriteUint32(params->modulusLengthBits());
    doWriteUint32(params->publicExponent().size());
    append(params->publicExponent().data(), params->publicExponent().size());
    doWriteAlgorithmId(params->hash().id());
}

void SerializedScriptValueWriterForModules::doWriteEcKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().ecParams());
    append(static_cast<uint8_t>(EcKeyTag));

    doWriteAlgorithmId(key.algorithm().id());
    doWriteAsymmetricKeyType(key.type());
    doWriteNamedCurve(key.algorithm().ecParams()->namedCurve());
}

void SerializedScriptValueWriterForModules::doWriteKeyWithoutParams(const WebCryptoKey& key)
{
    ASSERT(WebCryptoAlgorithm::isKdf(key.algorithm().id()));
    append(static_cast<uint8_t>(NoParamsKeyTag));

    doWriteAlgorithmId(key.algorithm().id());
}

void SerializedScriptValueWriterForModules::doWriteAlgorithmId(WebCryptoAlgorithmId id)
{
    switch (id) {
    case WebCryptoAlgorithmIdAesCbc:
        return doWriteUint32(AesCbcTag);
    case WebCryptoAlgorithmIdHmac:
        return doWriteUint32(HmacTag);
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return doWriteUint32(RsaSsaPkcs1v1_5Tag);
    case WebCryptoAlgorithmIdSha1:
        return doWriteUint32(Sha1Tag);
    case WebCryptoAlgorithmIdSha256:
        return doWriteUint32(Sha256Tag);
    case WebCryptoAlgorithmIdSha384:
        return doWriteUint32(Sha384Tag);
    case WebCryptoAlgorithmIdSha512:
        return doWriteUint32(Sha512Tag);
    case WebCryptoAlgorithmIdAesGcm:
        return doWriteUint32(AesGcmTag);
    case WebCryptoAlgorithmIdRsaOaep:
        return doWriteUint32(RsaOaepTag);
    case WebCryptoAlgorithmIdAesCtr:
        return doWriteUint32(AesCtrTag);
    case WebCryptoAlgorithmIdAesKw:
        return doWriteUint32(AesKwTag);
    case WebCryptoAlgorithmIdRsaPss:
        return doWriteUint32(RsaPssTag);
    case WebCryptoAlgorithmIdEcdsa:
        return doWriteUint32(EcdsaTag);
    case WebCryptoAlgorithmIdEcdh:
        return doWriteUint32(EcdhTag);
    case WebCryptoAlgorithmIdHkdf:
        return doWriteUint32(HkdfTag);
    case WebCryptoAlgorithmIdPbkdf2:
        return doWriteUint32(Pbkdf2Tag);
    }
    ASSERT_NOT_REACHED();
}

void SerializedScriptValueWriterForModules::doWriteAsymmetricKeyType(WebCryptoKeyType keyType)
{
    switch (keyType) {
    case WebCryptoKeyTypePublic:
        doWriteUint32(PublicKeyType);
        break;
    case WebCryptoKeyTypePrivate:
        doWriteUint32(PrivateKeyType);
        break;
    case WebCryptoKeyTypeSecret:
        ASSERT_NOT_REACHED();
    }
}

void SerializedScriptValueWriterForModules::doWriteNamedCurve(WebCryptoNamedCurve namedCurve)
{
    switch (namedCurve) {
    case WebCryptoNamedCurveP256:
        return doWriteUint32(P256Tag);
    case WebCryptoNamedCurveP384:
        return doWriteUint32(P384Tag);
    case WebCryptoNamedCurveP521:
        return doWriteUint32(P521Tag);
    }
    ASSERT_NOT_REACHED();
}

void SerializedScriptValueWriterForModules::doWriteKeyUsages(const WebCryptoKeyUsageMask usages, bool extractable)
{
    // Reminder to update this when adding new key usages.
    static_assert(EndOfWebCryptoKeyUsage == (1 << 7) + 1, "update required when adding new key usages");

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

    doWriteUint32(value);
}

ScriptValueSerializer::StateBase* ScriptValueSerializerForModules::doSerializeObject(v8::Local<v8::Object> jsObject, StateBase* next)
{
    DCHECK(!jsObject.IsEmpty());

    if (V8DOMFileSystem::hasInstance(jsObject, isolate())) {
        greyObject(jsObject);
        return writeDOMFileSystem(jsObject, next);
    }
    if (V8CryptoKey::hasInstance(jsObject, isolate())) {
        greyObject(jsObject);
        if (!writeCryptoKey(jsObject))
            return handleError(Status::DataCloneError, "Couldn't serialize key data", next);
        return nullptr;
    }
    if (V8RTCCertificate::hasInstance(jsObject, isolate())) {
        greyObject(jsObject);
        return writeRTCCertificate(jsObject, next);
    }

    return ScriptValueSerializer::doSerializeObject(jsObject, next);
}

bool SerializedScriptValueReaderForModules::read(v8::Local<v8::Value>* value, ScriptValueDeserializer& deserializer)
{
    SerializationTag tag;
    if (!readTag(&tag))
        return false;
    switch (tag) {
    case DOMFileSystemTag:
        if (!readDOMFileSystem(value))
            return false;
        deserializer.pushObjectReference(*value);
        break;
    case CryptoKeyTag:
        if (!readCryptoKey(value))
            return false;
        deserializer.pushObjectReference(*value);
        break;
    case RTCCertificateTag:
        if (!readRTCCertificate(value))
            return false;
        deserializer.pushObjectReference(*value);
        break;
    default:
        return SerializedScriptValueReader::readWithTag(tag, value, deserializer);
    }
    return !value->IsEmpty();
}

bool SerializedScriptValueReaderForModules::readDOMFileSystem(v8::Local<v8::Value>* value)
{
    uint32_t type;
    String name;
    String url;
    if (!doReadUint32(&type))
        return false;
    if (!readWebCoreString(&name))
        return false;
    if (!readWebCoreString(&url))
        return false;
    DOMFileSystem* fs = DOMFileSystem::create(getScriptState()->getExecutionContext(), name, static_cast<FileSystemType>(type), KURL(ParsedURLString, url));
    *value = toV8(fs, getScriptState()->context()->Global(), isolate());
    return !value->IsEmpty();
}

bool SerializedScriptValueReaderForModules::readCryptoKey(v8::Local<v8::Value>* value)
{
    uint32_t rawKeyType;
    if (!doReadUint32(&rawKeyType))
        return false;

    WebCryptoKeyAlgorithm algorithm;
    WebCryptoKeyType type = WebCryptoKeyTypeSecret;

    switch (static_cast<CryptoKeySubTag>(rawKeyType)) {
    case AesKeyTag:
        if (!doReadAesKey(algorithm, type))
            return false;
        break;
    case HmacKeyTag:
        if (!doReadHmacKey(algorithm, type))
            return false;
        break;
    case RsaHashedKeyTag:
        if (!doReadRsaHashedKey(algorithm, type))
            return false;
        break;
    case EcKeyTag:
        if (!doReadEcKey(algorithm, type))
            return false;
        break;
    case NoParamsKeyTag:
        if (!doReadKeyWithoutParams(algorithm, type))
            return false;
        break;
    default:
        return false;
    }

    WebCryptoKeyUsageMask usages;
    bool extractable;
    if (!doReadKeyUsages(usages, extractable))
        return false;

    uint32_t keyDataLength;
    if (!doReadUint32(&keyDataLength))
        return false;

    if (position() + keyDataLength > length())
        return false;

    const uint8_t* keyData = allocate(keyDataLength);
    WebCryptoKey key = WebCryptoKey::createNull();
    if (!Platform::current()->crypto()->deserializeKeyForClone(
        algorithm, type, extractable, usages, keyData, keyDataLength, key)) {
        return false;
    }

    *value = toV8(CryptoKey::create(key), getScriptState()->context()->Global(), isolate());
    return !value->IsEmpty();
}

bool SerializedScriptValueReaderForModules::readRTCCertificate(v8::Local<v8::Value>* value)
{
    String pemPrivateKey;
    if (!readWebCoreString(&pemPrivateKey))
        return false;
    String pemCertificate;
    if (!readWebCoreString(&pemCertificate))
        return false;

    std::unique_ptr<WebRTCCertificateGenerator> certificateGenerator = wrapUnique(
        Platform::current()->createRTCCertificateGenerator());

    std::unique_ptr<WebRTCCertificate> certificate(
        certificateGenerator->fromPEM(
            pemPrivateKey.utf8().data(),
            pemCertificate.utf8().data()));
    RTCCertificate* jsCertificate = new RTCCertificate(std::move(certificate));

    *value = toV8(jsCertificate, getScriptState()->context()->Global(), isolate());
    return !value->IsEmpty();
}

bool SerializedScriptValueReaderForModules::doReadHmacKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    uint32_t lengthBytes;
    if (!doReadUint32(&lengthBytes))
        return false;
    WebCryptoAlgorithmId hash;
    if (!doReadAlgorithmId(hash))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createHmac(hash, lengthBytes * 8);
    type = WebCryptoKeyTypeSecret;
    return !algorithm.isNull();
}

bool SerializedScriptValueReaderForModules::doReadAesKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;
    uint32_t lengthBytes;
    if (!doReadUint32(&lengthBytes))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createAes(id, lengthBytes * 8);
    type = WebCryptoKeyTypeSecret;
    return !algorithm.isNull();
}

bool SerializedScriptValueReaderForModules::doReadRsaHashedKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;

    if (!doReadAsymmetricKeyType(type))
        return false;

    uint32_t modulusLengthBits;
    if (!doReadUint32(&modulusLengthBits))
        return false;

    uint32_t publicExponentSize;
    if (!doReadUint32(&publicExponentSize))
        return false;

    if (position() + publicExponentSize > length())
        return false;

    const uint8_t* publicExponent = allocate(publicExponentSize);
    WebCryptoAlgorithmId hash;
    if (!doReadAlgorithmId(hash))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createRsaHashed(id, modulusLengthBits, publicExponent, publicExponentSize, hash);

    return !algorithm.isNull();
}

bool SerializedScriptValueReaderForModules::doReadEcKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;

    if (!doReadAsymmetricKeyType(type))
        return false;

    WebCryptoNamedCurve namedCurve;
    if (!doReadNamedCurve(namedCurve))
        return false;

    algorithm = WebCryptoKeyAlgorithm::createEc(id, namedCurve);
    return !algorithm.isNull();
}

bool SerializedScriptValueReaderForModules::doReadKeyWithoutParams(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createWithoutParams(id);
    type = WebCryptoKeyTypeSecret;
    return !algorithm.isNull();
}

bool SerializedScriptValueReaderForModules::doReadAlgorithmId(WebCryptoAlgorithmId& id)
{
    uint32_t rawId;
    if (!doReadUint32(&rawId))
        return false;

    switch (static_cast<CryptoKeyAlgorithmTag>(rawId)) {
    case AesCbcTag:
        id = WebCryptoAlgorithmIdAesCbc;
        return true;
    case HmacTag:
        id = WebCryptoAlgorithmIdHmac;
        return true;
    case RsaSsaPkcs1v1_5Tag:
        id = WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
        return true;
    case Sha1Tag:
        id = WebCryptoAlgorithmIdSha1;
        return true;
    case Sha256Tag:
        id = WebCryptoAlgorithmIdSha256;
        return true;
    case Sha384Tag:
        id = WebCryptoAlgorithmIdSha384;
        return true;
    case Sha512Tag:
        id = WebCryptoAlgorithmIdSha512;
        return true;
    case AesGcmTag:
        id = WebCryptoAlgorithmIdAesGcm;
        return true;
    case RsaOaepTag:
        id = WebCryptoAlgorithmIdRsaOaep;
        return true;
    case AesCtrTag:
        id = WebCryptoAlgorithmIdAesCtr;
        return true;
    case AesKwTag:
        id = WebCryptoAlgorithmIdAesKw;
        return true;
    case RsaPssTag:
        id = WebCryptoAlgorithmIdRsaPss;
        return true;
    case EcdsaTag:
        id = WebCryptoAlgorithmIdEcdsa;
        return true;
    case EcdhTag:
        id = WebCryptoAlgorithmIdEcdh;
        return true;
    case HkdfTag:
        id = WebCryptoAlgorithmIdHkdf;
        return true;
    case Pbkdf2Tag:
        id = WebCryptoAlgorithmIdPbkdf2;
        return true;
    }

    return false;
}

bool SerializedScriptValueReaderForModules::doReadAsymmetricKeyType(WebCryptoKeyType& type)
{
    uint32_t rawType;
    if (!doReadUint32(&rawType))
        return false;

    switch (static_cast<AssymetricCryptoKeyType>(rawType)) {
    case PublicKeyType:
        type = WebCryptoKeyTypePublic;
        return true;
    case PrivateKeyType:
        type = WebCryptoKeyTypePrivate;
        return true;
    }

    return false;
}

bool SerializedScriptValueReaderForModules::doReadNamedCurve(WebCryptoNamedCurve& namedCurve)
{
    uint32_t rawName;
    if (!doReadUint32(&rawName))
        return false;

    switch (static_cast<NamedCurveTag>(rawName)) {
    case P256Tag:
        namedCurve = WebCryptoNamedCurveP256;
        return true;
    case P384Tag:
        namedCurve = WebCryptoNamedCurveP384;
        return true;
    case P521Tag:
        namedCurve = WebCryptoNamedCurveP521;
        return true;
    }

    return false;
}

bool SerializedScriptValueReaderForModules::doReadKeyUsages(WebCryptoKeyUsageMask& usages, bool& extractable)
{
    // Reminder to update this when adding new key usages.
    static_assert(EndOfWebCryptoKeyUsage == (1 << 7) + 1, "update required when adding new key usages");
    const uint32_t allPossibleUsages = ExtractableUsage | EncryptUsage | DecryptUsage | SignUsage | VerifyUsage | DeriveKeyUsage | WrapKeyUsage | UnwrapKeyUsage | DeriveBitsUsage;

    uint32_t rawUsages;
    if (!doReadUint32(&rawUsages))
        return false;

    // Make sure it doesn't contain an unrecognized usage value.
    if (rawUsages & ~allPossibleUsages)
        return false;

    usages = 0;

    extractable = rawUsages & ExtractableUsage;

    if (rawUsages & EncryptUsage)
        usages |= WebCryptoKeyUsageEncrypt;
    if (rawUsages & DecryptUsage)
        usages |= WebCryptoKeyUsageDecrypt;
    if (rawUsages & SignUsage)
        usages |= WebCryptoKeyUsageSign;
    if (rawUsages & VerifyUsage)
        usages |= WebCryptoKeyUsageVerify;
    if (rawUsages & DeriveKeyUsage)
        usages |= WebCryptoKeyUsageDeriveKey;
    if (rawUsages & WrapKeyUsage)
        usages |= WebCryptoKeyUsageWrapKey;
    if (rawUsages & UnwrapKeyUsage)
        usages |= WebCryptoKeyUsageUnwrapKey;
    if (rawUsages & DeriveBitsUsage)
        usages |= WebCryptoKeyUsageDeriveBits;

    return true;
}

ScriptValueDeserializerForModules::ScriptValueDeserializerForModules(SerializedScriptValueReaderForModules& reader, MessagePortArray* messagePorts, ArrayBufferContentsArray* arrayBufferContents, ImageBitmapContentsArray* imageBitmapContents)
    : ScriptValueDeserializer(reader, messagePorts, arrayBufferContents, imageBitmapContents)
{
}

bool ScriptValueDeserializerForModules::read(v8::Local<v8::Value>* value)
{
    return toSerializedScriptValueReaderForModules(reader()).read(value, *this);
}

} // namespace blink
