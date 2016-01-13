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
#include "modules/crypto/NormalizeAlgorithm.h"

#include "bindings/v8/Dictionary.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebString.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace WebCore {

namespace {

struct AlgorithmNameMapping {
    // Must be an upper case ASCII string.
    const char* const algorithmName;
    // Must be strlen(algorithmName).
    unsigned char algorithmNameLength;
    blink::WebCryptoAlgorithmId algorithmId;

#if ASSERT_ENABLED
    bool operator<(const AlgorithmNameMapping&) const;
#endif
};

// Must be sorted by length, and then by reverse string.
// Also all names must be upper case ASCII.
const AlgorithmNameMapping algorithmNameMappings[] = {
    {"HMAC", 4, blink::WebCryptoAlgorithmIdHmac},
    {"SHA-1", 5, blink::WebCryptoAlgorithmIdSha1},
    {"AES-KW", 6, blink::WebCryptoAlgorithmIdAesKw},
    {"SHA-512", 7, blink::WebCryptoAlgorithmIdSha512},
    {"SHA-384", 7, blink::WebCryptoAlgorithmIdSha384},
    {"SHA-256", 7, blink::WebCryptoAlgorithmIdSha256},
    {"AES-CBC", 7, blink::WebCryptoAlgorithmIdAesCbc},
    {"AES-GCM", 7, blink::WebCryptoAlgorithmIdAesGcm},
    {"AES-CTR", 7, blink::WebCryptoAlgorithmIdAesCtr},
    {"RSA-OAEP", 8, blink::WebCryptoAlgorithmIdRsaOaep},
    {"RSASSA-PKCS1-V1_5", 17, blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5},
};

#if ASSERT_ENABLED

// Essentially std::is_sorted() (however that function is new to C++11).
template <typename Iterator>
bool isSorted(Iterator begin, Iterator end)
{
    if (begin == end)
        return true;

    Iterator prev = begin;
    Iterator cur = begin + 1;

    while (cur != end) {
        if (*cur < *prev)
            return false;
        cur++;
        prev++;
    }

    return true;
}

bool AlgorithmNameMapping::operator<(const AlgorithmNameMapping& o) const
{
    if (algorithmNameLength < o.algorithmNameLength)
        return true;
    if (algorithmNameLength > o.algorithmNameLength)
        return false;

    for (size_t i = 0; i < algorithmNameLength; ++i) {
        size_t reverseIndex = algorithmNameLength - i - 1;
        char c1 = algorithmName[reverseIndex];
        char c2 = o.algorithmName[reverseIndex];

        if (c1 < c2)
            return true;
        if (c1 > c2)
            return false;
    }

    return false;
}

bool verifyAlgorithmNameMappings(const AlgorithmNameMapping* begin, const AlgorithmNameMapping* end)
{
    for (const AlgorithmNameMapping* it = begin; it != end; ++it) {
        if (it->algorithmNameLength != strlen(it->algorithmName))
            return false;
        String str(it->algorithmName, it->algorithmNameLength);
        if (!str.containsOnlyASCII())
            return false;
        if (str.upper() != str)
            return false;
    }

    return isSorted(begin, end);
}
#endif

template <typename CharType>
bool algorithmNameComparator(const AlgorithmNameMapping& a, StringImpl* b)
{
    if (a.algorithmNameLength < b->length())
        return true;
    if (a.algorithmNameLength > b->length())
        return false;

    // Because the algorithm names contain many common prefixes, it is better
    // to compare starting at the end of the string.
    for (size_t i = 0; i < a.algorithmNameLength; ++i) {
        size_t reverseIndex = a.algorithmNameLength - i - 1;
        CharType c1 = a.algorithmName[reverseIndex];
        CharType c2 = b->getCharacters<CharType>()[reverseIndex];
        if (!isASCII(c2))
            return false;
        c2 = toASCIIUpper(c2);

        if (c1 < c2)
            return true;
        if (c1 > c2)
            return false;
    }

    return false;
}

bool lookupAlgorithmIdByName(const String& algorithmName, blink::WebCryptoAlgorithmId& id)
{
    const AlgorithmNameMapping* begin = algorithmNameMappings;
    const AlgorithmNameMapping* end = algorithmNameMappings + WTF_ARRAY_LENGTH(algorithmNameMappings);

    ASSERT(verifyAlgorithmNameMappings(begin, end));

    const AlgorithmNameMapping* it;
    if (algorithmName.impl()->is8Bit())
        it = std::lower_bound(begin, end, algorithmName.impl(), &algorithmNameComparator<LChar>);
    else
        it = std::lower_bound(begin, end, algorithmName.impl(), &algorithmNameComparator<UChar>);

    if (it == end)
        return false;

    if (it->algorithmNameLength != algorithmName.length() || !equalIgnoringCase(algorithmName, it->algorithmName))
        return false;

    id = it->algorithmId;
    return true;
}

void setSyntaxError(const String& message, AlgorithmError* error)
{
    error->errorType = blink::WebCryptoErrorTypeSyntax;
    error->errorDetails = message;
}

void setNotSupportedError(const String& message, AlgorithmError* error)
{
    error->errorType = blink::WebCryptoErrorTypeNotSupported;
    error->errorDetails = message;
}

void setDataError(const String& message, AlgorithmError* error)
{
    error->errorType = blink::WebCryptoErrorTypeData;
    error->errorDetails = message;
}

// ErrorContext holds a stack of string literals which describe what was
// happening at the time the error occurred. This is helpful because
// parsing of the algorithm dictionary can be recursive and it is difficult to
// tell what went wrong from a failure alone.
class ErrorContext {
public:
    void add(const char* message)
    {
        m_messages.append(message);
    }

