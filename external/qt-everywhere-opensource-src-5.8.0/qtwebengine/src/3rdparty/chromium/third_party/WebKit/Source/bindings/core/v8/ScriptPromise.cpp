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

#include "bindings/core/v8/ScriptPromise.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/inspector/InstanceCounters.h"
#include <v8.h>

namespace blink {

namespace {

class PromiseAllHandler final : public GarbageCollectedFinalized<PromiseAllHandler> {
    WTF_MAKE_NONCOPYABLE(PromiseAllHandler);
public:
    static ScriptPromise all(ScriptState* scriptState, const Vector<ScriptPromise>& promises)
    {
        if (promises.isEmpty())
            return ScriptPromise::cast(scriptState, v8::Array::New(scriptState->isolate()));
        return (new PromiseAllHandler(scriptState, promises))->m_resolver.promise();
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

private:
    class AdapterFunction : public ScriptFunction {
    public:
        enum ResolveType {
            Fulfilled,
            Rejected,
        };

        static v8::Local<v8::Function> create(ScriptState* scriptState, ResolveType resolveType, size_t index, PromiseAllHandler* handler)
        {
            AdapterFunction* self = new AdapterFunction(scriptState, resolveType, index, handler);
            return self->bindToV8Function();
        }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            visitor->trace(m_handler);
            ScriptFunction::trace(visitor);
        }

    private:
        AdapterFunction(ScriptState* scriptState, ResolveType resolveType, size_t index, PromiseAllHandler* handler)
            : ScriptFunction(scriptState)
            , m_resolveType(resolveType)
            , m_index(index)
            , m_handler(handler) { }

        ScriptValue call(ScriptValue value) override
        {
            if (m_resolveType == Fulfilled)
                m_handler->onFulfilled(m_index, value);
            else
                m_handler->onRejected(value);
            // This return value is never used.
            return ScriptValue();
        }

        const ResolveType m_resolveType;
        const size_t m_index;
        Member<PromiseAllHandler> m_handler;
    };

    PromiseAllHandler(ScriptState* scriptState, Vector<ScriptPromise> promises)
        : m_numberOfPendingPromises(promises.size())
        , m_resolver(scriptState)
    {
        ASSERT(!promises.isEmpty());
        m_values.resize(promises.size());
        for (size_t i = 0; i < promises.size(); ++i)
            promises[i].then(createFulfillFunction(scriptState, i), createRejectFunction(scriptState));
    }

    v8::Local<v8::Function> createFulfillFunction(ScriptState* scriptState, size_t index)
    {
        return AdapterFunction::create(scriptState, AdapterFunction::Fulfilled, index, this);
    }

    v8::Local<v8::Function> createRejectFunction(ScriptState* scriptState)
    {
        return AdapterFunction::create(scriptState, AdapterFunction::Rejected, 0, this);
    }

    void onFulfilled(size_t index, const ScriptValue& value)
    {
        if (m_isSettled)
            return;

        ASSERT(index < m_values.size());
        m_values[index] = value;
        if (--m_numberOfPendingPromises > 0)
            return;

        v8::Local<v8::Array> values = v8::Array::New(value.isolate(), m_values.size());
        for (size_t i = 0; i < m_values.size(); ++i) {
            if (!v8CallBoolean(values->CreateDataProperty(value.context(), i, m_values[i].v8Value())))
                return;
        }

        markPromiseSettled();
        m_resolver.resolve(values);
    }

    void onRejected(const ScriptValue& value)
    {
        if (m_isSettled)
            return;
        markPromiseSettled();
        m_resolver.reject(value.v8Value());
    }

    void markPromiseSettled()
    {
        ASSERT(!m_isSettled);
        m_isSettled = true;
        m_values.clear();
    }

    size_t m_numberOfPendingPromises;
    ScriptPromise::InternalResolver m_resolver;
    bool m_isSettled = false;

