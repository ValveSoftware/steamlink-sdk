/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "modules/crypto/KeyAlgorithm.h"

#include "modules/crypto/AesKeyAlgorithm.h"
#include "modules/crypto/HmacKeyAlgorithm.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "modules/crypto/RsaHashedKeyAlgorithm.h"
#include "modules/crypto/RsaKeyAlgorithm.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

KeyAlgorithm::~KeyAlgorithm()
{
}

KeyAlgorithm* KeyAlgorithm::create(const blink::WebCryptoKeyAlgorithm& algorithm)
{
    switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeNone:
        return new KeyAlgorithm(algorithm);
    case blink::WebCryptoKeyAlgorithmParamsTypeAes:
        return AesKeyAlgorithm::create(algorithm);
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac:
        return HmacKeyAlgorithm::create(algorithm);
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
        return RsaHashedKeyAlgorithm::create(algorithm);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

KeyAlgorithm* KeyAlgorithm::createHash(const blink::WebCryptoAlgorithm& hash)
{
    // Assume that none of the hash algorithms have any parameters.
    ASSERT(hash.paramsType() == blink::WebCryptoAlgorithmParamsTypeNone);
    return new KeyAlgorithm(blink::WebCryptoKeyAlgorithm(hash.id(), nullptr));
}

KeyAlgorithm::KeyAlgorithm(const blink::WebCryptoKeyAlgorithm& algorithm)
    : m_algorithm(algorithm)
{
    ScriptWrappable::init(this);
}

String KeyAlgorithm::name()
{
    const blink::WebCryptoAlgorithmInfo* info = blink::WebCryptoAlgorithm::lookupAlgorithmInfo(m_algorithm.id());
    return info->name;
}

bool KeyAlgorithm::isAesKeyAlgorithm() const
{
    return m_algorithm.paramsType() == blink::WebCryptoKeyAlgorithmParamsTypeAes;
}

bool KeyAlgorithm::isHmacKeyAlgorithm() const
{
    return m_algorithm.paramsType() == blink::WebCryptoKeyAlgorithmParamsTypeHmac;
}

bool KeyAlgorithm::isRsaHashedKeyAlgorithm() const
{
    return m_algorithm.paramsType() == blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed;
}

void KeyAlgorithm::trace(Visitor*)
{
}

} // namespace WebCore
