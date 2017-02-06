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

#include "bindings/core/v8/V8CustomEvent.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8CustomEventInit.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/ContextFeatures.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

static void storeDetail(ScriptState* scriptState, CustomEvent* impl, v8::Local<v8::Object> wrapper, v8::Local<v8::Value> detail)
{
    auto privateDetail = V8PrivateProperty::getCustomEventDetail(scriptState->isolate());
    privateDetail.set(scriptState->context(), wrapper, detail);

    // When a custom event is created in an isolated world, serialize
    // |detail| and store it in |impl| so that we can clone |detail|
    // when the getter of |detail| is called in the main world later.
    if (scriptState->world().isIsolatedWorld())
        impl->setSerializedDetail(SerializedScriptValue::serializeAndSwallowExceptions(scriptState->isolate(), detail));
}

void V8CustomEvent::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "CustomEvent", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < 1)) {
        setMinimumArityTypeError(exceptionState, 1, info.Length());
        exceptionState.throwIfNeeded();
        return;
    }

    V8StringResource<> type = info[0];
    if (!type.prepare())
        return;

    CustomEventInit eventInitDict;
    if (!isUndefinedOrNull(info[1])) {
        if (!info[1]->IsObject()) {
            exceptionState.throwTypeError("parameter 2 ('eventInitDict') is not an object.");
            exceptionState.throwIfNeeded();
            return;
        }
        V8CustomEventInit::toImpl(info.GetIsolate(), info[1], eventInitDict, exceptionState);
        if (exceptionState.throwIfNeeded())
            return;
    }

    CustomEvent* impl = CustomEvent::create(type, eventInitDict);
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->associateWithWrapper(info.GetIsolate(), &V8CustomEvent::wrapperTypeInfo, wrapper);

    // TODO(bashi): Workaround for http://crbug.com/529941. We need to store
    // |detail| as a private property to avoid cycle references.
    if (eventInitDict.hasDetail()) {
        v8::Local<v8::Value> v8Detail = eventInitDict.detail().v8Value();
        storeDetail(ScriptState::current(info.GetIsolate()), impl, wrapper, v8Detail);
    }
    v8SetReturnValue(info, wrapper);
}

void V8CustomEvent::initCustomEventMethodEpilogueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, CustomEvent* impl)
{
    ASSERT(info.Length() >= 3);
    v8::Local<v8::Value> detail = info[3];
    if (!detail.IsEmpty())
        storeDetail(ScriptState::current(info.GetIsolate()), impl, info.Holder(), detail);
}

void V8CustomEvent::detailAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    CustomEvent* event = V8CustomEvent::toImpl(info.Holder());
    ScriptState* scriptState = ScriptState::current(info.GetIsolate());

    auto privateDetail = V8PrivateProperty::getCustomEventDetail(info.GetIsolate());
    v8::Local<v8::Value> detail = privateDetail.get(scriptState->context(), info.Holder());
    if (!detail.IsEmpty()) {
        v8SetReturnValue(info, detail);
        return;
    }

    // Be careful not to return a V8 value which is created in different world.
    if (SerializedScriptValue* serializedValue = event->serializedDetail()) {
        detail = serializedValue->deserialize();
    } else if (scriptState->world().isIsolatedWorld()) {
        v8::Local<v8::Value> mainWorldDetail = privateDetail.getFromMainWorld(scriptState, event);
        if (!mainWorldDetail.IsEmpty()) {
            event->setSerializedDetail(SerializedScriptValue::serializeAndSwallowExceptions(info.GetIsolate(), mainWorldDetail));
            detail = event->serializedDetail()->deserialize();
        }
    }

    // |detail| should be null when it is an empty handle because its default value is null.
    if (detail.IsEmpty())
        detail = v8::Null(info.GetIsolate());
    privateDetail.set(scriptState->context(), info.Holder(), detail);
    v8SetReturnValue(info, detail);
}

} // namespace blink
