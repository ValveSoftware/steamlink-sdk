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
#include "modules/crypto/CryptoResultImpl.h"

#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMError.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

class CryptoResultImpl::WeakResolver : public ScriptPromiseResolverWithContext {
public:
    static WeakPtr<ScriptPromiseResolverWithContext> create(ScriptState* scriptState, CryptoResultImpl* result)
    {
        RefPtr<WeakResolver> p = adoptRef(new WeakResolver(scriptState, result));
        p->suspendIfNeeded();
        p->keepAliveWhilePending();
        return p->m_weakPtrFactory.createWeakPtr();
    }

    virtual ~WeakResolver()
    {
        m_result->cancel();
    }

private:
    WeakResolver(ScriptState* scriptState, CryptoResultImpl* result)
        : ScriptPromiseResolverWithContext(scriptState)
        , m_weakPtrFactory(this)
        , m_result(result) { }
    WeakPtrFactory<ScriptPromiseResolverWithContext> m_weakPtrFactory;
    RefPtr<CryptoResultImpl> m_result;
};

ExceptionCode webCryptoErrorToExceptionCode(blink::WebCryptoErrorType errorType)
{
    switch (errorType) {
    case blink::WebCryptoErrorTypeNotSupported:
        return NotSupportedError;
    case blink::WebCryptoErrorTypeSyntax:
        return SyntaxError;
    case blink::WebCryptoErrorTypeInvalidState:
        return InvalidStateError;
    case blink::WebCryptoErrorTypeInvalidAccess:
        return InvalidAccessError;
    case blink::WebCryptoErrorTypeUnknown:
        return UnknownError;
    case blink::WebCryptoErrorTypeData:
        return DataError;
    case blink::WebCryptoErrorTypeOperation:
        return OperationError;
    case blink::WebCryptoErrorTypeType:
        // FIXME: This should construct a TypeError instead. For now do
        //        something to facilitate refactor, but this will need to be
        //        revisited.
        return DataError;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

CryptoResultImpl::~CryptoResultImpl()
{
}

PassRefPtr<CryptoResultImpl> CryptoResultImpl::create(ScriptState* scriptState)
{
    return adoptRef(new CryptoResultImpl(scriptState));
}

void CryptoResultImpl::completeWithError(blink::WebCryptoErrorType errorType, const blink::WebString& errorDetails)
{
    if (m_resolver)
        m_resolver->reject(DOMException::create(webCryptoErrorToExceptionCode(errorType), errorDetails));
}

void CryptoResultImpl::completeWithBuffer(const blink::WebArrayBuffer& buffer)
{
    if (m_resolver)
        m_resolver->resolve(PassRefPtr<ArrayBuffer>(buffer));
}

void CryptoResultImpl::completeWithJson(const char* utf8Data, unsigned length)
{
    if (m_resolver) {
        ScriptPromiseResolverWithContext* resolver = m_resolver.get();
        ScriptState* scriptState = resolver->scriptState();
        ScriptState::Scope scope(scriptState);

        v8::Handle<v8::String> jsonString = v8::String::NewFromUtf8(scriptState->isolate(), utf8Data, v8::String::kInternalizedString, length);

        v8::TryCatch exceptionCatcher;
        v8::Handle<v8::Value> jsonDictionary = v8::JSON::Parse(jsonString);
        if (exceptionCatcher.HasCaught() || jsonDictionary.IsEmpty()) {
            ASSERT_NOT_REACHED();
            resolver->reject(DOMException::create(OperationError, "Failed inflating JWK JSON to object"));
        } else {
            resolver->resolve(jsonDictionary);
        }
    }
}

void CryptoResultImpl::completeWithBoolean(bool b)
{
    if (m_resolver)
        m_resolver->resolve(b);
}

void CryptoResultImpl::completeWithKey(const blink::WebCryptoKey& key)
{
    if (m_resolver)
        m_resolver->resolve(Key::create(key));
}

void CryptoResultImpl::completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey)
{
    if (m_resolver) {
        ScriptState* scriptState = m_resolver->scriptState();
        ScriptState::Scope scope(scriptState);

        // FIXME: Use Dictionary instead, to limit amount of direct v8 access used from WebCore.
        v8::Handle<v8::Object> keyPair = v8::Object::New(scriptState->isolate());

        v8::Handle<v8::Value> publicKeyValue = toV8NoInline(Key::create(publicKey), scriptState->context()->Global(), scriptState->isolate());
        v8::Handle<v8::Value> privateKeyValue = toV8NoInline(Key::create(privateKey), scriptState->context()->Global(), scriptState->isolate());

        keyPair->Set(v8::String::NewFromUtf8(scriptState->isolate(), "publicKey"), publicKeyValue);
        keyPair->Set(v8::String::NewFromUtf8(scriptState->isolate(), "privateKey"), privateKeyValue);

        m_resolver->resolve(v8::Handle<v8::Value>(keyPair));
    }
}

bool CryptoResultImpl::cancelled() const
{
    return acquireLoad(&m_cancelled);
}

void CryptoResultImpl::cancel()
{
    releaseStore(&m_cancelled, 1);
}

CryptoResultImpl::CryptoResultImpl(ScriptState* scriptState)
    : m_cancelled(0)
{
    // Creating the WeakResolver may return nullptr if active dom objects have
    // been stopped. And in the process set m_cancelled to 1.
    m_resolver = WeakResolver::create(scriptState, this);
}

ScriptPromise CryptoResultImpl::promise()
{
    return m_resolver ? m_resolver->promise() : ScriptPromise();
}

} // namespace WebCore
