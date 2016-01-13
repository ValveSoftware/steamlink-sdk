/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8InjectedScriptHost.h"

#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8HTMLAllCollection.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8NodeList.h"
#include "bindings/core/v8/V8Storage.h"
#include "bindings/modules/v8/V8Database.h"
#include "bindings/v8/BindingSecurity.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptDebugServer.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/custom/V8Float32ArrayCustom.h"
#include "bindings/v8/custom/V8Float64ArrayCustom.h"
#include "bindings/v8/custom/V8Int16ArrayCustom.h"
#include "bindings/v8/custom/V8Int32ArrayCustom.h"
#include "bindings/v8/custom/V8Int8ArrayCustom.h"
#include "bindings/v8/custom/V8Uint16ArrayCustom.h"
#include "bindings/v8/custom/V8Uint32ArrayCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "bindings/v8/custom/V8Uint8ClampedArrayCustom.h"
#include "core/events/EventTarget.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "modules/webdatabase/Database.h"
#include "platform/JSONValues.h"

namespace WebCore {

Node* InjectedScriptHost::scriptValueAsNode(ScriptState* scriptState, ScriptValue value)
{
    ScriptState::Scope scope(scriptState);
    if (!value.isObject() || value.isNull())
        return 0;
    return V8Node::toNative(v8::Handle<v8::Object>::Cast(value.v8Value()));
}

ScriptValue InjectedScriptHost::nodeAsScriptValue(ScriptState* scriptState, Node* node)
{
    ScriptState::Scope scope(scriptState);
    v8::Isolate* isolate = scriptState->isolate();
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "nodeAsScriptValue", "InjectedScriptHost", scriptState->context()->Global(), isolate);
    if (!BindingSecurity::shouldAllowAccessToNode(isolate, node, exceptionState))
        return ScriptValue(scriptState, v8::Null(isolate));
    return ScriptValue(scriptState, toV8(node, scriptState->context()->Global(), isolate));
}

void V8InjectedScriptHost::inspectedObjectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    if (!info[0]->IsInt32()) {
        throwTypeError("argument has to be an integer", info.GetIsolate());
        return;
    }

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    InjectedScriptHost::InspectableObject* object = host->inspectedObject(info[0]->ToInt32()->Value());
    v8SetReturnValue(info, object->get(ScriptState::current(info.GetIsolate())).v8Value());
}

void V8InjectedScriptHost::internalConstructorNameMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    if (!info[0]->IsObject())
        return;

    v8SetReturnValue(info, info[0]->ToObject()->GetConstructorName());
}

void V8InjectedScriptHost::isHTMLAllCollectionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    if (!info[0]->IsObject()) {
        v8SetReturnValue(info, false);
        return;
    }

    v8SetReturnValue(info, V8HTMLAllCollection::hasInstance(info[0], info.GetIsolate()));
}

