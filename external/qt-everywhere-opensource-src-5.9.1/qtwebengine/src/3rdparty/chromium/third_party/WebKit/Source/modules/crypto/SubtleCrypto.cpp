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
#include "platform/json/JSONValues.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"

namespace blink {

static bool parseAlgorithm(const AlgorithmIdentifier& raw,
                           WebCryptoOperation op,
                           WebCryptoAlgorithm& algorithm,
                           CryptoResult* result) {
  AlgorithmError error;
  bool success = normalizeAlgorithm(raw, op, algorithm, &error);
  if (!success)
    result->completeWithError(error.errorType, error.errorDetails);
  return success;
}

static bool canAccessWebCrypto(ScriptState* scriptState, CryptoResult* result) {
  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(
          errorMessage, ExecutionContext::WebCryptoSecureContextCheck)) {
    result->completeWithError(WebCryptoErrorTypeNotSupported, errorMessage);
    return false;
  }

  return true;
}

static bool copyStringProperty(const char* property,
                               const Dictionary& source,
                               JSONObject* destination) {
  String value;
  if (!DictionaryHelper::get(source, property, value))
    return false;
  destination->setString(property, value);
  return true;
}

static bool copySequenceOfStringProperty(const char* property,
                                         const Dictionary& source,
                                         JSONObject* destination) {
  Vector<String> value;
  if (!DictionaryHelper::get(source, property, value))
    return false;
  std::unique_ptr<JSONArray> jsonArray = JSONArray::create();
  for (unsigned i = 0; i < value.size(); ++i)
    jsonArray->pushString(value[i]);
  destination->setArray(property, std::move(jsonArray));
  return true;
}

// Parses a JsonWebKey dictionary. On success writes the result to
// |jsonUtf8| as a UTF8-encoded JSON octet string and returns true.
// On failure sets an error on |result| and returns false.
//
// Note: The choice of output as an octet string is to facilitate interop
// with the non-JWK formats, but does mean there is a second parsing step.
// This design choice should be revisited after crbug.com/614385).
//
// Defined by the WebCrypto spec as:
//
//    dictionary JsonWebKey {
//      DOMString kty;
//      DOMString use;
//      sequence<DOMString> key_ops;
//      DOMString alg;
//
//      boolean ext;
//
//      DOMString crv;
//      DOMString x;
//      DOMString y;
//      DOMString d;
//      DOMString n;
//      DOMString e;
//      DOMString p;
//      DOMString q;
//      DOMString dp;
//      DOMString dq;
//      DOMString qi;
//      sequence<RsaOtherPrimesInfo> oth;
//      DOMString k;
//    };
//
//    dictionary RsaOtherPrimesInfo {
//      DOMString r;
//      DOMString d;
//      DOMString t;
//    };
static bool parseJsonWebKey(const Dictionary& dict,
                            WebVector<uint8_t>& jsonUtf8,
                            CryptoResult* result) {
  // TODO(eroman): This implementation is incomplete and not spec compliant:
  //  * Properties need to be read in the definition order above
  //  * Preserve the type of optional parameters (crbug.com/385376)
  //  * Parse "oth" (crbug.com/441396)
  //  * Fail with TypeError (not DataError) if the input does not conform
  //    to a JsonWebKey
  std::unique_ptr<JSONObject> jsonObject = JSONObject::create();

  if (!copyStringProperty("kty", dict, jsonObject.get())) {
    result->completeWithError(WebCryptoErrorTypeData,
                              "The required JWK member \"kty\" was missing");
    return false;
  }

  copyStringProperty("use", dict, jsonObject.get());
  copySequenceOfStringProperty("key_ops", dict, jsonObject.get());
  copyStringProperty("alg", dict, jsonObject.get());

  bool ext;
  if (DictionaryHelper::get(dict, "ext", ext))
    jsonObject->setBoolean("ext", ext);

  const char* const propertyNames[] = {"d",  "n",  "e", "p",   "q", "dp",
                                       "dq", "qi", "k", "crv", "x", "y"};
  for (unsigned i = 0; i < WTF_ARRAY_LENGTH(propertyNames); ++i)
    copyStringProperty(propertyNames[i], dict, jsonObject.get());

  String json = jsonObject->toJSONString();
  jsonUtf8 = WebVector<uint8_t>(json.utf8().data(), json.utf8().length());
  return true;
}

