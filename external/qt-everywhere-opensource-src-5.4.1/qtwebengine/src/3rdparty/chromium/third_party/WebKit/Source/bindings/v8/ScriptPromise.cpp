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
#include "bindings/v8/ScriptPromise.h"

#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/DOMException.h"

#include <v8.h>

namespace WebCore {

namespace {

struct WithScriptState {
    // Used by ToV8Value<WithScriptState, ScriptState*>.
    static v8::Handle<v8::Object> getCreationContext(ScriptState* scriptState)
    {
        return scriptState->context()->Global();
    }
};

} // namespace

ScriptPromise::ScriptPromise(ScriptState* scriptState, v8::Handle<v8::Value> value)
    : m_scriptState(scriptState)
{
    if (value.IsEmpty())
        return;

    if (!value->IsPromise()) {
        m_promise = ScriptValue(scriptState, v8::Handle<v8::Value>());
        V8ThrowException::throwTypeError("the given value is not a Promise", scriptState->isolate());
        return;
    }
    m_promise = ScriptValue(scriptState, value);
}

ScriptPromise ScriptPromise::then(PassOwnPtr<ScriptFunction> onFulfilled, PassOwnPtr<ScriptFunction> onRejected)
{
    if (m_promise.isEmpty())
        return ScriptPromise();

    v8::Local<v8::Object> promise = m_promise.v8Value().As<v8::Object>();
    v8::Local<v8::Function> v8OnFulfilled = ScriptFunction::adoptByGarbageCollector(onFulfilled);
    v8::Local<v8::Function> v8OnRejected = ScriptFunction::adoptByGarbageCollector(onRejected);

    ASSERT(promise->IsPromise());
    // Return this Promise if no handlers are given.
    // In fact it is not the exact bahavior of Promise.prototype.then
    // but that is not a problem in this case.
    v8::Local<v8::Promise> resultPromise = promise.As<v8::Promise>();
    if (!v8OnFulfilled.IsEmpty()) {
        resultPromise = resultPromise->Then(v8OnFulfilled);
        if (resultPromise.IsEmpty()) {
            // v8::Promise::Then may return an empty value, for example when
            // the stack is exhausted.
            return ScriptPromise();
        }
    }
    if (!v8OnRejected.IsEmpty())
        resultPromise = resultPromise->Catch(v8OnRejected);

    return ScriptPromise(m_scriptState.get(), resultPromise);
}

ScriptPromise ScriptPromise::cast(ScriptState* scriptState, const ScriptValue& value)
{
    return ScriptPromise::cast(scriptState, value.v8Value());
}

ScriptPromise ScriptPromise::cast(ScriptState* scriptState, v8::Handle<v8::Value> value)
{
    if (value.IsEmpty())
        return ScriptPromise();
    if (value->IsPromise()) {
        return ScriptPromise(scriptState, value);
    }
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->resolve(value);
    return promise;
}

ScriptPromise ScriptPromise::reject(ScriptState* scriptState, const ScriptValue& value)
{
    return ScriptPromise::reject(scriptState, value.v8Value());
}

ScriptPromise ScriptPromise::reject(ScriptState* scriptState, v8::Handle<v8::Value> value)
{
    if (value.IsEmpty())
        return ScriptPromise();
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->reject(value);
    return promise;
}

ScriptPromise ScriptPromise::rejectWithDOMException(ScriptState* scriptState, PassRefPtrWillBeRawPtr<DOMException> exception)
{
    ASSERT(scriptState->isolate()->InContext());
    return reject(scriptState, ToV8Value<WithScriptState, v8::Handle<v8::Object> >::toV8Value(exception, scriptState->context()->Global(), scriptState->isolate()));
}

} // namespace WebCore