    void removeLast()
    {
        m_messages.removeLast();
    }

    // Join all of the string literals into a single String.
    String toString() const
    {
        if (m_messages.isEmpty())
            return String();

        StringBuilder result;
        const char* Separator = ": ";

        size_t length = (m_messages.size() - 1) * strlen(Separator);
        for (size_t i = 0; i < m_messages.size(); ++i)
            length += strlen(m_messages[i]);
        result.reserveCapacity(length);

        for (size_t i = 0; i < m_messages.size(); ++i) {
            if (i)
                result.append(Separator, strlen(Separator));
            result.append(m_messages[i], strlen(m_messages[i]));
        }

        return result.toString();
    }

    String toString(const char* message) const
    {
        ErrorContext stack(*this);
        stack.add(message);
        return stack.toString();
    }

    String toString(const char* message1, const char* message2) const
    {
        ErrorContext stack(*this);
        stack.add(message1);
        stack.add(message2);
        return stack.toString();
    }

private:
    // This inline size is large enough to avoid having to grow the Vector in
    // the majority of cases (up to 1 nested algorithm identifier).
    Vector<const char*, 10> m_messages;
};

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
//
// FIXME: Currently only supports ArrayBufferView.
bool getOptionalCryptoOperationData(const Dictionary& raw, const char* propertyName, bool& hasProperty, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, AlgorithmError* error)
{
    if (!raw.get(propertyName, buffer)) {
        hasProperty = false;
        return true;
    }

    hasProperty = true;

    if (!buffer) {
        setSyntaxError(context.toString(propertyName, "Not an ArrayBufferView"), error);
        return false;
    }

    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
//
// FIXME: Currently only supports ArrayBufferView.
bool getCryptoOperationData(const Dictionary& raw, const char* propertyName, RefPtr<ArrayBufferView>& buffer, const ErrorContext& context, AlgorithmError* error)
{
    bool hasProperty;
    bool ok = getOptionalCryptoOperationData(raw, propertyName, hasProperty, buffer, context, error);
    if (!hasProperty) {
        setSyntaxError(context.toString(propertyName, "Missing required property"), error);
        return false;
    }
    return ok;
}

bool getUint8Array(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, AlgorithmError* error)
{
    if (!raw.get(propertyName, array) || !array) {
        setSyntaxError(context.toString(propertyName, "Missing or not a Uint8Array"), error);
        return false;
    }
    return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef Uint8Array BigInteger;
bool getBigInteger(const Dictionary& raw, const char* propertyName, RefPtr<Uint8Array>& array, const ErrorContext& context, AlgorithmError* error)
{
    if (!getUint8Array(raw, propertyName, array, context, error))
        return false;

    if (!array->byteLength()) {
        setSyntaxError(context.toString(propertyName, "BigInteger should not be empty"), error);
        return false;
    }

    if (!raw.get(propertyName, array) || !array) {
        setSyntaxError(context.toString(propertyName, "Missing or not a Uint8Array"), error);
        return false;
    }
    return true;
}

// Gets an integer according to WebIDL's [EnforceRange].
bool getOptionalInteger(const Dictionary& raw, const char* propertyName, bool& hasProperty, double& value, double minValue, double maxValue, const ErrorContext& context, AlgorithmError* error)
{
    double number;
    bool ok = raw.get(propertyName, number, hasProperty);

    if (!hasProperty)
        return true;

    if (!ok || std::isnan(number)) {
        setSyntaxError(context.toString(propertyName, "Is not a number"), error);
        return false;
    }

    number = trunc(number);

    if (std::isinf(number) || number < minValue || number > maxValue) {
        setSyntaxError(context.toString(propertyName, "Outside of numeric range"), error);
        return false;
    }

    value = number;
    return true;
}

bool getInteger(const Dictionary& raw, const char* propertyName, double& value, double minValue, double maxValue, const ErrorContext& context, AlgorithmError* error)
{
    bool hasProperty;
    if (!getOptionalInteger(raw, propertyName, hasProperty, value, minValue, maxValue, context, error))
        return false;

    if (!hasProperty) {
        setSyntaxError(context.toString(propertyName, "Missing required property"), error);
        return false;
    }

    return true;
}

bool getUint32(const Dictionary& raw, const char* propertyName, uint32_t& value, const ErrorContext& context, AlgorithmError* error)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFFFFFF, context, error))
        return false;
    value = number;
    return true;
}