static WebVector<uint8_t> copyBytes(const DOMArrayPiece& source) {
  return WebVector<uint8_t>(static_cast<uint8_t*>(source.data()),
                            source.byteLength());
}

SubtleCrypto::SubtleCrypto() {}

ScriptPromise SubtleCrypto::encrypt(ScriptState* scriptState,
                                    const AlgorithmIdentifier& rawAlgorithm,
                                    CryptoKey* key,
                                    const BufferSource& rawData) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-encrypt

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.1.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the encrypt method.
  WebVector<uint8_t> data = copyBytes(rawData);

  // 14.3.1.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "encrypt".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationEncrypt,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.1.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.1.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "encrypt", then throw an InvalidAccessError.
  if (!key->canBeUsedForAlgorithm(normalizedAlgorithm, WebCryptoKeyUsageEncrypt,
                                  result))
    return promise;

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, key->key());
  Platform::current()->crypto()->encrypt(normalizedAlgorithm, key->key(),
                                         std::move(data), result->result());
  return promise;
}

ScriptPromise SubtleCrypto::decrypt(ScriptState* scriptState,
                                    const AlgorithmIdentifier& rawAlgorithm,
                                    CryptoKey* key,
                                    const BufferSource& rawData) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-decrypt

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.2.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the decrypt method.
  WebVector<uint8_t> data = copyBytes(rawData);

  // 14.3.2.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "decrypt".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDecrypt,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.2.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.2.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "decrypt", then throw an InvalidAccessError.
  if (!key->canBeUsedForAlgorithm(normalizedAlgorithm, WebCryptoKeyUsageDecrypt,
                                  result))
    return promise;

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, key->key());
  Platform::current()->crypto()->decrypt(normalizedAlgorithm, key->key(),
                                         std::move(data), result->result());
  return promise;
}

ScriptPromise SubtleCrypto::sign(ScriptState* scriptState,
                                 const AlgorithmIdentifier& rawAlgorithm,
                                 CryptoKey* key,
                                 const BufferSource& rawData) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-sign

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.3.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the sign method.
  WebVector<uint8_t> data = copyBytes(rawData);

  // 14.3.3.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "sign".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationSign, normalizedAlgorithm,
                      result))
    return promise;

  // 14.3.3.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.3.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "sign", then throw an InvalidAccessError.
  if (!key->canBeUsedForAlgorithm(normalizedAlgorithm, WebCryptoKeyUsageSign,
                                  result))
    return promise;

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, key->key());
  Platform::current()->crypto()->sign(normalizedAlgorithm, key->key(),
                                      std::move(data), result->result());
  return promise;
}

ScriptPromise SubtleCrypto::verifySignature(
    ScriptState* scriptState,
    const AlgorithmIdentifier& rawAlgorithm,
    CryptoKey* key,
    const BufferSource& rawSignature,
    const BufferSource& rawData) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-verify

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.4.2: Let signature be the result of getting a copy of the bytes
  //           held by the signature parameter passed to the verify method.
  WebVector<uint8_t> signature = copyBytes(rawSignature);

  // 14.3.4.3: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the verify method.
  WebVector<uint8_t> data = copyBytes(rawData);

  // 14.3.4.4: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "verify".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationVerify,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.4.9: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.4.10: If the [[usages]] internal slot of key does not contain an
  //            entry that is "verify", then throw an InvalidAccessError.
  if (!key->canBeUsedForAlgorithm(normalizedAlgorithm, WebCryptoKeyUsageVerify,
                                  result))
    return promise;

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, key->key());
  Platform::current()->crypto()->verifySignature(
      normalizedAlgorithm, key->key(), std::move(signature), std::move(data),
      result->result());
  return promise;
}

ScriptPromise SubtleCrypto::digest(ScriptState* scriptState,
                                   const AlgorithmIdentifier& rawAlgorithm,
                                   const BufferSource& rawData) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-digest

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.5.2: Let data be the result of getting a copy of the bytes held
  //              by the data parameter passed to the digest method.
  WebVector<uint8_t> data = copyBytes(rawData);

  // 14.3.5.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "digest".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDigest,
                      normalizedAlgorithm, result))
    return promise;

  histogramAlgorithm(scriptState->getExecutionContext(), normalizedAlgorithm);
  Platform::current()->crypto()->digest(normalizedAlgorithm, std::move(data),
                                        result->result());
  return promise;
}

