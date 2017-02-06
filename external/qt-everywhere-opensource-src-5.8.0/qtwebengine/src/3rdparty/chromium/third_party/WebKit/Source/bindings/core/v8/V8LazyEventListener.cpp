/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8LazyEventListener.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8HTMLFormElement.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormElement.h"
#include "wtf/StdLibExtras.h"

namespace blink {

V8LazyEventListener::V8LazyEventListener(v8::Isolate* isolate, const AtomicString& functionName, const AtomicString& eventParameterName, const String& code, const String sourceURL, const TextPosition& position, Node* node)
    : V8AbstractEventListener(true, DOMWrapperWorld::mainWorld(), isolate)
    , m_functionName(functionName)
    , m_eventParameterName(eventParameterName)
    , m_code(code)
    , m_sourceURL(sourceURL)
    , m_node(node)
    , m_position(position)
{
}

template<typename T>
v8::Local<v8::Object> toObjectWrapper(T* domObject, ScriptState* scriptState)
{
    if (!domObject)
        return v8::Object::New(scriptState->isolate());
    v8::Local<v8::Value> value = toV8(domObject, scriptState->context()->Global(), scriptState->isolate());
    if (value.IsEmpty())
        return v8::Object::New(scriptState->isolate());
    return v8::Local<v8::Object>::New(scriptState->isolate(), value.As<v8::Object>());
}

v8::Local<v8::Value> V8LazyEventListener::callListenerFunction(ScriptState* scriptState, v8::Local<v8::Value> jsEvent, Event* event)
{
    ASSERT(!jsEvent.IsEmpty());
    v8::Local<v8::Object> listenerObject = getListenerObject(scriptState->getExecutionContext());
    if (listenerObject.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Local<v8::Function> handlerFunction = listenerObject.As<v8::Function>();
    v8::Local<v8::Object> receiver = getReceiverObject(scriptState, event);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    if (!scriptState->getExecutionContext()->isDocument())
        return v8::Local<v8::Value>();

    LocalFrame* frame = toDocument(scriptState->getExecutionContext())->frame();
    if (!frame)
        return v8::Local<v8::Value>();

    if (!frame->script().canExecuteScripts(AboutToExecuteScript))
        return v8::Local<v8::Value>();

    v8::Local<v8::Value> parameters[1] = { jsEvent };
    v8::Local<v8::Value> result;
    if (!frame->script().callFunction(handlerFunction, receiver, WTF_ARRAY_LENGTH(parameters), parameters).ToLocal(&result))
        return v8::Local<v8::Value>();
    return result;
}

static void V8LazyEventListenerToString(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8SetReturnValue(info, V8HiddenValue::getHiddenValue(ScriptState::current(info.GetIsolate()), info.Holder(), V8HiddenValue::toStringString(info.GetIsolate())));
}

void V8LazyEventListener::prepareListenerObject(ExecutionContext* executionContext)
{
    if (!executionContext)
        return;

    // A ScriptState used by the event listener needs to be calculated based on
    // the ExecutionContext that fired the the event listener and the world
    // that installed the event listener.
    v8::HandleScope handleScope(toIsolate(executionContext));
    v8::Local<v8::Context> v8Context = toV8Context(executionContext, world());
    if (v8Context.IsEmpty())
        return;
    ScriptState* scriptState = ScriptState::from(v8Context);
    if (!scriptState->contextIsValid())
        return;

    if (!executionContext->isDocument())
        return;

    if (!toDocument(executionContext)->allowInlineEventHandler(m_node, this, m_sourceURL, m_position.m_line)) {
        clearListenerObject();
        return;
    }

    if (hasExistingListenerObject())
        return;

    ScriptState::Scope scope(scriptState);

    // Nodes other than the document object, when executing inline event
    // handlers push document, form owner, and the target node on the scope chain.
    // We do this by using 'with' statement.
    // See fast/forms/form-action.html
    //     fast/forms/selected-index-value.html
    //     fast/overflow/onscroll-layer-self-destruct.html
    HTMLFormElement* formElement = 0;
    if (m_node && m_node->isHTMLElement())
        formElement = toHTMLElement(m_node)->formOwner();

    v8::Local<v8::Object> scopes[3];

    scopes[2] = toObjectWrapper<Node>(m_node, scriptState);
    scopes[1] = toObjectWrapper<HTMLFormElement>(formElement, scriptState);
    scopes[0] = toObjectWrapper<Document>(m_node ? m_node->ownerDocument() : 0, scriptState);

    v8::Local<v8::String> parameterName = v8String(isolate(), m_eventParameterName);
    v8::ScriptOrigin origin(
        v8String(isolate(), m_sourceURL),
        v8::Integer::New(isolate(), m_position.m_line.zeroBasedInt()),
        v8::Integer::New(isolate(), m_position.m_column.zeroBasedInt()),
        v8::True(isolate()),
        v8::Local<v8::Integer>(),
        v8::True(isolate()));
    v8::ScriptCompiler::Source source(v8String(isolate(), m_code), origin);

    v8::Local<v8::Function> wrappedFunction;
    {
        // JavaScript compilation error shouldn't be reported as a runtime
        // exception because we're not running any program code.  Instead,
        // it should be reported as an ErrorEvent.
        v8::TryCatch block(isolate());
        wrappedFunction = v8::ScriptCompiler::CompileFunctionInContext(isolate(), &source, v8Context, 1, &parameterName, 3, scopes);
        if (block.HasCaught()) {
            fireErrorEvent(v8Context, executionContext, block.Message());
            return;
        }
    }

    // Change the toString function on the wrapper function to avoid it
    // returning the source for the actual wrapper function. Instead it
    // returns source for a clean wrapper function with the event
    // argument wrapping the event source code. The reason for this is
    // that some web sites use toString on event functions and eval the
    // source returned (sometimes a RegExp is applied as well) for some
    // other use. That fails miserably if the actual wrapper source is
    // returned.
    v8::Local<v8::Function> toStringFunction;
    if (!v8::Function::New(scriptState->context(), V8LazyEventListenerToString, v8::Local<v8::Value>(), 0, v8::ConstructorBehavior::kThrow).ToLocal(&toStringFunction))
        return;
    String toStringString = "function " + m_functionName + "(" + m_eventParameterName + ") {\n  " + m_code + "\n}";
    V8HiddenValue::setHiddenValue(scriptState, wrappedFunction, V8HiddenValue::toStringString(isolate()), v8String(isolate(), toStringString));
    if (!v8CallBoolean(wrappedFunction->CreateDataProperty(scriptState->context(), v8AtomicString(isolate(), "toString"), toStringFunction)))
        return;
    wrappedFunction->SetName(v8String(isolate(), m_functionName));

    // FIXME: Remove the following comment-outs.
    // See https://bugs.webkit.org/show_bug.cgi?id=85152 for more details.
    //
    // For the time being, we comment out the following code since the
    // second parsing can happen.
    // // Since we only parse once, there's no need to keep data used for parsing around anymore.
    // m_functionName = String();
    // m_code = String();
    // m_eventParameterName = String();
    // m_sourceURL = String();
    setListenerObject(wrappedFunction);
}

void V8LazyEventListener::fireErrorEvent(v8::Local<v8::Context> v8Context, ExecutionContext* executionContext, v8::Local<v8::Message> message)
{
    ErrorEvent* event = ErrorEvent::create(toCoreStringWithNullCheck(message->Get()), SourceLocation::fromMessage(isolate(), message, executionContext), &world());

    AccessControlStatus accessControlStatus = NotSharableCrossOrigin;
    if (message->IsOpaque())
        accessControlStatus = OpaqueResource;
    else if (message->IsSharedCrossOrigin())
        accessControlStatus = SharableCrossOrigin;

    executionContext->reportException(event, accessControlStatus);
}

} // namespace blink
