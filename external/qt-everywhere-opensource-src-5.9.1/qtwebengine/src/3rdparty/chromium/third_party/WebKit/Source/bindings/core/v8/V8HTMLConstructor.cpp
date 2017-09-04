// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HTMLConstructor.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8HTMLElement.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElementRegistry.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

// https://html.spec.whatwg.org/multipage/dom.html#html-element-constructors
void V8HTMLConstructor::htmlConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo& wrapperTypeInfo,
    const HTMLElementType elementInterfaceName) {
  TRACE_EVENT0("blink", "HTMLConstructor");
  DCHECK(info.IsConstructCall());

  v8::Isolate* isolate = info.GetIsolate();
  ScriptState* scriptState = ScriptState::current(isolate);
  v8::Local<v8::Value> newTarget = info.NewTarget();

  if (!scriptState->contextIsValid()) {
    V8ThrowException::throwError(isolate, "The context has been destroyed");
    return;
  }

  if (!RuntimeEnabledFeatures::customElementsV1Enabled() ||
      !scriptState->world().isMainWorld()) {
    V8ThrowException::throwTypeError(isolate, "Illegal constructor");
    return;
  }

  // 2. If NewTarget is equal to the active function object, then
  // throw a TypeError and abort these steps.
  v8::Local<v8::Function> activeFunctionObject =
      scriptState->perContextData()->constructorForType(
          &V8HTMLElement::wrapperTypeInfo);
  if (newTarget == activeFunctionObject) {
    V8ThrowException::throwTypeError(isolate, "Illegal constructor");
    return;
  }

  LocalDOMWindow* window = scriptState->domWindow();
  CustomElementRegistry* registry = window->customElements();

  // 3. Let definition be the entry in registry with constructor equal to
  // NewTarget.
  // If there is no such definition, then throw a TypeError and abort these
  // steps.
  ScriptCustomElementDefinition* definition =
      ScriptCustomElementDefinition::forConstructor(scriptState, registry,
                                                    newTarget);
  if (!definition) {
    V8ThrowException::throwTypeError(isolate, "Illegal constructor");
    return;
  }

  const AtomicString& localName = definition->descriptor().localName();
  const AtomicString& name = definition->descriptor().name();

  if (localName == name) {
    // Autonomous custom element
    // 4.1. If the active function object is not HTMLElement, then throw a
    // TypeError
    if (!V8HTMLElement::wrapperTypeInfo.equals(&wrapperTypeInfo)) {
      V8ThrowException::throwTypeError(isolate,
                                       "Illegal constructor: autonomous custom "
                                       "elements must extend HTMLElement");
      return;
    }
  } else {
    // Customized built-in element
    // 5. If local name is not valid for interface, throw TypeError
    if (htmlElementTypeForTag(localName) != elementInterfaceName) {
      V8ThrowException::throwTypeError(isolate,
                                       "Illegal constructor: localName does "
                                       "not match the HTML element interface");
      return;
    }
  }

  ExceptionState exceptionState(isolate, ExceptionState::ConstructionContext,
                                "HTMLElement");
  v8::TryCatch tryCatch(isolate);

  // 6. Let prototype be Get(NewTarget, "prototype"). Rethrow any exceptions.
  v8::Local<v8::Value> prototype;
  v8::Local<v8::String> prototypeString = v8AtomicString(isolate, "prototype");
  if (!v8Call(newTarget.As<v8::Object>()->Get(scriptState->context(),
                                              prototypeString),
              prototype)) {
    return;
  }

  // 7. If Type(prototype) is not Object, then: ...
  if (!prototype->IsObject()) {
    if (V8PerContextData* perContextData = V8PerContextData::from(
            newTarget.As<v8::Object>()->CreationContext())) {
      prototype =
          perContextData->prototypeForType(&V8HTMLElement::wrapperTypeInfo);
    } else {
      V8ThrowException::throwError(isolate, "The context has been destroyed");
      return;
    }
  }

  // 8. If definition's construction stack is empty...
  Element* element;
  if (definition->constructionStack().isEmpty()) {
    // This is an element being created with 'new' from script
    element = definition->createElementForConstructor(*window->document());
  } else {
    element = definition->constructionStack().last();
    if (element) {
      // This is an element being upgraded that has called super
      definition->constructionStack().last().clear();
    } else {
      // During upgrade an element has invoked the same constructor
      // before calling 'super' and that invocation has poached the
      // element.
      exceptionState.throwDOMException(InvalidStateError,
                                       "this instance is already constructed");
      return;
    }
  }
  const WrapperTypeInfo* wrapperType = element->wrapperTypeInfo();
  v8::Local<v8::Object> wrapper = V8DOMWrapper::associateObjectWithWrapper(
      isolate, element, wrapperType, info.Holder());
  // If the element had a wrapper, we now update and return that
  // instead.
  v8SetReturnValue(info, wrapper);

  // 11. Perform element.[[SetPrototypeOf]](prototype). Rethrow any exceptions.
  // Note: I don't think this prototype set *can* throw exceptions.
  wrapper->SetPrototype(scriptState->context(), prototype.As<v8::Object>())
      .ToChecked();
}
}  // namespace blink
