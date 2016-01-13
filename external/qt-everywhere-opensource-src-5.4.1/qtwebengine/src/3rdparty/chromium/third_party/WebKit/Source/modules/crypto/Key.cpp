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
#include "modules/crypto/Key.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/crypto/KeyAlgorithm.h"
#include "platform/CryptoResult.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebString.h"

namespace WebCore {

namespace {

const char* keyTypeToString(blink::WebCryptoKeyType type)
{
    switch (type) {
    case blink::WebCryptoKeyTypeSecret:
        return "secret";
    case blink::WebCryptoKeyTypePublic:
        return "public";
    case blink::WebCryptoKeyTypePrivate:
        return "private";
    }
    ASSERT_NOT_REACHED();
    return 0;
}

struct KeyUsageMapping {
    blink::WebCryptoKeyUsage value;
    const char* const name;
};

// The order of this array is the same order that will appear in Key.usages. It
// must be kept ordered as described by the Web Crypto spec.
const KeyUsageMapping keyUsageMappings[] = {
    { blink::WebCryptoKeyUsageEncrypt, "encrypt" },
    { blink::WebCryptoKeyUsageDecrypt, "decrypt" },
    { blink::WebCryptoKeyUsageSign, "sign" },
    { blink::WebCryptoKeyUsageVerify, "verify" },
    { blink::WebCryptoKeyUsageDeriveKey, "deriveKey" },
    { blink::WebCryptoKeyUsageDeriveBits, "deriveBits" },
    { blink::WebCryptoKeyUsageWrapKey, "wrapKey" },
    { blink::WebCryptoKeyUsageUnwrapKey, "unwrapKey" },
};

COMPILE_ASSERT(blink::EndOfWebCryptoKeyUsage == (1 << 7) + 1, update_keyUsageMappings);

const char* keyUsageToString(blink::WebCryptoKeyUsage usage)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].value == usage)
            return keyUsageMappings[i].name;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

blink::WebCryptoKeyUsageMask keyUsageStringToMask(const String& usageString)
{
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        if (keyUsageMappings[i].name == usageString)
            return keyUsageMappings[i].value;
    }
    return 0;
}

blink::WebCryptoKeyUsageMask toKeyUsage(blink::WebCryptoOperation operation)
{
    switch (operation) {
    case blink::WebCryptoOperationEncrypt:
        return blink::WebCryptoKeyUsageEncrypt;
    case blink::WebCryptoOperationDecrypt:
        return blink::WebCryptoKeyUsageDecrypt;
    case blink::WebCryptoOperationSign:
        return blink::WebCryptoKeyUsageSign;
    case blink::WebCryptoOperationVerify:
        return blink::WebCryptoKeyUsageVerify;
    case blink::WebCryptoOperationDeriveKey:
        return blink::WebCryptoKeyUsageDeriveKey;
    case blink::WebCryptoOperationDeriveBits:
        return blink::WebCryptoKeyUsageDeriveBits;
    case blink::WebCryptoOperationWrapKey:
        return blink::WebCryptoKeyUsageWrapKey;
    case blink::WebCryptoOperationUnwrapKey:
        return blink::WebCryptoKeyUsageUnwrapKey;
    case blink::WebCryptoOperationDigest:
    case blink::WebCryptoOperationGenerateKey:
    case blink::WebCryptoOperationImportKey:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace

Key::~Key()
{
}

Key::Key(const blink::WebCryptoKey& key)
    : m_key(key)
{
    ScriptWrappable::init(this);
}

String Key::type() const
{
    return keyTypeToString(m_key.type());
}

bool Key::extractable() const
{
    return m_key.extractable();
}

KeyAlgorithm* Key::algorithm()
{
    if (!m_algorithm)
        m_algorithm = KeyAlgorithm::create(m_key.algorithm());
    return m_algorithm.get();
}

// FIXME: This creates a new javascript array each time. What should happen
//        instead is return the same (immutable) array. (Javascript callers can
//        distinguish this by doing an == test on the arrays and seeing they are
//        different).
Vector<String> Key::usages() const
{
    Vector<String> result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(keyUsageMappings); ++i) {
        blink::WebCryptoKeyUsage usage = keyUsageMappings[i].value;
        if (m_key.usages() & usage)
            result.append(keyUsageToString(usage));
    }
    return result;
}

bool Key::canBeUsedForAlgorithm(const blink::WebCryptoAlgorithm& algorithm, blink::WebCryptoOperation op, CryptoResult* result) const
{
    if (!(m_key.usages() & toKeyUsage(op))) {
        result->completeWithError(blink::WebCryptoErrorTypeInvalidAccess, "key.usages does not permit this operation");
        return false;
    }

    if (m_key.algorithm().id() != algorithm.id()) {
        result->completeWithError(blink::WebCryptoErrorTypeInvalidAccess, "key.algorithm does not match that of operation");
        return false;
    }

    return true;
}

bool Key::parseFormat(const String& formatString, blink::WebCryptoKeyFormat& format, CryptoResult* result)
{
    // There are few enough values that testing serially is fast enough.
    if (formatString == "raw") {
        format = blink::WebCryptoKeyFormatRaw;
        return true;
    }
    if (formatString == "pkcs8") {
        format = blink::WebCryptoKeyFormatPkcs8;
        return true;
    }
    if (formatString == "spki") {
        format = blink::WebCryptoKeyFormatSpki;
        return true;
    }
    if (formatString == "jwk") {
        format = blink::WebCryptoKeyFormatJwk;
        return true;
    }

    result->completeWithError(blink::WebCryptoErrorTypeSyntax, "Invalid keyFormat argument");
    return false;
}

bool Key::parseUsageMask(const Vector<String>& usages, blink::WebCryptoKeyUsageMask& mask, CryptoResult* result)
{
    mask = 0;
    for (size_t i = 0; i < usages.size(); ++i) {
        blink::WebCryptoKeyUsageMask usage = keyUsageStringToMask(usages[i]);
        if (!usage) {
            result->completeWithError(blink::WebCryptoErrorTypeSyntax, "Invalid keyUsages argument");
            return false;
        }
        mask |= usage;
    }
    return true;
}

void Key::trace(Visitor* visitor)
{
    visitor->trace(m_algorithm);
}

} // namespace WebCore
