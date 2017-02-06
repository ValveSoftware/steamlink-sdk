// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8PerformanceObserverCallback_h
#define V8PerformanceObserverCallback_h

#include "bindings/core/v8/ActiveDOMCallback.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "core/CoreExport.h"
#include "core/timing/PerformanceObserverCallback.h"

namespace blink {

class V8PerformanceObserverCallback final : public PerformanceObserverCallback, public ActiveDOMCallback {
    USING_GARBAGE_COLLECTED_MIXIN(V8PerformanceObserverCallback);
public:
    static V8PerformanceObserverCallback* create(v8::Local<v8::Function> callback, v8::Local<v8::Object> owner, ScriptState* scriptState)
    {
        return new V8PerformanceObserverCallback(callback, owner, scriptState);
    }

    ~V8PerformanceObserverCallback() override;

    DECLARE_VIRTUAL_TRACE();

    void handleEvent(PerformanceObserverEntryList*, PerformanceObserver*) override;
    ExecutionContext* getExecutionContext() const override { return ContextLifecycleObserver::getExecutionContext(); }
private:
    CORE_EXPORT V8PerformanceObserverCallback(v8::Local<v8::Function>, v8::Local<v8::Object>, ScriptState*);

    ScopedPersistent<v8::Function> m_callback;
    RefPtr<ScriptState> m_scriptState;
};

} // namespace blink
#endif // V8PerformanceObserverCallback_h
