// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserverCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/modules/v8/V8IDBObserver.h"

namespace blink {

V8IDBObserverCallback::V8IDBObserverCallback(v8::Local<v8::Function> callback, v8::Local<v8::Object> owner, ScriptState* scriptState)
    : ActiveDOMCallback(scriptState->getExecutionContext())
    , m_callback(scriptState->isolate(), callback)
    , m_scriptState(scriptState)
{
    V8PrivateProperty::getIDBObserverCallback(scriptState->isolate()).set(scriptState->context(), owner, callback);
    m_callback.setPhantom();
}

V8IDBObserverCallback::~V8IDBObserverCallback()
{
}

DEFINE_TRACE(V8IDBObserverCallback)
{
    IDBObserverCallback::trace(visitor);
    ActiveDOMCallback::trace(visitor);
}

} // namespace blink
