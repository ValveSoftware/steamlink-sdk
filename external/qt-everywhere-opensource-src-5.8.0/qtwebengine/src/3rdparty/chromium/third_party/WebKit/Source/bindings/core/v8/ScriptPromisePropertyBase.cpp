// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromisePropertyBase.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ScriptPromisePropertyBase::ScriptPromisePropertyBase(ExecutionContext* executionContext, Name name)
    : ContextLifecycleObserver(executionContext)
    , m_isolate(toIsolate(executionContext))
    , m_name(name)
    , m_state(Pending)
{
}

ScriptPromisePropertyBase::~ScriptPromisePropertyBase()
{
    clearWrappers();
}

static void clearHandle(const v8::WeakCallbackInfo<ScopedPersistent<v8::Object>>& data)
{
    data.GetParameter()->clear();
}

ScriptPromise ScriptPromisePropertyBase::promise(DOMWrapperWorld& world)
{
    if (!getExecutionContext())
        return ScriptPromise();

    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Context> context = toV8Context(getExecutionContext(), world);
    if (context.IsEmpty())
        return ScriptPromise();
    ScriptState* scriptState = ScriptState::from(context);
    ScriptState::Scope scope(scriptState);

    v8::Local<v8::Object> wrapper = ensureHolderWrapper(scriptState);
    ASSERT(wrapper->CreationContext() == context);

    v8::Local<v8::Value> cachedPromise = V8HiddenValue::getHiddenValue(scriptState, wrapper, promiseName());
    if (!cachedPromise.IsEmpty() && cachedPromise->IsPromise())
        return ScriptPromise(scriptState, cachedPromise);

    // Create and cache the Promise
    v8::Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(context).ToLocal(&resolver))
        return ScriptPromise();
    v8::Local<v8::Promise> promise = resolver->GetPromise();
    V8HiddenValue::setHiddenValue(scriptState, wrapper, promiseName(), promise);

    switch (m_state) {
    case Pending:
        // Cache the resolver too
        V8HiddenValue::setHiddenValue(scriptState, wrapper, resolverName(), resolver);
        break;
    case Resolved:
    case Rejected:
        resolveOrRejectInternal(resolver);
        break;
    }

    return ScriptPromise(scriptState, promise);
}

void ScriptPromisePropertyBase::resolveOrReject(State targetState)
{
    ASSERT(getExecutionContext());
    ASSERT(m_state == Pending);
    ASSERT(targetState == Resolved || targetState == Rejected);

    m_state = targetState;

    v8::HandleScope handleScope(m_isolate);
    size_t i = 0;
    while (i < m_wrappers.size()) {
        const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent = m_wrappers[i];
        if (persistent->isEmpty()) {
            // wrapper has died.
            // Since v8 GC can run during the iteration and clear the reference,
            // we can't move this check out of the loop.
            m_wrappers.remove(i);
            continue;
        }
        v8::Local<v8::Object> wrapper = persistent->newLocal(m_isolate);
        ScriptState* scriptState = ScriptState::from(wrapper->CreationContext());
        ScriptState::Scope scope(scriptState);

        v8::Local<v8::Promise::Resolver> resolver = V8HiddenValue::getHiddenValue(scriptState, wrapper, resolverName()).As<v8::Promise::Resolver>();

        V8HiddenValue::deleteHiddenValue(scriptState, wrapper, resolverName());
        resolveOrRejectInternal(resolver);
        ++i;
    }
}

void ScriptPromisePropertyBase::resetBase()
{
    checkThis();
    clearWrappers();
    m_state = Pending;
}

void ScriptPromisePropertyBase::resolveOrRejectInternal(v8::Local<v8::Promise::Resolver> resolver)
{
    v8::Local<v8::Context> context = resolver->CreationContext();
    switch (m_state) {
    case Pending:
        ASSERT_NOT_REACHED();
        break;
    case Resolved:
        resolver->Resolve(context, resolvedValue(m_isolate, context->Global()));
        break;
    case Rejected:
        resolver->Reject(context, rejectedValue(m_isolate, context->Global()));
        break;
    }
}

v8::Local<v8::Object> ScriptPromisePropertyBase::ensureHolderWrapper(ScriptState* scriptState)
{
    v8::Local<v8::Context> context = scriptState->context();
    size_t i = 0;
    while (i < m_wrappers.size()) {
        const std::unique_ptr<ScopedPersistent<v8::Object>>& persistent = m_wrappers[i];
        if (persistent->isEmpty()) {
            // wrapper has died.
            // Since v8 GC can run during the iteration and clear the reference,
            // we can't move this check out of the loop.
            m_wrappers.remove(i);
            continue;
        }

        v8::Local<v8::Object> wrapper = persistent->newLocal(m_isolate);
        if (wrapper->CreationContext() == context)
            return wrapper;
        ++i;
    }
    v8::Local<v8::Object> wrapper = holder(m_isolate, context->Global());
    std::unique_ptr<ScopedPersistent<v8::Object>> weakPersistent = wrapUnique(new ScopedPersistent<v8::Object>);
    weakPersistent->set(m_isolate, wrapper);
    weakPersistent->setWeak(weakPersistent.get(), &clearHandle);
    m_wrappers.append(std::move(weakPersistent));
    return wrapper;
}

void ScriptPromisePropertyBase::clearWrappers()
{
    checkThis();
    checkWrappers();
    v8::HandleScope handleScope(m_isolate);
    for (WeakPersistentSet::iterator i = m_wrappers.begin(); i != m_wrappers.end(); ++i) {
        v8::Local<v8::Object> wrapper = (*i)->newLocal(m_isolate);
        if (!wrapper.IsEmpty()) {
            ScriptState* scriptState = ScriptState::from(wrapper->CreationContext());
            V8HiddenValue::deleteHiddenValue(scriptState, wrapper, resolverName());
            V8HiddenValue::deleteHiddenValue(scriptState, wrapper, promiseName());
        }
    }
    m_wrappers.clear();
}

void ScriptPromisePropertyBase::checkThis()
{
    RELEASE_ASSERT(this);
}

void ScriptPromisePropertyBase::checkWrappers()
{
    for (WeakPersistentSet::iterator i = m_wrappers.begin(); i != m_wrappers.end(); ++i) {
        RELEASE_ASSERT(*i);
    }
}

v8::Local<v8::String> ScriptPromisePropertyBase::promiseName()
{
    switch (m_name) {
#define P(Name)                                           \
    case Name:                                            \
        return V8HiddenValue::Name ## Promise(m_isolate);

        SCRIPT_PROMISE_PROPERTIES(P)

#undef P
    }
    ASSERT_NOT_REACHED();
    return v8::Local<v8::String>();
}

v8::Local<v8::String> ScriptPromisePropertyBase::resolverName()
{
    switch (m_name) {
#define P(Name)                                            \
    case Name:                                             \
        return V8HiddenValue::Name ## Resolver(m_isolate);

        SCRIPT_PROMISE_PROPERTIES(P)

#undef P
    }
    ASSERT_NOT_REACHED();
    return v8::Local<v8::String>();
}

DEFINE_TRACE(ScriptPromisePropertyBase)
{
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
