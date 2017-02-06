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

#include "modules/crypto/CryptoResultImpl.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "bindings/modules/v8/V8CryptoKey.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMError.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "modules/crypto/CryptoKey.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/Atomics.h"

namespace blink {

static void rejectWithTypeError(const String& errorDetails, ScriptPromiseResolver* resolver)
{
    // Duplicate some of the checks done by ScriptPromiseResolver.
    if (!resolver->getExecutionContext() || resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;

    ScriptState::Scope scope(resolver->getScriptState());
    v8::Isolate* isolate = resolver->getScriptState()->isolate();
    resolver->reject(v8::Exception::TypeError(v8String(isolate, errorDetails)));
}

class CryptoResultImpl::Resolver final : public ScriptPromiseResolver {
public:
    static Resolver* create(ScriptState* scriptState, CryptoResultImpl* result)
    {
        ASSERT(scriptState->contextIsValid());
        Resolver* resolver = new Resolver(scriptState, result);
        resolver->suspendIfNeeded();
        resolver->keepAliveWhilePending();
        return resolver;
    }

    void stop() override
    {
        m_result->cancel();
        m_result = nullptr;
        ScriptPromiseResolver::stop();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_result);
        ScriptPromiseResolver::trace(visitor);
    }

private:
    Resolver(ScriptState* scriptState, CryptoResultImpl* result)
        : ScriptPromiseResolver(scriptState)
        , m_result(result) { }

    Member<CryptoResultImpl> m_result;
};

CryptoResultImpl::ResultCancel::ResultCancel()
    : m_cancelled(0)
{
}

bool CryptoResultImpl::ResultCancel::cancelled() const
{
    return acquireLoad(&m_cancelled);
}

void CryptoResultImpl::ResultCancel::cancel()
{
    releaseStore(&m_cancelled, 1);
}

ExceptionCode webCryptoErrorToExceptionCode(WebCryptoErrorType errorType)
{
    switch (errorType) {
    case WebCryptoErrorTypeNotSupported:
        return NotSupportedError;
    case WebCryptoErrorTypeSyntax:
        return SyntaxError;
    case WebCryptoErrorTypeInvalidAccess:
        return InvalidAccessError;
    case WebCryptoErrorTypeData:
        return DataError;
    case WebCryptoErrorTypeOperation:
        return OperationError;
    case WebCryptoErrorTypeType:
        return V8TypeError;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

CryptoResultImpl::CryptoResultImpl(ScriptState* scriptState)
    : m_resolver(Resolver::create(scriptState, this))
    , m_cancel(ResultCancel::create())
{
    // Sync cancellation state.
    if (scriptState->getExecutionContext()->activeDOMObjectsAreStopped())
        m_cancel->cancel();
}

CryptoResultImpl::~CryptoResultImpl()
{
    ASSERT(!m_resolver);
}

DEFINE_TRACE(CryptoResultImpl)
{
    visitor->trace(m_resolver);
    CryptoResult::trace(visitor);
}

void CryptoResultImpl::clearResolver()
{
    m_resolver = nullptr;
}

CryptoResultImpl* CryptoResultImpl::create(ScriptState* scriptState)
{
    return new CryptoResultImpl(scriptState);
}

void CryptoResultImpl::completeWithError(WebCryptoErrorType errorType, const WebString& errorDetails)
{
    if (!m_resolver)
        return;

    ExceptionCode ec = webCryptoErrorToExceptionCode(errorType);

    // Handle TypeError separately, as it cannot be created using
    // DOMException.
    if (ec == V8TypeError)
        rejectWithTypeError(errorDetails, m_resolver);
    else
        m_resolver->reject(DOMException::create(ec, errorDetails));
    clearResolver();
}

void CryptoResultImpl::completeWithBuffer(const void* bytes, unsigned bytesSize)
{
    if (!m_resolver)
        return;

    m_resolver->resolve(DOMArrayBuffer::create(bytes, bytesSize));
    clearResolver();
}

void CryptoResultImpl::completeWithJson(const char* utf8Data, unsigned length)
{
    if (!m_resolver)
        return;

    ScriptState* scriptState = m_resolver->getScriptState();
    ScriptState::Scope scope(scriptState);

    v8::Local<v8::String> jsonString = v8AtomicString(scriptState->isolate(), utf8Data, length);

    v8::TryCatch exceptionCatcher(scriptState->isolate());
    v8::Local<v8::Value> jsonDictionary;
    if (v8Call(v8::JSON::Parse(scriptState->isolate(), jsonString), jsonDictionary, exceptionCatcher))
        m_resolver->resolve(jsonDictionary);
    else
        m_resolver->reject(exceptionCatcher.Exception());
    clearResolver();
}

void CryptoResultImpl::completeWithBoolean(bool b)
{
    if (!m_resolver)
        return;

    m_resolver->resolve(b);
    clearResolver();
}

void CryptoResultImpl::completeWithKey(const WebCryptoKey& key)
{
    if (!m_resolver)
        return;

    m_resolver->resolve(CryptoKey::create(key));
    clearResolver();
}

void CryptoResultImpl::completeWithKeyPair(const WebCryptoKey& publicKey, const WebCryptoKey& privateKey)
{
    if (!m_resolver)
        return;

    ScriptState* scriptState = m_resolver->getScriptState();
    ScriptState::Scope scope(scriptState);

    V8ObjectBuilder keyPair(scriptState);

    keyPair.add("publicKey", ScriptValue::from(scriptState, CryptoKey::create(publicKey)));
    keyPair.add("privateKey", ScriptValue::from(scriptState, CryptoKey::create(privateKey)));

    m_resolver->resolve(keyPair.v8Value());
    clearResolver();
}

void CryptoResultImpl::cancel()
{
    ASSERT(m_cancel);
    m_cancel->cancel();
    m_cancel.clear();
    clearResolver();
}

ScriptPromise CryptoResultImpl::promise()
{
    return m_resolver ? m_resolver->promise() : ScriptPromise();
}

} // namespace blink
