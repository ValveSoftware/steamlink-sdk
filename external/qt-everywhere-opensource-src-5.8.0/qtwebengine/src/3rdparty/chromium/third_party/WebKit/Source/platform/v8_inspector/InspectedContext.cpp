// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/InspectedContext.h"

#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/V8Console.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

namespace {

    void clearContext(const v8::WeakCallbackInfo<v8::Global<v8::Context>>& data) {
        // Inspected context is created in V8InspectorImpl::contextCreated method
        // and destroyed in V8InspectorImpl::contextDestroyed.
        // Both methods takes valid v8::Local<v8::Context> handle to the same context,
        // it means that context is created before InspectedContext constructor and is
        // always destroyed after InspectedContext destructor therefore this callback
        // should be never called.
        // It's possible only if inspector client doesn't call contextDestroyed which
        // is considered an error.
        CHECK(false);
        data.GetParameter()->Reset();
    }
    
} // namespace

InspectedContext::InspectedContext(V8DebuggerImpl* debugger, const V8ContextInfo& info, int contextId)
    : m_debugger(debugger)
    , m_context(info.context->GetIsolate(), info.context)
    , m_contextId(contextId)
    , m_contextGroupId(info.contextGroupId)
    , m_isDefault(info.isDefault)
    , m_origin(info.origin)
    , m_humanReadableName(info.humanReadableName)
    , m_frameId(info.frameId)
    , m_reported(false)
{
    m_context.SetWeak(&m_context, &clearContext, v8::WeakCallbackType::kParameter);

    v8::Isolate* isolate = m_debugger->isolate();
    v8::Local<v8::Object> global = info.context->Global();
    v8::Local<v8::Object> console = V8Console::createConsole(this, info.hasMemoryOnConsole);
    if (!global->Set(info.context, toV8StringInternalized(isolate, "console"), console).FromMaybe(false))
        return;
    m_console.Reset(isolate, console);
    m_console.SetWeak();
}

InspectedContext::~InspectedContext()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope scope(isolate());
        V8Console::clearInspectedContextIfNeeded(context(), m_console.Get(isolate()));
    }
}

v8::Local<v8::Context> InspectedContext::context() const
{
    return m_context.Get(isolate());
}

v8::Isolate* InspectedContext::isolate() const
{
    return m_debugger->isolate();
}

bool InspectedContext::createInjectedScript()
{
    DCHECK(!m_injectedScript);
    v8::HandleScope handles(isolate());
    v8::Local<v8::Context> localContext = context();
    v8::Local<v8::Context> callingContext = isolate()->GetCallingContext();
    if (!callingContext.IsEmpty() && !m_debugger->client()->callingContextCanAccessContext(callingContext, localContext))
        return false;
    std::unique_ptr<InjectedScript> injectedScript = InjectedScript::create(this);
    // InjectedScript::create can destroy |this|.
    if (!injectedScript) return false;
    m_injectedScript = std::move(injectedScript);
    return true;
}

void InspectedContext::discardInjectedScript()
{
    m_injectedScript.reset();
}

} // namespace blink
