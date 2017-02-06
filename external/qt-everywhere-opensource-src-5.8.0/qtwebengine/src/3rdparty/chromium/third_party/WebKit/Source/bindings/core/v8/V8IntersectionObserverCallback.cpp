// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserverCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8IntersectionObserver.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Assertions.h"

namespace blink {

V8IntersectionObserverCallback::V8IntersectionObserverCallback(v8::Local<v8::Function> callback, v8::Local<v8::Object> owner, ScriptState* scriptState)
    : ActiveDOMCallback(scriptState->getExecutionContext())
    , m_callback(scriptState->isolate(), callback)
    , m_scriptState(scriptState)
{
    V8PrivateProperty::getIntersectionObserverCallback(scriptState->isolate()).set(scriptState->context(), owner, callback);
    m_callback.setPhantom();
}

V8IntersectionObserverCallback::~V8IntersectionObserverCallback()
{
}

void V8IntersectionObserverCallback::handleEvent(const HeapVector<Member<IntersectionObserverEntry>>& entries, IntersectionObserver& observer)
{
    if (!canInvokeCallback())
        return;

    if (!m_scriptState->contextIsValid())
        return;
    ScriptState::Scope scope(m_scriptState.get());

    if (m_callback.isEmpty())
        return;
    v8::Local<v8::Value> observerHandle = toV8(&observer, m_scriptState->context()->Global(), m_scriptState->isolate());
    if (observerHandle.IsEmpty()) {
        if (!isScriptControllerTerminating())
            CRASH();
        return;
    }

    if (!observerHandle->IsObject())
        return;

    v8::Local<v8::Object> thisObject = v8::Local<v8::Object>::Cast(observerHandle);
    v8::Local<v8::Value> entriesHandle = toV8(entries, m_scriptState->context()->Global(), m_scriptState->isolate());
    if (entriesHandle.IsEmpty()) {
        return;
    }
    v8::Local<v8::Value> argv[] = { entriesHandle, observerHandle };

    v8::TryCatch exceptionCatcher(m_scriptState->isolate());
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunction(m_scriptState->getExecutionContext(), m_callback.newLocal(m_scriptState->isolate()), thisObject, 2, argv, m_scriptState->isolate());
}

DEFINE_TRACE(V8IntersectionObserverCallback)
{
    IntersectionObserverCallback::trace(visitor);
    ActiveDOMCallback::trace(visitor);
}

} // namespace blink