ScriptPromise SubtleCrypto::generateKey(ScriptState* scriptState,
                                        const AlgorithmIdentifier& rawAlgorithm,
                                        bool extractable,
                                        const Vector<String>& rawKeyUsages) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-generateKey

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  WebCryptoKeyUsageMask keyUsages;
  if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
    return promise;

  // 14.3.6.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "generateKey".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationGenerateKey,
                      normalizedAlgorithm, result))
    return promise;

  // NOTE: Steps (8) and (9) disallow empty usages on secret and private
  // keys. This normative requirement is enforced by the platform
  // implementation in the call below.

  histogramAlgorithm(scriptState->getExecutionContext(), normalizedAlgorithm);
  Platform::current()->crypto()->generateKey(normalizedAlgorithm, extractable,
                                             keyUsages, result->result());
  return promise;
}

ScriptPromise SubtleCrypto::importKey(
    ScriptState* scriptState,
    const String& rawFormat,
    const ArrayBufferOrArrayBufferViewOrDictionary& rawKeyData,
    const AlgorithmIdentifier& rawAlgorithm,
    bool extractable,
    const Vector<String>& rawKeyUsages) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-importKey

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

  // In the case of JWK keyData will hold the UTF8-encoded JSON for the
  // JsonWebKey, otherwise it holds a copy of the BufferSource.
  WebVector<uint8_t> keyData;

  switch (format) {
    // 14.3.9.2: If format is equal to the string "raw", "pkcs8", or "spki":
    //
    //  (1) If the keyData parameter passed to the importKey method is a
    //      JsonWebKey dictionary, throw a TypeError.
    //
    //  (2) Let keyData be the result of getting a copy of the bytes held by
    //      the keyData parameter passed to the importKey method.
    case WebCryptoKeyFormatRaw:
    case WebCryptoKeyFormatPkcs8:
    case WebCryptoKeyFormatSpki:
      if (rawKeyData.isArrayBuffer()) {
        keyData = copyBytes(rawKeyData.getAsArrayBuffer());
      } else if (rawKeyData.isArrayBufferView()) {
        keyData = copyBytes(rawKeyData.getAsArrayBufferView());
      } else {
        result->completeWithError(
            WebCryptoErrorTypeType,
            "Key data must be a BufferSource for non-JWK formats");
        return promise;
      }
      break;
    // 14.3.9.2: If format is equal to the string "jwk":
    //
    //  (1) If the keyData parameter passed to the importKey method is not a
    //      JsonWebKey dictionary, throw a TypeError.
    //
    //  (2) Let keyData be the keyData parameter passed to the importKey
    //      method.
    case WebCryptoKeyFormatJwk:
      if (rawKeyData.isDictionary()) {
        // TODO(eroman): To match the spec error order, parsing of the
        // JsonWebKey should be done earlier (at the WebIDL layer of
        // parameter checking), regardless of the format being "jwk".
        if (!parseJsonWebKey(rawKeyData.getAsDictionary(), keyData, result))
          return promise;
      } else {
        result->completeWithError(WebCryptoErrorTypeType,
                                  "Key data must be an object for JWK import");
        return promise;
      }
      break;
  }

  // 14.3.9.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "importKey".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationImportKey,
                      normalizedAlgorithm, result))
    return promise;

  histogramAlgorithm(scriptState->getExecutionContext(), normalizedAlgorithm);
  Platform::current()->crypto()->importKey(format, std::move(keyData),
                                           normalizedAlgorithm, extractable,
                                           keyUsages, result->result());
  return promise;
}

ScriptPromise SubtleCrypto::exportKey(ScriptState* scriptState,
                                      const String& rawFormat,
                                      CryptoKey* key) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-exportKey

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  WebCryptoKeyFormat format;
  if (!CryptoKey::parseFormat(rawFormat, format, result))
    return promise;

  // 14.3.10.6: If the [[extractable]] internal slot of key is false, then
  //            throw an InvalidAccessError.
  if (!key->extractable()) {
    result->completeWithError(WebCryptoErrorTypeInvalidAccess,
                              "key is not extractable");
    return promise;
  }

  histogramKey(scriptState->getExecutionContext(), key->key());
  Platform::current()->crypto()->exportKey(format, key->key(),
                                           result->result());
  return promise;
}