void V8InjectedScriptHost::typeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    v8::Isolate* isolate = info.GetIsolate();

    v8::Handle<v8::Value> value = info[0];
    if (value->IsString()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "string"));
        return;
    }
    if (value->IsArray()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (value->IsBoolean()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "boolean"));
        return;
    }
    if (value->IsNumber()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "number"));
        return;
    }
    if (value->IsDate()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "date"));
        return;
    }
    if (value->IsRegExp()) {
        v8SetReturnValue(info, v8AtomicString(isolate, "regexp"));
        return;
    }
    if (V8Node::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "node"));
        return;
    }
    if (V8NodeList::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (V8HTMLCollection::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (V8Int8Array::hasInstance(value, isolate) || V8Int16Array::hasInstance(value, isolate) || V8Int32Array::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (V8Uint8Array::hasInstance(value, isolate) || V8Uint16Array::hasInstance(value, isolate) || V8Uint32Array::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (V8Float32Array::hasInstance(value, isolate) || V8Float64Array::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
    if (V8Uint8ClampedArray::hasInstance(value, isolate)) {
        v8SetReturnValue(info, v8AtomicString(isolate, "array"));
        return;
    }
}

static bool setFunctionName(v8::Handle<v8::Object>& result, const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
{
    if (value->IsString() && v8::Handle<v8::String>::Cast(value)->Length()) {
        result->Set(v8AtomicString(isolate, "functionName"), value);
        return true;
    }
    return false;
}

void V8InjectedScriptHost::functionDetailsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    v8::Isolate* isolate = info.GetIsolate();

    v8::Handle<v8::Value> value = info[0];
    if (!value->IsFunction())
        return;
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(value);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();

    v8::Local<v8::Object> location = v8::Object::New(isolate);
    location->Set(v8AtomicString(isolate, "lineNumber"), v8::Integer::New(isolate, lineNumber));
    location->Set(v8AtomicString(isolate, "columnNumber"), v8::Integer::New(isolate, columnNumber));
    location->Set(v8AtomicString(isolate, "scriptId"), v8::Integer::New(isolate, function->ScriptId())->ToString());

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result->Set(v8AtomicString(isolate, "location"), location);

    if (!setFunctionName(result, function->GetDisplayName(), isolate)
        && !setFunctionName(result, function->GetName(), isolate)
        && !setFunctionName(result, function->GetInferredName(), isolate))
        result->Set(v8AtomicString(isolate, "functionName"), v8AtomicString(isolate, ""));

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8::Handle<v8::Value> scopes = debugServer.functionScopes(function);
    if (!scopes.IsEmpty() && scopes->IsArray())
        result->Set(v8AtomicString(isolate, "rawScopes"), scopes);

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::getInternalPropertiesMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(info[0]);

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8SetReturnValue(info, debugServer.getInternalProperties(object));
}

static v8::Handle<v8::Array> getJSListenerFunctions(ExecutionContext* executionContext, const EventListenerInfo& listenerInfo, v8::Isolate* isolate)
{
    v8::Local<v8::Array> result = v8::Array::New(isolate);
    size_t handlersCount = listenerInfo.eventListenerVector.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        RefPtr<EventListener> listener = listenerInfo.eventListenerVector[i].listener;
        if (listener->type() != EventListener::JSEventListenerType) {
            ASSERT_NOT_REACHED();
            continue;
        }
        V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener.get());
        v8::Local<v8::Context> context = toV8Context(executionContext, v8Listener->world());
        // Hide listeners from other contexts.
        if (context != isolate->GetCurrentContext())
            continue;
        v8::Local<v8::Object> function;
        {
            // getListenerObject() may cause JS in the event attribute to get compiled, potentially unsuccessfully.
            v8::TryCatch block;
            function = v8Listener->getListenerObject(executionContext);
            if (block.HasCaught())
                continue;
        }
        ASSERT(!function.IsEmpty());
        v8::Local<v8::Object> listenerEntry = v8::Object::New(isolate);
        listenerEntry->Set(v8AtomicString(isolate, "listener"), function);
        listenerEntry->Set(v8AtomicString(isolate, "useCapture"), v8::Boolean::New(isolate, listenerInfo.eventListenerVector[i].useCapture));
        result->Set(v8::Number::New(isolate, outputIndex++), listenerEntry);
    }
    return result;
}

void V8InjectedScriptHost::getEventListenersMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;


    v8::Local<v8::Value> value = info[0];
    EventTarget* target = V8EventTarget::toNativeWithTypeCheck(info.GetIsolate(), value);

    // We need to handle a LocalDOMWindow specially, because a LocalDOMWindow wrapper exists on a prototype chain.
    if (!target)
        target = toDOMWindow(value, info.GetIsolate());

    if (!target || !target->executionContext())
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    Vector<EventListenerInfo> listenersArray;
    host->getEventListenersImpl(target, listenersArray);

    v8::Local<v8::Object> result = v8::Object::New(info.GetIsolate());
    for (size_t i = 0; i < listenersArray.size(); ++i) {
        v8::Handle<v8::Array> listeners = getJSListenerFunctions(target->executionContext(), listenersArray[i], info.GetIsolate());
        if (!listeners->Length())
            continue;
        AtomicString eventType = listenersArray[i].eventType;
        result->Set(v8String(info.GetIsolate(), eventType), listeners);
    }

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::inspectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    ScriptValue object(scriptState, info[0]);
    ScriptValue hints(scriptState, info[1]);
    host->inspectImpl(object.toJSONValue(scriptState), hints.toJSONValue(scriptState));
}

void V8InjectedScriptHost::evaluateMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    if (info.Length() < 1) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "One argument expected.")));
        return;
    }

    v8::Handle<v8::String> expression = info[0]->ToString();
    if (expression.IsEmpty()) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "The argument must be a string.")));
        return;
    }

    ASSERT(isolate->InContext());
    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> result = V8ScriptRunner::compileAndRunInternalScript(expression, info.GetIsolate());
    if (tryCatch.HasCaught()) {
        v8SetReturnValue(info, tryCatch.ReThrow());
        return;
    }
    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::setFunctionVariableValueMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Value> functionValue = info[0];
    int scopeIndex = info[1]->Int32Value();
    String variableName = toCoreStringWithUndefinedOrNullCheck(info[2]);
    v8::Handle<v8::Value> newValue = info[3];

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8SetReturnValue(info, debugServer.setFunctionVariableValue(functionValue, scopeIndex, variableName, newValue));
}

static bool getFunctionLocation(const v8::FunctionCallbackInfo<v8::Value>& info, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (info.Length() < 1)
        return false;
    v8::Handle<v8::Value> fn = info[0];
    if (!fn->IsFunction())
        return false;
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(fn);
    *lineNumber = function->GetScriptLineNumber();
    *columnNumber = function->GetScriptColumnNumber();
    if (*lineNumber == v8::Function::kLineOffsetNotFound || *columnNumber == v8::Function::kLineOffsetNotFound)
        return false;
    *scriptId = String::number(function->ScriptId());
    return true;
}

void V8InjectedScriptHost::debugFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    host->debugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::undebugFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    host->undebugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::monitorFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    v8::Handle<v8::Value> name;
    if (info.Length() > 0 && info[0]->IsFunction()) {
        v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(info[0]);
        name = function->GetName();
        if (!name->IsString() || !v8::Handle<v8::String>::Cast(name)->Length())
            name = function->GetInferredName();
    }

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    host->monitorFunction(scriptId, lineNumber, columnNumber, toCoreStringWithUndefinedOrNullCheck(name));
}

void V8InjectedScriptHost::unmonitorFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    host->unmonitorFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::suppressWarningsAndCallMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2 || !info[0]->IsObject() || !info[1]->IsFunction())
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(info.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    debugServer.muteWarningsAndDeprecations();

    v8::Handle<v8::Object> receiver = v8::Handle<v8::Object>::Cast(info[0]);
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(info[1]);
    size_t argc = info.Length() - 2;
    OwnPtr<v8::Handle<v8::Value>[]> argv = adoptArrayPtr(new v8::Handle<v8::Value>[argc]);
    for (size_t i = 0; i < argc; ++i)
        argv[i] = info[i + 2];

    v8::Local<v8::Value> result = function->Call(receiver, argc, argv.get());
    debugServer.unmuteWarningsAndDeprecations();
    v8SetReturnValue(info, result);
}

} // namespace WebCore
