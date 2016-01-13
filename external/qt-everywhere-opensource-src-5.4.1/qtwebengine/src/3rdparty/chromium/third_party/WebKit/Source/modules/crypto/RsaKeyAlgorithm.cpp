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
#include "modules/crypto/RsaKeyAlgorithm.h"

#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/WebVector.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

RsaKeyAlgorithm::~RsaKeyAlgorithm()
{
}

RsaKeyAlgorithm* RsaKeyAlgorithm::create(const blink::WebCryptoKeyAlgorithm& algorithm)
{
    return new RsaKeyAlgorithm(algorithm);
}

unsigned RsaKeyAlgorithm::modulusLength()
{
    return m_algorithm.rsaHashedParams()->modulusLengthBits();
}

Uint8Array* RsaKeyAlgorithm::publicExponent()
{
    if (!m_publicExponent.get()) {
        const blink::WebVector<unsigned char>& exponent = m_algorithm.rsaHashedParams()->publicExponent();
        m_publicExponent = Uint8Array::create(exponent.data(), exponent.size());
    }
    return m_publicExponent.get();
}

void RsaKeyAlgorithm::trace(Visitor* visitor)
{
    KeyAlgorithm::trace(visitor);
}

RsaKeyAlgorithm::RsaKeyAlgorithm(const blink::WebCryptoKeyAlgorithm& algorithm)
    : KeyAlgorithm(algorithm)
{
    ASSERT(algorithm.rsaHashedParams());
    ScriptWrappable::init(this);
}

} // namespace WebCore
