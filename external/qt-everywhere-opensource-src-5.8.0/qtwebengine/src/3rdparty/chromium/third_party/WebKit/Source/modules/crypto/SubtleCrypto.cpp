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

#include "modules/crypto/SubtleCrypto.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMArrayPiece.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/CryptoHistograms.h"
#include "modules/crypto/CryptoKey.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "platform/JSONValues.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"

// TODO(eroman): Change the public blink::WebCrypto interface to allow
//               transferring ownership of data buffers instead of just taking
//               a raw pointer+length. This will avoid an extra copy.

namespace blink {

static bool parseAlgorithm(const AlgorithmIdentifier& raw, WebCryptoOperation op, WebCryptoAlgorithm& algorithm, CryptoResult* result)
{
    AlgorithmError error;
    bool success = normalizeAlgorithm(raw, op, algorithm, &error);
    if (!success)
        result->completeWithError(error.errorType, error.errorDetails);
    return success;
}

static bool canAccessWebCrypto(ScriptState* scriptState, CryptoResult* result)
{
    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage, ExecutionContext::WebCryptoSecureContextCheck)) {
        result->completeWithError(WebCryptoErrorTypeNotSupported, errorMessage);
        return false;
    }

    return true;
}

static bool copyStringProperty(const char* property, const Dictionary& source, JSONObject* destination)
{
    String value;
    if (!DictionaryHelper::get(source, property, value))
        return false;
    destination->setString(property, value);
    return true;
}

static bool copySequenceOfStringProperty(const char* property, const Dictionary& source, JSONObject* destination)
{
    Vector<String> value;
    if (!DictionaryHelper::get(source, property, value))
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
static bool copyJwkDictionaryToJson(const Dictionary& dict, Vector<uint8_t>& jsonUtf8, CryptoResult* result)
{
    RefPtr<JSONObject> jsonObject = JSONObject::create();

    if (!copyStringProperty("kty", dict, jsonObject.get())) {
        result->completeWithError(WebCryptoErrorTypeData, "The required JWK member \"kty\" was missing");
        return false;
    }

    copyStringProperty("use", dict, jsonObject.get());
    copySequenceOfStringProperty("key_ops", dict, jsonObject.get());
    copyStringProperty("alg", dict, jsonObject.get());

    bool ext;
    if (DictionaryHelper::get(dict, "ext", ext))
        jsonObject->setBoolean("ext", ext);

    const char* const propertyNames[] = { "d", "n", "e", "p", "q", "dp", "dq", "qi", "k", "crv", "x", "y" };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(propertyNames); ++i)
        copyStringProperty(propertyNames[i], dict, jsonObject.get());

    String json = jsonObject->toJSONString();
    jsonUtf8.clear();
    jsonUtf8.append(json.utf8().data(), json.utf8().length());
    return true;
}

static Vector<uint8_t> copyBytes(const DOMArrayPiece& source)
{
    Vector<uint8_t> result;
    result.append(reinterpret_cast<const uint8_t*>(source.data()), source.byteLength());
    return result;
}

SubtleCrypto::SubtleCrypto()
{
}

