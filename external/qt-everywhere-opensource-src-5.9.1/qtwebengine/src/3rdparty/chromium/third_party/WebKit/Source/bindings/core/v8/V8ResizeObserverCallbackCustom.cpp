// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ResizeObserverCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ResizeObserver.h"
#include "bindings/core/v8/V8ResizeObserverEntry.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

void V8ResizeObserverCallback::handleEvent(
    const HeapVector<Member<ResizeObserverEntry>>& entries,
    ResizeObserver* observer) {
  if (!canInvokeCallback())
    return;

  v8::Isolate* isolate = m_scriptState->isolate();

  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());

  if (m_callback.isEmpty())
    return;

  v8::Local<v8::Value> observerHandle = toV8(
      observer, m_scriptState->context()->Global(), m_scriptState->isolate());
  if (!observerHandle->IsObject())
    return;

  v8::Local<v8::Object> thisObject =
      v8::Local<v8::Object>::Cast(observerHandle);
  v8::Local<v8::Value> entriesHandle = toV8(
      entries, m_scriptState->context()->Global(), m_scriptState->isolate());
  if (entriesHandle.IsEmpty())
    return;
  v8::Local<v8::Value> argv[] = {entriesHandle, observerHandle};

  v8::TryCatch exceptionCatcher(m_scriptState->isolate());
  exceptionCatcher.SetVerbose(true);
  V8ScriptRunner::callFunction(m_callback.newLocal(isolate),
                               getExecutionContext(), thisObject,
                               WTF_ARRAY_LENGTH(argv), argv, isolate);
}

}  // namespace blink
