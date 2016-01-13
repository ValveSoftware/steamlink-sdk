/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/inspector/InjectedScriptManager.h"

#include "bindings/core/v8/V8InjectedScriptHost.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/v8/BindingSecurity.h"
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptDebugServer.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct InjectedScriptManager::CallbackData {
    ScopedPersistent<v8::Object> handle;
    RefPtr<InjectedScriptHost> host;
};

static v8::Local<v8::Object> createInjectedScriptHostV8Wrapper(InjectedScriptHost* host, v8::Isolate* isolate)
{
    v8::Local<v8::Function> function = V8InjectedScriptHost::domTemplate(isolate)->GetFunction();
    if (function.IsEmpty()) {
        // Return if allocation failed.
        return v8::Local<v8::Object>();
    }
    v8::Local<v8::Object> instanceTemplate = V8ObjectConstructor::newInstance(isolate, function);
    if (instanceTemplate.IsEmpty()) {
        // Avoid setting the wrapper if allocation failed.
        return v8::Local<v8::Object>();
    }
    V8DOMWrapper::setNativeInfo(instanceTemplate, &V8InjectedScriptHost::wrapperTypeInfo, host);
    // Create a weak reference to the v8 wrapper of InspectorBackend to deref
    // InspectorBackend when the wrapper is garbage collected.
    InjectedScriptManager::CallbackData* data = new InjectedScriptManager::CallbackData;
    data->host = host;
    data->handle.set(isolate, instanceTemplate);
    data->handle.setWeak(data, &InjectedScriptManager::setWeakCallback);
    return instanceTemplate;
}

ScriptValue InjectedScriptManager::createInjectedScript(const String& scriptSource, ScriptState* inspectedScriptState, int id)
{
    v8::Isolate* isolate = inspectedScriptState->isolate();
    ScriptState::Scope scope(inspectedScriptState);

    // Call custom code to create InjectedScripHost wrapper specific for the context
    // instead of calling toV8() that would create the
    // wrapper in the current context.
    // FIXME: make it possible to use generic bindings factory for InjectedScriptHost.
    v8::Local<v8::Object> scriptHostWrapper = createInjectedScriptHostV8Wrapper(m_injectedScriptHost.get(), inspectedScriptState->isolate());
    if (scriptHostWrapper.IsEmpty())
        return ScriptValue();

    // Inject javascript into the context. The compiled script is supposed to evaluate into
    // a single anonymous function(it's anonymous to avoid cluttering the global object with
    // inspector's stuff) the function is called a few lines below with InjectedScriptHost wrapper,
    // injected script id and explicit reference to the inspected global object. The function is expected
    // to create and configure InjectedScript instance that is going to be used by the inspector.
    v8::Local<v8::Value> value = V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, scriptSource), isolate);
    ASSERT(!value.IsEmpty());
    ASSERT(value->IsFunction());

    v8::Local<v8::Object> windowGlobal = inspectedScriptState->context()->Global();
    v8::Handle<v8::Value> info[] = { scriptHostWrapper, windowGlobal, v8::Number::New(inspectedScriptState->isolate(), id) };
    v8::Local<v8::Value> injectedScriptValue = V8ScriptRunner::callInternalFunction(v8::Local<v8::Function>::Cast(value), windowGlobal, WTF_ARRAY_LENGTH(info), info, inspectedScriptState->isolate());
    return ScriptValue(inspectedScriptState, injectedScriptValue);
}

bool InjectedScriptManager::canAccessInspectedWindow(ScriptState* scriptState)
{
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Object> global = scriptState->context()->Global();
    if (global.IsEmpty())
        return false;
    v8::Handle<v8::Object> holder = V8Window::findInstanceInPrototypeChain(global, scriptState->isolate());
    if (holder.IsEmpty())
        return false;
    LocalFrame* frame = V8Window::toNative(holder)->frame();

    return BindingSecurity::shouldAllowAccessToFrame(scriptState->isolate(), frame, DoNotReportSecurityError);
}

void InjectedScriptManager::setWeakCallback(const v8::WeakCallbackData<v8::Object, InjectedScriptManager::CallbackData>& data)
{
    data.GetParameter()->handle.clear();
    data.GetParameter()->host.clear();
    delete data.GetParameter();
}

} // namespace WebCore