ScriptPromise SubtleCrypto::encrypt(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* key, const BufferSource& rawData)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-encrypt

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    // 14.3.1.2: Let data be the result of getting a copy of the bytes held by
    //           the data parameter passed to the encrypt method.
    Vector<uint8_t> data = copyBytes(rawData);

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationEncrypt, algorithm, result))
        return promise;

    if (!key->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageEncrypt, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, key->key());
    Platform::current()->crypto()->encrypt(algorithm, key->key(), data.data(), data.size(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::decrypt(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* key, const BufferSource& rawData)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-decrypt

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    // 14.3.2.2: Let data be the result of getting a copy of the bytes held by
    //           the data parameter passed to the decrypt method.
    Vector<uint8_t> data = copyBytes(rawData);

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDecrypt, algorithm, result))
        return promise;

    if (!key->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageDecrypt, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, key->key());
    Platform::current()->crypto()->decrypt(algorithm, key->key(), data.data(), data.size(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::sign(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* key, const BufferSource& rawData)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-sign

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    // 14.3.3.2: Let data be the result of getting a copy of the bytes held by
    //           the data parameter passed to the sign method.
    Vector<uint8_t> data = copyBytes(rawData);

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationSign, algorithm, result))
        return promise;

    if (!key->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageSign, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, key->key());
    Platform::current()->crypto()->sign(algorithm, key->key(), data.data(), data.size(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::verifySignature(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* key, const BufferSource& rawSignature, const BufferSource& rawData)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-verify

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    // 14.3.4.2: Let signature be the result of getting a copy of the bytes
    //           held by the signature parameter passed to the verify method.
    Vector<uint8_t> signature = copyBytes(rawSignature);

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationVerify, algorithm, result))
        return promise;

    // 14.3.4.5: Let data be the result of getting a copy of the bytes held by
    //           the data parameter passed to the verify method.
    Vector<uint8_t> data = copyBytes(rawData);

    if (!key->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageVerify, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, key->key());
    Platform::current()->crypto()->verifySignature(algorithm, key->key(), signature.data(), signature.size(), data.data(), data.size(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::digest(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, const BufferSource& rawData)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-digest

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    // 14.3.5.2: Let data be the result of getting a copy of the bytes held
    //              by the data parameter passed to the digest method.
    Vector<uint8_t> data = copyBytes(rawData);

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDigest, algorithm, result))
        return promise;

    histogramAlgorithm(scriptState->getExecutionContext(), algorithm);
    Platform::current()->crypto()->digest(algorithm, data.data(), data.size(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::generateKey(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyUsageMask keyUsages;
    if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
        return promise;

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationGenerateKey, algorithm, result))
        return promise;

    histogramAlgorithm(scriptState->getExecutionContext(), algorithm);
    Platform::current()->crypto()->generateKey(algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::importKey(ScriptState* scriptState, const String& rawFormat, const ArrayBufferOrArrayBufferViewOrDictionary& rawKeyData, const AlgorithmIdentifier& rawAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-importKey

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyFormat format;
    if (!CryptoKey::parseFormat(rawFormat, format, result))
        return promise;

    if (rawKeyData.isDictionary()) {
        if (format != WebCryptoKeyFormatJwk) {
            result->completeWithError(WebCryptoErrorTypeData, "Key data must be a buffer for non-JWK formats");
            return promise;
        }
    } else if (format == WebCryptoKeyFormatJwk) {
        result->completeWithError(WebCryptoErrorTypeData, "Key data must be an object for JWK import");
        return promise;
    }

    WebCryptoKeyUsageMask keyUsages;
    if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
        return promise;

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationImportKey, algorithm, result))
        return promise;

    Vector<uint8_t> keyData;
    if (rawKeyData.isArrayBuffer()) {
        keyData = copyBytes(rawKeyData.getAsArrayBuffer());
    } else if (rawKeyData.isArrayBufferView()) {
        keyData = copyBytes(rawKeyData.getAsArrayBufferView());
    } else if (rawKeyData.isDictionary()) {
        if (!copyJwkDictionaryToJson(rawKeyData.getAsDictionary(), keyData, result))
            return promise;
    }
    histogramAlgorithm(scriptState->getExecutionContext(), algorithm);
    Platform::current()->crypto()->importKey(format, keyData.data(), keyData.size(), algorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::exportKey(ScriptState* scriptState, const String& rawFormat, CryptoKey* key)
{
    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyFormat format;
    if (!CryptoKey::parseFormat(rawFormat, format, result))
        return promise;

    if (!key->extractable()) {
        result->completeWithError(WebCryptoErrorTypeInvalidAccess, "key is not extractable");
        return promise;
    }

    histogramKey(scriptState->getExecutionContext(), key->key());
    Platform::current()->crypto()->exportKey(format, key->key(), result->result());
    return promise;
}

ScriptPromise SubtleCrypto::wrapKey(ScriptState* scriptState, const String& rawFormat, CryptoKey* key, CryptoKey* wrappingKey, const AlgorithmIdentifier& rawWrapAlgorithm)
{
    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyFormat format;
    if (!CryptoKey::parseFormat(rawFormat, format, result))
        return promise;

    WebCryptoAlgorithm wrapAlgorithm;
    if (!parseAlgorithm(rawWrapAlgorithm, WebCryptoOperationWrapKey, wrapAlgorithm, result))
        return promise;

    if (!key->extractable()) {
        result->completeWithError(WebCryptoErrorTypeInvalidAccess, "key is not extractable");
        return promise;
    }

    if (!wrappingKey->canBeUsedForAlgorithm(wrapAlgorithm, WebCryptoKeyUsageWrapKey, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), wrapAlgorithm, wrappingKey->key());
    histogramKey(scriptState->getExecutionContext(), key->key());
    Platform::current()->crypto()->wrapKey(format, key->key(), wrappingKey->key(), wrapAlgorithm, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::unwrapKey(ScriptState* scriptState, const String& rawFormat, const BufferSource& rawWrappedKey, CryptoKey* unwrappingKey, const AlgorithmIdentifier& rawUnwrapAlgorithm, const AlgorithmIdentifier& rawUnwrappedKeyAlgorithm, bool extractable, const Vector<String>& rawKeyUsages)
{
    // Method described by: https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-unwrapKey

    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyFormat format;
    if (!CryptoKey::parseFormat(rawFormat, format, result))
        return promise;

    WebCryptoKeyUsageMask keyUsages;
    if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
        return promise;

    // 14.3.12.2: Let wrappedKey be the result of getting a copy of the bytes
    //            held by the wrappedKey parameter passed to the unwrapKey
    //            method.
    Vector<uint8_t> wrappedKey = copyBytes(rawWrappedKey);

    WebCryptoAlgorithm unwrapAlgorithm;
    if (!parseAlgorithm(rawUnwrapAlgorithm, WebCryptoOperationUnwrapKey, unwrapAlgorithm, result))
        return promise;

    WebCryptoAlgorithm unwrappedKeyAlgorithm;
    if (!parseAlgorithm(rawUnwrappedKeyAlgorithm, WebCryptoOperationImportKey, unwrappedKeyAlgorithm, result))
        return promise;

    if (!unwrappingKey->canBeUsedForAlgorithm(unwrapAlgorithm, WebCryptoKeyUsageUnwrapKey, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), unwrapAlgorithm, unwrappingKey->key());
    histogramAlgorithm(scriptState->getExecutionContext(), unwrappedKeyAlgorithm);
    Platform::current()->crypto()->unwrapKey(format, wrappedKey.data(), wrappedKey.size(), unwrappingKey->key(), unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::deriveBits(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* baseKey, unsigned lengthBits)
{
    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDeriveBits, algorithm, result))
        return promise;

    if (!baseKey->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageDeriveBits, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, baseKey->key());
    Platform::current()->crypto()->deriveBits(algorithm, baseKey->key(), lengthBits, result->result());
    return promise;
}

ScriptPromise SubtleCrypto::deriveKey(ScriptState* scriptState, const AlgorithmIdentifier& rawAlgorithm, CryptoKey* baseKey, const AlgorithmIdentifier& rawDerivedKeyType, bool extractable, const Vector<String>& rawKeyUsages)
{
    CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
    ScriptPromise promise = result->promise();

    if (!canAccessWebCrypto(scriptState, result))
        return promise;

    WebCryptoKeyUsageMask keyUsages;
    if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
        return promise;

    WebCryptoAlgorithm algorithm;
    if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDeriveBits, algorithm, result))
        return promise;

    if (!baseKey->canBeUsedForAlgorithm(algorithm, WebCryptoKeyUsageDeriveKey, result))
        return promise;

    WebCryptoAlgorithm importAlgorithm;
    if (!parseAlgorithm(rawDerivedKeyType, WebCryptoOperationImportKey, importAlgorithm, result))
        return promise;

    WebCryptoAlgorithm keyLengthAlgorithm;
    if (!parseAlgorithm(rawDerivedKeyType, WebCryptoOperationGetKeyLength, keyLengthAlgorithm, result))
        return promise;

    histogramAlgorithmAndKey(scriptState->getExecutionContext(), algorithm, baseKey->key());
    histogramAlgorithm(scriptState->getExecutionContext(), importAlgorithm);
    Platform::current()->crypto()->deriveKey(algorithm, baseKey->key(), importAlgorithm, keyLengthAlgorithm, extractable, keyUsages, result->result());
    return promise;
}

} // namespace blink
