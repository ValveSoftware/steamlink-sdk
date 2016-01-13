/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/crypto/SubtleCrypto.h"

#include "bindings/v8/Dictionary.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "platform/JSONValues.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

// Seems like the generated bindings should take care of these however it
// currently doesn't. See also http://crbug.com/264520
static bool ensureNotNull(const ArrayPiece& x, const char* paramName, CryptoResult* result)
{
    if (x.isNull()) {
        String message = String("Invalid ") + paramName + String(" argument");
        result->completeWithError(blink::WebCryptoErrorTypeType, blink::WebString(message));
        return false;
    }
    return true;
}

static bool ensureNotNull(Key* key, const char* paramName, CryptoResult* result)
{
    if (!key) {
        String message = String("Invalid ") + paramName + String(" argument");
        result->completeWithError(blink::WebCryptoErrorTypeType, blink::WebString(message));
        return false;
    }
    return true;
}

static bool parseAlgorithm(const Dictionary& raw, blink::WebCryptoOperation op, blink::WebCryptoAlgorithm& algorithm, CryptoResult* result)
{
    AlgorithmError error;
    bool success = normalizeAlgorithm(raw, op, algorithm, &error);
    if (!success)
        result->completeWithError(error.errorType, error.errorDetails);
    return success;
}

static bool canAccessWebCrypto(ScriptState* scriptState, CryptoResult* result)
{
    const SecurityOrigin* origin = scriptState->executionContext()->securityOrigin();
    if (!origin->canAccessFeatureRequiringSecureOrigin()) {
        result->completeWithError(blink::WebCryptoErrorTypeNotSupported, "WebCrypto is only supported over secure origins. See http://crbug.com/373032");
        return false;
    }

    return true;
}

static ScriptPromise startCryptoOperation(ScriptState* scriptState, const Dictionary& rawAlgorithm, Key* key, blink::WebCryptoOperation operationType, const ArrayPiece& signature, const ArrayPiece& dataBuffer)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    bool requiresKey = operationType != blink::WebCryptoOperationDigest;

    if (requiresKey && !ensureNotNull(key, "key", result.get()))
        return promise;
    if (operationType == blink::WebCryptoOperationVerify && !ensureNotNull(signature, "signature", result.get()))
        return promise;
    if (!ensureNotNull(dataBuffer, "dataBuffer", result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, operationType, algorithm, result.get()))
        return promise;

    if (requiresKey && !key->canBeUsedForAlgorithm(algorithm, operationType, result.get()))
        return promise;

    const unsigned char* data = dataBuffer.bytes();
    unsigned dataSize = dataBuffer.byteLength();

    switch (operationType) {
    case blink::WebCryptoOperationEncrypt:
        blink::Platform::current()->crypto()->encrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case blink::WebCryptoOperationDecrypt:
        blink::Platform::current()->crypto()->decrypt(algorithm, key->key(), data, dataSize, result->result());
        break;
    case blink::WebCryptoOperationSign:
        blink::Platform::current()->crypto()->sign(algorithm, key->key(), data, dataSize, result->result());
        break;
    case blink::WebCryptoOperationVerify:
        blink::Platform::current()->crypto()->verifySignature(algorithm, key->key(), signature.bytes(), signature.byteLength(), data, dataSize, result->result());
        break;
    case blink::WebCryptoOperationDigest:
        blink::Platform::current()->crypto()->digest(algorithm, data, dataSize, result->result());
        break;
    default:
        ASSERT_NOT_REACHED();
        return ScriptPromise();
    }

    return promise;
}

static bool copyStringProperty(const char* property, const Dictionary& source, JSONObject* destination)
{
    String value;
    if (!source.get(property, value))
        return false;
    destination->setString(property, value);
    return true;
}

static bool copySequenceOfStringProperty(const char* property, const Dictionary& source, JSONObject* destination)
{
    Vector<String> value;
    if (!source.get(property, value))
        return false;
    RefPtr<JSONArray> jsonArray = JSONArray::create();
    for (unsigned i = 0; i < value.size(); ++i)
        jsonArray->pushString(value[i]);
    destination->setArray(property, jsonArray.release());
    return true;
}

// FIXME: At the time of writing this is not a part of the spec. It is based an
// an unpublished editor's draft for:
//   https://www.w3.org/Bugs/Public/show_bug.cgi?id=24963
// See http://crbug.com/373917.
static bool copyJwkDictionaryToJson(const Dictionary& dict, CString& jsonUtf8, CryptoResult* result)
{
    RefPtr<JSONObject> jsonObject = JSONObject::create();

    if (!copyStringProperty("kty", dict, jsonObject.get())) {
        result->completeWithError(blink::WebCryptoErrorTypeData, "The required JWK property \"kty\" was missing");
        return false;
    }

    copyStringProperty("use", dict, jsonObject.get());
    copySequenceOfStringProperty("key_ops", dict, jsonObject.get());
    copyStringProperty("alg", dict, jsonObject.get());

    bool ext;
    if (dict.get("ext", ext))
        jsonObject->setBoolean("ext", ext);

    const char* const propertyNames[] = { "d", "n", "e", "p", "q", "dp", "dq", "qi", "k" };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(propertyNames); ++i)
        copyStringProperty(propertyNames[i], dict, jsonObject.get());

    String json = jsonObject->toJSONString();
    jsonUtf8 = json.utf8();
    return true;
}

SubtleCrypto::SubtleCrypto()
{
    ScriptWrappable::init(this);
}

ScriptPromise SubtleCrypto::encrypt(ScriptState* scriptState, const Dictionary& rawAlgorithm, Key* key, const ArrayPiece& data)
{
    return startCryptoOperation(scriptState, rawAlgorithm, key, blink::WebCryptoOperationEncrypt, ArrayPiece(), data);
}

