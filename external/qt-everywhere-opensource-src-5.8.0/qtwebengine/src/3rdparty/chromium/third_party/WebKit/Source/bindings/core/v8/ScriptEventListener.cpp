/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptEventListener.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/QualifiedName.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include <v8.h>

namespace blink {

V8LazyEventListener* createAttributeEventListener(Node* node, const QualifiedName& name, const AtomicString& value, const AtomicString& eventParameterName)
{
    ASSERT(node);
    if (value.isNull())
        return nullptr;

    // FIXME: Very strange: we initialize zero-based number with '1'.
    TextPosition position(OrdinalNumber::fromZeroBasedInt(1), OrdinalNumber::first());
    String sourceURL;

    v8::Isolate* isolate;
    if (LocalFrame* frame = node->document().frame()) {
        isolate = toIsolate(frame);
        ScriptController& scriptController = frame->script();
        if (!scriptController.canExecuteScripts(AboutToExecuteScript))
            return nullptr;
        position = scriptController.eventHandlerPosition();
        sourceURL = node->document().url().getString();
    } else {
        isolate = v8::Isolate::GetCurrent();
    }

    return V8LazyEventListener::create(name.localName(), eventParameterName, value, sourceURL, position, node, isolate);
}

V8LazyEventListener* createAttributeEventListener(LocalFrame* frame, const QualifiedName& name, const AtomicString& value, const AtomicString& eventParameterName)
{
    if (!frame)
        return nullptr;

    if (value.isNull())
        return nullptr;

    ScriptController& scriptController = frame->script();
    if (!scriptController.canExecuteScripts(AboutToExecuteScript))
        return nullptr;

    TextPosition position = scriptController.eventHandlerPosition();
    String sourceURL = frame->document()->url().getString();

    return V8LazyEventListener::create(name.localName(), eventParameterName, value, sourceURL, position, 0, toIsolate(frame));
}

v8::Local<v8::Object> eventListenerHandler(ExecutionContext* executionContext, EventListener* listener)
{
    if (listener->type() != EventListener::JSEventListenerType)
        return v8::Local<v8::Object>();
    V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener);
    return v8Listener->getListenerObject(executionContext);
}

v8::Local<v8::Function> eventListenerEffectiveFunction(v8::Isolate* isolate, v8::Local<v8::Object> handler)
{
    v8::Local<v8::Function> function;
    if (handler->IsFunction()) {
        function = handler.As<v8::Function>();
    } else if (handler->IsObject()) {
        v8::Local<v8::Value> property;
        // Try the "handleEvent" method (EventListener interface).
        if (handler->Get(handler->CreationContext(), v8AtomicString(isolate, "handleEvent")).ToLocal(&property) && property->IsFunction())
            function = property.As<v8::Function>();
        // Fall back to the "constructor" property.
        else if (handler->Get(handler->CreationContext(), v8AtomicString(isolate, "constructor")).ToLocal(&property) && property->IsFunction())
            function = property.As<v8::Function>();
    }
    if (!function.IsEmpty())
        return getBoundFunction(function);
    return v8::Local<v8::Function>();
}

void getFunctionLocation(v8::Local<v8::Function> function, String& scriptId, int& lineNumber, int& columnNumber)
{
    int scriptIdValue = function->ScriptId();
    scriptId = String::number(scriptIdValue);
    lineNumber = function->GetScriptLineNumber();
    columnNumber = function->GetScriptColumnNumber();
}

} // namespace blink
