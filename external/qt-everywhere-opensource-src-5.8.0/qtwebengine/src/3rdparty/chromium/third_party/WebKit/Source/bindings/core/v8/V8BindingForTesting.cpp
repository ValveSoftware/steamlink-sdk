// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForTesting.h"

#include "core/testing/DummyPageHolder.h"

namespace blink {

PassRefPtr<ScriptStateForTesting> ScriptStateForTesting::create(v8::Local<v8::Context> context, PassRefPtr<DOMWrapperWorld> world)
{
    RefPtr<ScriptStateForTesting> scriptState = adoptRef(new ScriptStateForTesting(context, world));
    // This ref() is for keeping this ScriptState alive as long as the v8::Context is alive.
    // This is deref()ed in the weak callback of the v8::Context.
    scriptState->ref();
    return scriptState;
}

ScriptStateForTesting::ScriptStateForTesting(v8::Local<v8::Context> context, PassRefPtr<DOMWrapperWorld> world)
    : ScriptState(context, world)
{
}

ExecutionContext* ScriptStateForTesting::getExecutionContext() const
{
    return m_executionContext;
}

void ScriptStateForTesting::setExecutionContext(ExecutionContext* executionContext)
{
    m_executionContext = executionContext;
}

V8TestingScope::V8TestingScope()
    : m_holder(DummyPageHolder::create())
    , m_handleScope(isolate())
    , m_context(getScriptState()->context())
    , m_contextScope(context())
{
}

ScriptState* V8TestingScope::getScriptState() const
{
    return ScriptState::forMainWorld(m_holder->document().frame());
}

ExecutionContext* V8TestingScope::getExecutionContext() const
{
    return getScriptState()->getExecutionContext();
}

v8::Isolate* V8TestingScope::isolate() const
{
    return getScriptState()->isolate();
}

v8::Local<v8::Context> V8TestingScope::context() const
{
    return m_context;
}

ExceptionState& V8TestingScope::getExceptionState()
{
    return m_exceptionState;
}

Page& V8TestingScope::page()
{
    return m_holder->page();
}

LocalFrame& V8TestingScope::frame()
{
    return m_holder->frame();
}

Document& V8TestingScope::document()
{
    return m_holder->document();
}

V8TestingScope::~V8TestingScope()
{
    if (m_holder->document().frame())
        getScriptState()->disposePerContextData();
}

} // namespace blink