bool getUint16(const Dictionary& raw, const char* propertyName, uint16_t& value, const ErrorContext& context, AlgorithmError* error)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFFFF, context, error))
        return false;
    value = number;
    return true;
}

bool getUint8(const Dictionary& raw, const char* propertyName, uint8_t& value, const ErrorContext& context, AlgorithmError* error)
{
    double number;
    if (!getInteger(raw, propertyName, number, 0, 0xFF, context, error))
        return false;
    value = number;
    return true;
}

bool getOptionalUint32(const Dictionary& raw, const char* propertyName, bool& hasValue, uint32_t& value, const ErrorContext& context, AlgorithmError* error)
{
    double number;
    if (!getOptionalInteger(raw, propertyName, hasValue, number, 0, 0xFFFFFFFF, context, error))
        return false;
    if (hasValue)
        value = number;
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCbcParams : Algorithm {
//      CryptoOperationData iv;
//    };
bool parseAesCbcParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, error))
        return false;

    if (iv->byteLength() != 16) {
        setDataError(context.toString("iv", "Must be 16 bytes"), error);
        return false;
    }

    params = adoptPtr(new blink::WebCryptoAesCbcParams(static_cast<unsigned char*>(iv->baseAddress()), iv->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesKeyGenParams : Algorithm {
//      [EnforceRange] unsigned short length;
//    };
bool parseAesKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    uint16_t length;
    if (!getUint16(raw, "length", length, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoAesKeyGenParams(length));
    return true;
}

bool parseAlgorithm(const Dictionary&, blink::WebCryptoOperation, blink::WebCryptoAlgorithm&, ErrorContext, AlgorithmError*);

bool parseHash(const Dictionary& raw, blink::WebCryptoAlgorithm& hash, ErrorContext context, AlgorithmError* error)
{
    Dictionary rawHash;
    if (!raw.get("hash", rawHash)) {
        setSyntaxError(context.toString("hash", "Missing or not a dictionary"), error);
        return false;
    }

    context.add("hash");
    return parseAlgorithm(rawHash, blink::WebCryptoOperationDigest, hash, context, error);
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacImportParams : Algorithm {
//      AlgorithmIdentifier hash;
//    };
bool parseHmacImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoHmacImportParams(hash));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacKeyGenParams : Algorithm {
//      AlgorithmIdentifier hash;
//      // The length (in bits) of the key to generate. If unspecified, the
//      // recommended length will be used, which is the size of the associated hash function's block
//      // size.
//      unsigned long length;
//    };
bool parseHmacKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, error))
        return false;

    bool hasLength;
    uint32_t length = 0;
    if (!getOptionalUint32(raw, "length", hasLength, length, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoHmacKeyGenParams(hash, hasLength, length));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedImportParams {
//      AlgorithmIdentifier hash;
//    };
bool parseRsaHashedImportParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaHashedImportParams(hash));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
//      AlgorithmIdentifier hash;
//    };
//
//    dictionary RsaKeyGenParams : Algorithm {
//      unsigned long modulusLength;
//      BigInteger publicExponent;
//    };
bool parseRsaHashedKeyGenParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    uint32_t modulusLength;
    if (!getUint32(raw, "modulusLength", modulusLength, context, error))
        return false;

    RefPtr<Uint8Array> publicExponent;
    if (!getBigInteger(raw, "publicExponent", publicExponent, context, error))
        return false;

    blink::WebCryptoAlgorithm hash;
    if (!parseHash(raw, hash, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoRsaHashedKeyGenParams(hash, modulusLength, static_cast<const unsigned char*>(publicExponent->baseAddress()), publicExponent->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCtrParams : Algorithm {
//      CryptoOperationData counter;
//      [EnforceRange] octet length;
//    };
bool parseAesCtrParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    RefPtr<ArrayBufferView> counter;
    if (!getCryptoOperationData(raw, "counter", counter, context, error))
        return false;

    uint8_t length;
    if (!getUint8(raw, "length", length, context, error))
        return false;

    params = adoptPtr(new blink::WebCryptoAesCtrParams(length, static_cast<const unsigned char*>(counter->baseAddress()), counter->byteLength()));
    return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary AesGcmParams : Algorithm {
//       CryptoOperationData iv;
//       CryptoOperationData? additionalData;
//       [EnforceRange] octet? tagLength;  // May be 0-128
//     }
bool parseAesGcmParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    RefPtr<ArrayBufferView> iv;
    if (!getCryptoOperationData(raw, "iv", iv, context, error))
        return false;

    bool hasAdditionalData;
    RefPtr<ArrayBufferView> additionalData;
    if (!getOptionalCryptoOperationData(raw, "additionalData", hasAdditionalData, additionalData, context, error))
        return false;

    double tagLength;
    bool hasTagLength;
    if (!getOptionalInteger(raw, "tagLength", hasTagLength, tagLength, 0, 128, context, error))
        return false;

    const unsigned char* ivStart = static_cast<const unsigned char*>(iv->baseAddress());
    unsigned ivLength = iv->byteLength();

    const unsigned char* additionalDataStart = hasAdditionalData ? static_cast<const unsigned char*>(additionalData->baseAddress()) : 0;
    unsigned additionalDataLength = hasAdditionalData ? additionalData->byteLength() : 0;

    params = adoptPtr(new blink::WebCryptoAesGcmParams(ivStart, ivLength, hasAdditionalData, additionalDataStart, additionalDataLength, hasTagLength, tagLength));
    return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary RsaOaepParams : Algorithm {
//       CryptoOperationData? label;
//     };
bool parseRsaOaepParams(const Dictionary& raw, OwnPtr<blink::WebCryptoAlgorithmParams>& params, const ErrorContext& context, AlgorithmError* error)
{
    bool hasLabel;
    RefPtr<ArrayBufferView> label;
    if (!getOptionalCryptoOperationData(raw, "label", hasLabel, label, context, error))
        return false;

    const unsigned char* labelStart = hasLabel ? static_cast<const unsigned char*>(label->baseAddress()) : 0;
    unsigned labelLength = hasLabel ? label->byteLength() : 0;

    params = adoptPtr(new blink::WebCryptoRsaOaepParams(hasLabel, labelStart, labelLength));
    return true;
}

bool parseAlgorithmParams(const Dictionary& raw, blink::WebCryptoAlgorithmParamsType type, OwnPtr<blink::WebCryptoAlgorithmParams>& params, ErrorContext& context, AlgorithmError* error)
{
    switch (type) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
        return true;
    case blink::WebCryptoAlgorithmParamsTypeAesCbcParams:
        context.add("AesCbcParams");
        return parseAesCbcParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        context.add("AesKeyGenParams");
        return parseAesKeyGenParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
        context.add("HmacImportParams");
        return parseHmacImportParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams:
        context.add("HmacKeyGenParams");
        return parseHmacKeyGenParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
        context.add("RsaHashedKeyGenParams");
        return parseRsaHashedKeyGenParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
        context.add("RsaHashedImportParams");
        return parseRsaHashedImportParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeAesCtrParams:
        context.add("AesCtrParams");
        return parseAesCtrParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeAesGcmParams:
        context.add("AesGcmParams");
        return parseAesGcmParams(raw, params, context, error);
    case blink::WebCryptoAlgorithmParamsTypeRsaOaepParams:
        context.add("RsaOaepParams");
        return parseRsaOaepParams(raw, params, context, error);
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

const char* operationToString(blink::WebCryptoOperation op)
{
    switch (op) {
    case blink::WebCryptoOperationEncrypt:
        return "encrypt";
    case blink::WebCryptoOperationDecrypt:
        return "decrypt";
    case blink::WebCryptoOperationSign:
        return "sign";
    case blink::WebCryptoOperationVerify:
        return "verify";
    case blink::WebCryptoOperationDigest:
        return "digest";
    case blink::WebCryptoOperationGenerateKey:
        return "generateKey";
    case blink::WebCryptoOperationImportKey:
        return "importKey";
    case blink::WebCryptoOperationDeriveKey:
        return "deriveKey";
    case blink::WebCryptoOperationDeriveBits:
        return "deriveBits";
    case blink::WebCryptoOperationWrapKey:
        return "wrapKey";
    case blink::WebCryptoOperationUnwrapKey:
        return "unwrapKey";
    }
    return 0;
}

bool parseAlgorithm(const Dictionary& raw, blink::WebCryptoOperation op, blink::WebCryptoAlgorithm& algorithm, ErrorContext context, AlgorithmError* error)
{
    context.add("Algorithm");

    if (!raw.isObject()) {
        setSyntaxError(context.toString("Not an object"), error);
        return false;
    }

    String algorithmName;
    if (!raw.get("name", algorithmName)) {
        setSyntaxError(context.toString("name", "Missing or not a string"), error);
        return false;
    }

    blink::WebCryptoAlgorithmId algorithmId;
    if (!lookupAlgorithmIdByName(algorithmName, algorithmId)) {
        // FIXME: The spec says to return a SyntaxError if the input contains
        //        any non-ASCII characters.
        setNotSupportedError(context.toString("Unrecognized name"), error);
        return false;
    }

    // Remove the "Algorithm:" prefix for all subsequent errors.
    context.removeLast();

    const blink::WebCryptoAlgorithmInfo* algorithmInfo = blink::WebCryptoAlgorithm::lookupAlgorithmInfo(algorithmId);

    if (algorithmInfo->operationToParamsType[op] == blink::WebCryptoAlgorithmInfo::Undefined) {
        context.add(algorithmInfo->name);
        setNotSupportedError(context.toString("Unsupported operation", operationToString(op)), error);
        return false;
    }

    blink::WebCryptoAlgorithmParamsType paramsType = static_cast<blink::WebCryptoAlgorithmParamsType>(algorithmInfo->operationToParamsType[op]);

    OwnPtr<blink::WebCryptoAlgorithmParams> params;
    if (!parseAlgorithmParams(raw, paramsType, params, context, error))
        return false;

    algorithm = blink::WebCryptoAlgorithm(algorithmId, params.release());
    return true;
}

} // namespace

bool normalizeAlgorithm(const Dictionary& raw, blink::WebCryptoOperation op, blink::WebCryptoAlgorithm& algorithm, AlgorithmError* error)
{
    return parseAlgorithm(raw, op, algorithm, ErrorContext(), error);
}

} // namespace WebCore
