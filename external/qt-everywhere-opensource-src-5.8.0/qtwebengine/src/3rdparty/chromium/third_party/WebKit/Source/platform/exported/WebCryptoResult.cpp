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

#include "public/platform/WebCrypto.h"

#include "platform/CryptoResult.h"
#include "platform/heap/Handle.h"

namespace blink {

void WebCryptoResult::completeWithError(WebCryptoErrorType errorType, const WebString& errorDetails)
{
    if (!cancelled())
        m_impl->completeWithError(errorType, errorDetails);
    reset();
}

void WebCryptoResult::completeWithBuffer(const void* bytes, unsigned bytesSize)
{
    if (!cancelled())
        m_impl->completeWithBuffer(bytes, bytesSize);
    reset();
}

void WebCryptoResult::completeWithJson(const char* utf8Data, unsigned length)
{
    if (!cancelled())
        m_impl->completeWithJson(utf8Data, length);
    reset();
}

void WebCryptoResult::completeWithBoolean(bool b)
{
    if (!cancelled())
        m_impl->completeWithBoolean(b);
    reset();
}

void WebCryptoResult::completeWithKey(const WebCryptoKey& key)
{
    ASSERT(!key.isNull());
    if (!cancelled())
        m_impl->completeWithKey(key);
    reset();
}

void WebCryptoResult::completeWithKeyPair(const WebCryptoKey& publicKey, const WebCryptoKey& privateKey)
{
    ASSERT(!publicKey.isNull());
    ASSERT(!privateKey.isNull());
    if (!cancelled())
        m_impl->completeWithKeyPair(publicKey, privateKey);
    reset();
}

bool WebCryptoResult::cancelled() const
{
    return m_cancel->cancelled();
}

WebCryptoResult::WebCryptoResult(CryptoResult* impl, const PassRefPtr<CryptoResultCancel>& cancel)
    : m_impl(impl)
    , m_cancel(cancel)
{
    ASSERT(m_impl.get());
    ASSERT(m_cancel.get());
}

void WebCryptoResult::reset()
{
    m_impl.reset();
    m_cancel.reset();
}

void WebCryptoResult::assign(const WebCryptoResult& o)
{
    m_impl = o.m_impl;
    m_cancel = o.m_cancel;
}

} // namespace blink