ScriptPromise SubtleCrypto::decrypt(ScriptState* scriptState, const Dictionary& rawAlgorithm, Key* key, const ArrayPiece& data)
{
    return startCryptoOperation(scriptState, rawAlgorithm, key, blink::WebCryptoOperationDecrypt, ArrayPiece(), data);
}

ScriptPromise SubtleCrypto::sign(ScriptState* scriptState, const Dictionary& rawAlgorithm, Key* key, const ArrayPiece& data)
{
    return startCryptoOperation(scriptState, rawAlgorithm, key, blink::WebCryptoOperationSign, ArrayPiece(), data);
}

ScriptPromise SubtleCrypto::verifySignature(ScriptState* scriptState, const Dictionary& rawAlgorithm, Key* key, const ArrayPiece& signature, const ArrayPiece& data)
{
    return startCryptoOperation(scriptState, rawAlgorithm, key, blink::WebCryptoOperationVerify, signature, data);
}

ScriptPromise SubtleCrypto::digest(ScriptState* scriptState, const Dictionary& rawAlgorithm, const ArrayPiece& data)
{
    return startCryptoOperation(scriptState, rawAlgorithm, 0, blink::WebCryptoOperationDigest, ArrayPiece(), data);
}

ScriptPromise SubtleCrypto::generateKey(ScriptState* scriptState, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, blink::WebCryptoOperationGenerateKey, algorithm, result.get()))
        return promise;

    blink::Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(ScriptState* scriptState, const String& rawFormat, const ArrayPiece& keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    if (!ensureNotNull(keyData, "keyData", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    if (format == blink::WebCryptoKeyFormatJwk) {
        result->completeWithError(blink::WebCryptoErrorTypeData, "Key data must be an object for JWK import");
        return promise;
    }

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, blink::WebCryptoOperationImportKey, algorithm, result.get()))
        return promise;

    blink::Platform::current()->crypto()->importKey(format, keyData.bytes(), keyData.byteLength(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(ScriptState* scriptState, const String& rawFormat, const Dictionary& keyData, const Dictionary& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    if (format != blink::WebCryptoKeyFormatJwk) {
        result->completeWithError(blink::WebCryptoErrorTypeData, "Key data must be a buffer for non-JWK formats");
        return promise;
    }

    blink::WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, blink::WebCryptoOperationImportKey, algorithm, result.get()))
        return promise;

    CString jsonUtf8;
    if (!copyJwkDictionaryToJson(keyData, jsonUtf8, result.get()))
        return promise;

    blink::Platform::current()->crypto()->importKey(format, reinterpret_cast<const unsigned char*>(jsonUtf8.data()), jsonUtf8.length(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::exportKey(ScriptState* scriptState, const String& rawFormat, Key* key)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    if (!ensureNotNull(key, "key", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    if (!key->extractable()) {
        result->completeWithError(blink::WebCryptoErrorTypeInvalidAccess, "key is not extractable");
        return promise;
    }

    blink::Platform::current()->crypto()->exportKey(format, key->key(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::wrapKey(ScriptState* scriptState, const String& rawFormat, Key* key, Key* wrappingKey, const Dictionary& rawWrapAlgorithm)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    if (!ensureNotNull(key, "key", result.get()))
        return promise;

    if (!ensureNotNull(wrappingKey, "wrappingKey", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoAlgorithm wrapAlgorithm;
    if (!parseAlgorithm(rawWrapAlgorithm, blink::WebCryptoOperationWrapKey, wrapAlgorithm, result.get()))
        return promise;

    if (!key->extractable()) {
        result->completeWithError(blink::WebCryptoErrorTypeInvalidAccess, "key is not extractable");
        return promise;
    }

    if (!wrappingKey->canBeUsedForAlgorithm(wrapAlgorithm, blink::WebCryptoOperationWrapKey, result.get()))
        return promise;

    blink::Platform::current()->crypto()->wrapKey(format, key->key(), wrappingKey->key(), wrapAlgorithm, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::unwrapKey(ScriptState* scriptState, const String& rawFormat, const ArrayPiece& wrappedKey, Key* unwrappingKey, const Dictionary& rawUnwrapAlgorithm, const Dictionary& rawUnwrappedKeyAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    RefPtr<CryptoResultImpl> result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result.get()))
        return promise;

    if (!ensureNotNull(wrappedKey, "wrappedKey", result.get()))
        return promise;
    if (!ensureNotNull(unwrappingKey, "unwrappingKey", result.get()))
        return promise;

    blink::WebCryptoKeyFormat format;
    if (!Key::parseFormat(rawFormat, format, result.get()))
        return promise;

    blink::WebCryptoKeyUsageMask keyUsages;
    if (!Key::parseUsageMask(rawKeyUsages, keyUsages, result.get()))
        return promise;

    blink::WebCryptoAlgorithm unwrapAlgorithm;
    if (!parseAlgorithm(rawUnwrapAlgorithm, blink::WebCryptoOperationUnwrapKey, unwrapAlgorithm, result.get()))
        return promise;

    blink::WebCryptoAlgorithm unwrappedKeyAlgorithm;
    if (!parseAlgorithm(rawUnwrappedKeyAlgorithm, blink::WebCryptoOperationImportKey, unwrappedKeyAlgorithm, result.get()))
        return promise;

    if (!unwrappingKey->canBeUsedForAlgorithm(unwrapAlgorithm, blink::WebCryptoOperationUnwrapKey, result.get()))
        return promise;

    blink::Platform::current()->crypto()->unwrapKey(format, wrappedKey.bytes(), wrappedKey.byteLength(), unwrappingKey->key(), unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages, result->result());
    return promise;
}

} // namespace WebCore
