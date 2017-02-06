// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8IDBObserverCallback_h
#define V8IDBObserverCallback_h

#include "bindings/core/v8/ActiveDOMCallback.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "modules/indexeddb/IDBObserverCallback.h"

namespace blink {

class V8IDBObserverCallback final : public IDBObserverCallback, public ActiveDOMCallback {
    USING_GARBAGE_COLLECTED_MIXIN(V8IDBObserverCallback);

public:
    V8IDBObserverCallback(v8::Local<v8::Function>, v8::Local<v8::Object>, ScriptState*);
    ~V8IDBObserverCallback() override;

    DECLARE_VIRTUAL_TRACE();

    ExecutionContext* getExecutionContext() const override { return ContextLifecycleObserver::getExecutionContext(); }
private:
    ScopedPersistent<v8::Function> m_callback;
    RefPtr<ScriptState> m_scriptState;
};

} // namespace blink
#endif // V8IDBObserverCallback_h
