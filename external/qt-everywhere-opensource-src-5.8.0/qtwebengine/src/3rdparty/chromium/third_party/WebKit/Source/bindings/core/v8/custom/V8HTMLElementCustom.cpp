// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HTMLElement.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void V8HTMLElement::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DCHECK(info.IsConstructCall());

    v8::Isolate* isolate = info.GetIsolate();
    ScriptState* scriptState = ScriptState::current(isolate);

    if (!RuntimeEnabledFeatures::customElementsV1Enabled()
        || !scriptState->world().isMainWorld()) {
        V8ThrowException::throwTypeError(
            info.GetIsolate(),
            "Illegal constructor");
        return;
    }

    LocalDOMWindow* window = scriptState->domWindow();
    ScriptCustomElementDefinition* definition =
        ScriptCustomElementDefinition::forConstructor(
            scriptState,
            window->customElements(),
            info.NewTarget());
    if (!definition) {
        V8ThrowException::throwTypeError(isolate, "Illegal constructor");
        return;
    }

    ExceptionState exceptionState(
        ExceptionState::ConstructionContext,
        "HTMLElement",
        info.Holder(),
        isolate);

    Element* element;
    if (definition->constructionStack().isEmpty()) {
        // This is an element being created with 'new' from script
        // TODO(kojii): When HTMLElementFactory has an option not to queue
        // upgrade, call that instead of HTMLElement. HTMLElement is enough
        // for now, but type extension will require HTMLElementFactory.
        element = HTMLElement::create(
            QualifiedName(nullAtom, definition->descriptor().localName(), HTMLNames::xhtmlNamespaceURI),
            *window->document());
        element->setCustomElementState(CustomElementState::Undefined);
    } else {
        element = definition->constructionStack().last();
        if (element) {
            // This is an element being upgraded that has called super
            definition->constructionStack().last().clear();
        } else {
            // During upgrade an element has invoked the same constructor
            // before calling 'super' and that invocation has poached the
            // element.
            exceptionState.throwDOMException(
                InvalidStateError,
                "this instance is already constructed");
            exceptionState.throwIfNeeded();
            return;
        }
    }
    const WrapperTypeInfo* wrapperType = element->wrapperTypeInfo();
    v8::Local<v8::Object> wrapper = V8DOMWrapper::associateObjectWithWrapper(
        isolate,
        element,
        wrapperType,
        info.Holder());
    // If the element had a wrapper, we now update and return that
    // instead.
    v8SetReturnValue(info, wrapper);

    v8CallOrCrash(wrapper->SetPrototype(
        scriptState->context(),
        definition->prototype()));

    // TODO(dominicc): Move this to the exactly correct place when
    // https://github.com/whatwg/html/issues/1297 is closed.
    element->setCustomElementState(CustomElementState::Custom);
}

} // namespace blink