ScriptPromise SubtleCrypto::wrapKey(
    ScriptState* scriptState,
    const String& rawFormat,
    CryptoKey* key,
    CryptoKey* wrappingKey,
    const AlgorithmIdentifier& rawWrapAlgorithm) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-wrapKey

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  WebCryptoKeyFormat format;
  if (!CryptoKey::parseFormat(rawFormat, format, result))
    return promise;

  // 14.3.11.2: Let normalizedAlgorithm be the result of normalizing an
  //            algorithm, with alg set to algorithm and op set to "wrapKey".
  //
  // 14.3.11.3: If an error occurred, let normalizedAlgorithm be the result
  //            of normalizing an algorithm, with alg set to algorithm and op
  //            set to "encrypt".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawWrapAlgorithm, WebCryptoOperationWrapKey,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.11.9: If the name member of normalizedAlgorithm is not equal to the
  //            name attribute of the [[algorithm]] internal slot of
  //            wrappingKey then throw an InvalidAccessError.
  //
  // 14.3.11.10: If the [[usages]] internal slot of wrappingKey does not
  //             contain an entry that is "wrapKey", then throw an
  //             InvalidAccessError.
  if (!wrappingKey->canBeUsedForAlgorithm(normalizedAlgorithm,
                                          WebCryptoKeyUsageWrapKey, result))
    return promise;

  // TODO(crbug.com/628416): The error from step 11
  // (NotSupportedError) is thrown after step 12 which does not match
  // the spec order.

  // 14.3.11.12: If the [[extractable]] internal slot of key is false, then
  //             throw an InvalidAccessError.
  if (!key->extractable()) {
    result->completeWithError(WebCryptoErrorTypeInvalidAccess,
                              "key is not extractable");
    return promise;
  }

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, wrappingKey->key());
  histogramKey(scriptState->getExecutionContext(), key->key());
  Platform::current()->crypto()->wrapKey(format, key->key(), wrappingKey->key(),
                                         normalizedAlgorithm, result->result());
  return promise;
}

ScriptPromise SubtleCrypto::unwrapKey(
    ScriptState* scriptState,
    const String& rawFormat,
    const BufferSource& rawWrappedKey,
    CryptoKey* unwrappingKey,
    const AlgorithmIdentifier& rawUnwrapAlgorithm,
    const AlgorithmIdentifier& rawUnwrappedKeyAlgorithm,
    bool extractable,
    const Vector<String>& rawKeyUsages) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-unwrapKey

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
  WebVector<uint8_t> wrappedKey = copyBytes(rawWrappedKey);

  // 14.3.12.3: Let normalizedAlgorithm be the result of normalizing an
  //            algorithm, with alg set to algorithm and op set to
  //            "unwrapKey".
  //
  // 14.3.12.4: If an error occurred, let normalizedAlgorithm be the result
  //            of normalizing an algorithm, with alg set to algorithm and op
  //            set to "decrypt".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawUnwrapAlgorithm, WebCryptoOperationUnwrapKey,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.12.6: Let normalizedKeyAlgorithm be the result of normalizing an
  //            algorithm, with alg set to unwrappedKeyAlgorithm and op set
  //            to "importKey".
  WebCryptoAlgorithm normalizedKeyAlgorithm;
  if (!parseAlgorithm(rawUnwrappedKeyAlgorithm, WebCryptoOperationImportKey,
                      normalizedKeyAlgorithm, result))
    return promise;

  // 14.3.12.11: If the name member of normalizedAlgorithm is not equal to
  //             the name attribute of the [[algorithm]] internal slot of
  //             unwrappingKey then throw an InvalidAccessError.
  //
  // 14.3.12.12: If the [[usages]] internal slot of unwrappingKey does not
  //             contain an entry that is "unwrapKey", then throw an
  //             InvalidAccessError.
  if (!unwrappingKey->canBeUsedForAlgorithm(normalizedAlgorithm,
                                            WebCryptoKeyUsageUnwrapKey, result))
    return promise;

  // NOTE: Step (16) disallows empty usages on secret and private keys. This
  // normative requirement is enforced by the platform implementation in the
  // call below.

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, unwrappingKey->key());
  histogramAlgorithm(scriptState->getExecutionContext(),
                     normalizedKeyAlgorithm);
  Platform::current()->crypto()->unwrapKey(
      format, std::move(wrappedKey), unwrappingKey->key(), normalizedAlgorithm,
      normalizedKeyAlgorithm, extractable, keyUsages, result->result());
  return promise;
}

