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

#ifndef CryptoResultImpl_h
#define CryptoResultImpl_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ExceptionCode.h"
#include "modules/ModulesExport.h"
#include "platform/CryptoResult.h"
#include "public/platform/WebCrypto.h"
#include "wtf/Forward.h"

namespace blink {

MODULES_EXPORT ExceptionCode webCryptoErrorToExceptionCode(WebCryptoErrorType);

// Wrapper around a Promise to notify completion of the crypto operation.
//
// The thread on which CryptoResultImpl was created on is referred to as the
// "origin thread".
//
//  * At creation time there must be an active ExecutionContext.
//  * All methods of the CryptoResult implementation must be called from
//    the origin thread. The exception is that destruction may happen on
//    another thread.
//  * One of the completeWith***() functions must be called, or the
//    m_resolver will be leaked until the ExecutionContext is destroyed.
class CryptoResultImpl final : public CryptoResult {
public:
    static CryptoResultImpl* create(ScriptState*);

    ~CryptoResultImpl();

    void completeWithError(WebCryptoErrorType, const WebString&) override;
    void completeWithBuffer(const void* bytes, unsigned bytesSize) override;
    void completeWithJson(const char* utf8Data, unsigned length) override;
    void completeWithBoolean(bool) override;
    void completeWithKey(const WebCryptoKey&) override;
    void completeWithKeyPair(const WebCryptoKey& publicKey, const WebCryptoKey& privateKey) override;

    // If called after completion (including cancellation) will return an empty
    // ScriptPromise.
    ScriptPromise promise();

    WebCryptoResult result()
    {
        return WebCryptoResult(this, m_cancel.get());
    }

    DECLARE_VIRTUAL_TRACE();

private:
    class Resolver;
    class ResultCancel : public CryptoResultCancel {
    public:
        static PassRefPtr<ResultCancel> create()
        {
            return adoptRef(new ResultCancel);
        }

        bool cancelled() const override;

        void cancel();
    private:
        ResultCancel();

        int m_cancelled;
    };

    explicit CryptoResultImpl(ScriptState*);

    void cancel();
    void clearResolver();

    Member<Resolver> m_resolver;

    // Separately communicate cancellation to WebCryptoResults so as to
    // allow this result object, which will be on the Oilpan heap, to be
    // GCed and destructed as needed. That is, it may end being GCed while
    // the thread owning the heap is detached and shut down, which will
    // in some cases happen before corresponding webcrypto operations have
    // all been processed. Hence these webcrypto operations cannot reliably
    // check cancellation status via this result object. So, keep a separate
    // cancellation status object for the purpose, which will outlive the
    // result object and can be safely accessed by multiple threads.
    RefPtr<ResultCancel> m_cancel;
};

} // namespace blink

#endif // CryptoResultImpl_h