    // This is cleared when owners of this handler, that is, given promises are
    // settled.
    Vector<ScriptValue> m_values;
};

} // namespace

ScriptPromise::InternalResolver::InternalResolver(ScriptState* scriptState)
    : m_resolver(scriptState, v8::Promise::Resolver::New(scriptState->context())) { }

v8::Local<v8::Promise> ScriptPromise::InternalResolver::v8Promise() const
{
    if (m_resolver.isEmpty())
        return v8::Local<v8::Promise>();
    return m_resolver.v8Value().As<v8::Promise::Resolver>()->GetPromise();
}

ScriptPromise ScriptPromise::InternalResolver::promise() const
{
    if (m_resolver.isEmpty())
        return ScriptPromise();
    return ScriptPromise(m_resolver.getScriptState(), v8Promise());
}

void ScriptPromise::InternalResolver::resolve(v8::Local<v8::Value> value)
{
    if (m_resolver.isEmpty())
        return;
    m_resolver.v8Value().As<v8::Promise::Resolver>()->Resolve(m_resolver.context(), value);
    clear();
}

void ScriptPromise::InternalResolver::reject(v8::Local<v8::Value> value)
{
    if (m_resolver.isEmpty())
        return;
    m_resolver.v8Value().As<v8::Promise::Resolver>()->Reject(m_resolver.context(), value);
    clear();
}

ScriptPromise::ScriptPromise()
{
    increaseInstanceCount();
}

ScriptPromise::ScriptPromise(ScriptState* scriptState, v8::Local<v8::Value> value)
    : m_scriptState(scriptState)
{
    increaseInstanceCount();

    if (value.IsEmpty())
        return;

    if (!value->IsPromise()) {
        m_promise = ScriptValue(scriptState, v8::Local<v8::Value>());
        V8ThrowException::throwTypeError(scriptState->isolate(), "the given value is not a Promise");
        return;
    }
    m_promise = ScriptValue(scriptState, value);
}

ScriptPromise::ScriptPromise(const ScriptPromise& other)
{
    increaseInstanceCount();

    this->m_scriptState = other.m_scriptState;
    this->m_promise = other.m_promise;
}

ScriptPromise::~ScriptPromise()
{
    decreaseInstanceCount();
}

ScriptPromise ScriptPromise::then(v8::Local<v8::Function> onFulfilled, v8::Local<v8::Function> onRejected)
{
    if (m_promise.isEmpty())
        return ScriptPromise();

    v8::Local<v8::Object> promise = m_promise.v8Value().As<v8::Object>();

    ASSERT(promise->IsPromise());
    // Return this Promise if no handlers are given.
    // In fact it is not the exact bahavior of Promise.prototype.then
    // but that is not a problem in this case.
    v8::Local<v8::Promise> resultPromise = promise.As<v8::Promise>();
    if (!onFulfilled.IsEmpty()) {
        if (!resultPromise->Then(m_scriptState->context(), onFulfilled).ToLocal(&resultPromise))
            return ScriptPromise();
    }
    if (!onRejected.IsEmpty()) {
        if (!resultPromise->Catch(m_scriptState->context(), onRejected).ToLocal(&resultPromise))
            return ScriptPromise();
    }

    return ScriptPromise(m_scriptState.get(), resultPromise);
}

ScriptPromise ScriptPromise::castUndefined(ScriptState* scriptState)
{
    return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
}

ScriptPromise ScriptPromise::cast(ScriptState* scriptState, const ScriptValue& value)
{
    return ScriptPromise::cast(scriptState, value.v8Value());
}

ScriptPromise ScriptPromise::cast(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (value.IsEmpty())
        return ScriptPromise();
    if (value->IsPromise()) {
        return ScriptPromise(scriptState, value);
    }
    InternalResolver resolver(scriptState);
    ScriptPromise promise = resolver.promise();
    resolver.resolve(value);
    return promise;
}

ScriptPromise ScriptPromise::reject(ScriptState* scriptState, const ScriptValue& value)
{
    return ScriptPromise::reject(scriptState, value.v8Value());
}

ScriptPromise ScriptPromise::reject(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (value.IsEmpty())
        return ScriptPromise();
    InternalResolver resolver(scriptState);
    ScriptPromise promise = resolver.promise();
    resolver.reject(value);
    return promise;
}

ScriptPromise ScriptPromise::rejectWithDOMException(ScriptState* scriptState, DOMException* exception)
{
    ASSERT(scriptState->isolate()->InContext());
    return reject(scriptState, toV8(exception, scriptState->context()->Global(), scriptState->isolate()));
}

v8::Local<v8::Promise> ScriptPromise::rejectRaw(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (value.IsEmpty())
        return v8::Local<v8::Promise>();
    v8::Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(scriptState->context()).ToLocal(&resolver))
        return v8::Local<v8::Promise>();
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    resolver->Reject(scriptState->context(), value);
    return promise;
}

ScriptPromise ScriptPromise::all(ScriptState* scriptState, const Vector<ScriptPromise>& promises)
{
    return PromiseAllHandler::all(scriptState, promises);
}

void ScriptPromise::increaseInstanceCount()
{
    // An instance is only counted only on the main thread. This is because the
    // leak detector can detect leaks on the main thread so far. We plan to fix
    // the leak detector to work on worker threads (crbug.com/507224).
    if (isMainThread())
        InstanceCounters::incrementCounter(InstanceCounters::ScriptPromiseCounter);
}

void ScriptPromise::decreaseInstanceCount()
{
    if (isMainThread())
        InstanceCounters::decrementCounter(InstanceCounters::ScriptPromiseCounter);
}

} // namespace blink
