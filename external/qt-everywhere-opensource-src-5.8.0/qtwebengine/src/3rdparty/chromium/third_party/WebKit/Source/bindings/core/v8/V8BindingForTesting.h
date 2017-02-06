// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8BindingForTesting_h
#define V8BindingForTesting_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include <v8.h>

namespace blink {

class Document;
class DOMWrapperWorld;
class DummyPageHolder;
class ExecutionContext;
class LocalFrame;
class Page;

class ScriptStateForTesting : public ScriptState {
public:
    static PassRefPtr<ScriptStateForTesting> create(v8::Local<v8::Context>, PassRefPtr<DOMWrapperWorld>);
    ExecutionContext* getExecutionContext() const override;
    void setExecutionContext(ExecutionContext*) override;
private:
    ScriptStateForTesting(v8::Local<v8::Context>, PassRefPtr<DOMWrapperWorld>);
    Persistent<ExecutionContext> m_executionContext;
};

class V8TestingScope {
    STACK_ALLOCATED();
public:
    V8TestingScope();
    ScriptState* getScriptState() const;
    ExecutionContext* getExecutionContext() const;
    v8::Isolate* isolate() const;
    v8::Local<v8::Context> context() const;
    ExceptionState& getExceptionState();
    Page& page();
    LocalFrame& frame();
    Document& document();
    ~V8TestingScope();

private:
    std::unique_ptr<DummyPageHolder> m_holder;
    v8::HandleScope m_handleScope;
    v8::Local<v8::Context> m_context;
    v8::Context::Scope m_contextScope;
    TrackExceptionState m_exceptionState;
};

} // namespace blink

#endif // V8BindingForTesting_h