ScriptPromise SubtleCrypto::deriveBits(ScriptState* scriptState,
                                       const AlgorithmIdentifier& rawAlgorithm,
                                       CryptoKey* baseKey,
                                       unsigned lengthBits) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-deriveBits

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  // 14.3.8.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "deriveBits".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDeriveBits,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.8.7: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of baseKey
  //           then throw an InvalidAccessError.
  //
  // 14.3.8.8: If the [[usages]] internal slot of baseKey does not contain an
  //           entry that is "deriveBits", then throw an InvalidAccessError.
  if (!baseKey->canBeUsedForAlgorithm(normalizedAlgorithm,
                                      WebCryptoKeyUsageDeriveBits, result))
    return promise;

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, baseKey->key());
  Platform::current()->crypto()->deriveBits(normalizedAlgorithm, baseKey->key(),
                                            lengthBits, result->result());
  return promise;
}

ScriptPromise SubtleCrypto::deriveKey(
    ScriptState* scriptState,
    const AlgorithmIdentifier& rawAlgorithm,
    CryptoKey* baseKey,
    const AlgorithmIdentifier& rawDerivedKeyType,
    bool extractable,
    const Vector<String>& rawKeyUsages) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-deriveKey

  CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
  ScriptPromise promise = result->promise();

  if (!canAccessWebCrypto(scriptState, result))
    return promise;

  WebCryptoKeyUsageMask keyUsages;
  if (!CryptoKey::parseUsageMask(rawKeyUsages, keyUsages, result))
    return promise;

  // 14.3.7.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "deriveBits".
  WebCryptoAlgorithm normalizedAlgorithm;
  if (!parseAlgorithm(rawAlgorithm, WebCryptoOperationDeriveBits,
                      normalizedAlgorithm, result))
    return promise;

  // 14.3.7.4: Let normalizedDerivedKeyAlgorithm be the result of normalizing
  //           an algorithm, with alg set to derivedKeyType and op set to
  //           "importKey".
  WebCryptoAlgorithm normalizedDerivedKeyAlgorithm;
  if (!parseAlgorithm(rawDerivedKeyType, WebCryptoOperationImportKey,
                      normalizedDerivedKeyAlgorithm, result))
    return promise;

  // TODO(eroman): The description in the spec needs to be updated as
  // it doesn't describe algorithm normalization for the Get Key
  // Length parameters (https://github.com/w3c/webcrypto/issues/127)
  // For now reference step 10 which is the closest.
  //
  // 14.3.7.10: If the name member of normalizedDerivedKeyAlgorithm does not
  //            identify a registered algorithm that supports the get key length
  //            operation, then throw a NotSupportedError.
  WebCryptoAlgorithm keyLengthAlgorithm;
  if (!parseAlgorithm(rawDerivedKeyType, WebCryptoOperationGetKeyLength,
                      keyLengthAlgorithm, result))
    return promise;

  // 14.3.7.11: If the name member of normalizedAlgorithm is not equal to the
  //            name attribute of the [[algorithm]] internal slot of baseKey
  //            then throw an InvalidAccessError.
  //
  // 14.3.7.12: If the [[usages]] internal slot of baseKey does not contain
  //            an entry that is "deriveKey", then throw an InvalidAccessError.
  if (!baseKey->canBeUsedForAlgorithm(normalizedAlgorithm,
                                      WebCryptoKeyUsageDeriveKey, result))
    return promise;

  // NOTE: Step (16) disallows empty usages on secret and private keys. This
  // normative requirement is enforced by the platform implementation in the
  // call below.

  histogramAlgorithmAndKey(scriptState->getExecutionContext(),
                           normalizedAlgorithm, baseKey->key());
  histogramAlgorithm(scriptState->getExecutionContext(),
                     normalizedDerivedKeyAlgorithm);
  Platform::current()->crypto()->deriveKey(
      normalizedAlgorithm, baseKey->key(), normalizedDerivedKeyAlgorithm,
      keyLengthAlgorithm, extractable, keyUsages, result->result());
  return promise;
}

}  // namespace blink
