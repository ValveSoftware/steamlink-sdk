// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8CustomElementRegistry.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElement.h"
#include "core/events/ErrorEvent.h"
#include "core/html/HTMLElement.h"
#include "v8.h"
#include "wtf/Allocator.h"

namespace blink {

// Retrieves the custom elements constructor -> name map, creating it
// if necessary.
static v8::Local<v8::Map> ensureCustomElementRegistryMap(
    ScriptState* scriptState,
    CustomElementRegistry* registry) {
  CHECK(scriptState->world().isMainWorld());
  v8::Local<v8::String> name =
      V8HiddenValue::customElementsRegistryMap(scriptState->isolate());
  v8::Local<v8::Object> wrapper = toV8(registry, scriptState).As<v8::Object>();
  v8::Local<v8::Value> map =
      V8HiddenValue::getHiddenValue(scriptState, wrapper, name);
  if (map.IsEmpty()) {
    map = v8::Map::New(scriptState->isolate());
    V8HiddenValue::setHiddenValue(scriptState, wrapper, name, map);
  }
  return map.As<v8::Map>();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::forConstructor(
    ScriptState* scriptState,
    CustomElementRegistry* registry,
    const v8::Local<v8::Value>& constructor) {
  v8::Local<v8::Map> map =
      ensureCustomElementRegistryMap(scriptState, registry);
  v8::Local<v8::Value> nameValue =
      map->Get(scriptState->context(), constructor).ToLocalChecked();
  if (!nameValue->IsString())
    return nullptr;
  AtomicString name = toCoreAtomicString(nameValue.As<v8::String>());

  // This downcast is safe because only
  // ScriptCustomElementDefinitions have a name associated with a V8
  // constructor in the map; see
  // ScriptCustomElementDefinition::create. This relies on three
  // things:
  //
  // 1. Only ScriptCustomElementDefinition adds entries to the map.
  //    Audit the use of V8HiddenValue/hidden values in general and
  //    how the map is handled--it should never be leaked to script.
  //
  // 2. CustomElementRegistry does not overwrite definitions with a
  //    given name--see the CHECK in CustomElementRegistry::define
  //    --and adds ScriptCustomElementDefinitions to the map without
  //    fail.
  //
  // 3. The relationship between the CustomElementRegistry and its
  //    map is never mixed up; this is guaranteed by the bindings
  //    system which provides a stable wrapper, and the map hangs
  //    off the wrapper.
  //
  // At a meta-level, this downcast is safe because there is
  // currently only one implementation of CustomElementDefinition in
  // product code and that is ScriptCustomElementDefinition. But
  // that may change in the future.
  CustomElementDefinition* definition = registry->definitionForName(name);
  CHECK(definition);
  return static_cast<ScriptCustomElementDefinition*>(definition);
}

template <typename T>
static void keepAlive(v8::Local<v8::Array>& array,
                      uint32_t index,
                      const v8::Local<T>& value,
                      ScopedPersistent<T>& persistent,
                      ScriptState* scriptState) {
  if (value.IsEmpty())
    return;

  array->Set(scriptState->context(), index, value).ToChecked();

  persistent.set(scriptState->isolate(), value);
  persistent.setPhantom();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::create(
    ScriptState* scriptState,
    CustomElementRegistry* registry,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Function>& connectedCallback,
    const v8::Local<v8::Function>& disconnectedCallback,
    const v8::Local<v8::Function>& adoptedCallback,
    const v8::Local<v8::Function>& attributeChangedCallback,
    const HashSet<AtomicString>& observedAttributes) {
  ScriptCustomElementDefinition* definition = new ScriptCustomElementDefinition(
      scriptState, descriptor, constructor, connectedCallback,
      disconnectedCallback, adoptedCallback, attributeChangedCallback,
      observedAttributes);

  // Add a constructor -> name mapping to the registry.
  v8::Local<v8::Value> nameValue =
      v8String(scriptState->isolate(), descriptor.name());
  v8::Local<v8::Map> map =
      ensureCustomElementRegistryMap(scriptState, registry);
  map->Set(scriptState->context(), constructor, nameValue).ToLocalChecked();
  definition->m_constructor.setPhantom();

  // We add the callbacks here to keep them alive. We use the name as
  // the key because it is unique per-registry.
  v8::Local<v8::Array> array = v8::Array::New(scriptState->isolate(), 5);
  keepAlive(array, 0, connectedCallback, definition->m_connectedCallback,
            scriptState);
  keepAlive(array, 1, disconnectedCallback, definition->m_disconnectedCallback,
            scriptState);
  keepAlive(array, 2, adoptedCallback, definition->m_adoptedCallback,
            scriptState);
  keepAlive(array, 3, attributeChangedCallback,
            definition->m_attributeChangedCallback, scriptState);
  map->Set(scriptState->context(), nameValue, array).ToLocalChecked();

  return definition;
}

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    ScriptState* scriptState,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Function>& connectedCallback,
    const v8::Local<v8::Function>& disconnectedCallback,
    const v8::Local<v8::Function>& adoptedCallback,
    const v8::Local<v8::Function>& attributeChangedCallback,
    const HashSet<AtomicString>& observedAttributes)
    : CustomElementDefinition(descriptor, observedAttributes),
      m_scriptState(scriptState),
      m_constructor(scriptState->isolate(), constructor) {}

static void dispatchErrorEvent(v8::Isolate* isolate,
                               v8::Local<v8::Value> exception,
                               v8::Local<v8::Object> constructor) {
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);
  V8ScriptRunner::throwException(
      isolate, exception, constructor.As<v8::Function>()->GetScriptOrigin());
}

HTMLElement* ScriptCustomElementDefinition::handleCreateElementSyncException(
    Document& document,
    const QualifiedName& tagName,
    v8::Isolate* isolate,
    ExceptionState& exceptionState) {
  DCHECK(exceptionState.hadException());
  // 6.1."If any of these subsubsteps threw an exception".1
  // Report the exception.
  dispatchErrorEvent(isolate, exceptionState.getException(), constructor());
  exceptionState.clearException();
  // ... .2 Return HTMLUnknownElement.
  return CustomElement::createFailedElement(document, tagName);
}

HTMLElement* ScriptCustomElementDefinition::createElementSync(
    Document& document,
    const QualifiedName& tagName) {
  if (!m_scriptState->contextIsValid())
    return CustomElement::createFailedElement(document, tagName);
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();

  ExceptionState exceptionState(ExceptionState::ConstructionContext,
                                "CustomElement", constructor(), isolate);

  // Create an element with the synchronous custom elements flag set.
  // https://dom.spec.whatwg.org/#concept-create-element

  // Create an element and push to the construction stack.
  // V8HTMLElement::constructorCustom() can only refer to
  // window.document(), but it is different from the document here
  // when it is an import document. This is not exactly what the
  // spec defines, but the non-imports behavior matches to the spec.
  Element* element = createElementForConstructor(document);
  {
    ConstructionStackScope constructionStackScope(this, element);
    v8::TryCatch tryCatch(m_scriptState->isolate());
    element = runConstructor();
    if (tryCatch.HasCaught()) {
      exceptionState.rethrowV8Exception(tryCatch.Exception());
      return handleCreateElementSyncException(document, tagName, isolate,
                                              exceptionState);
    }
  }

  // 6.1.3. through 6.1.9.
  checkConstructorResult(element, document, tagName, exceptionState);
  if (exceptionState.hadException()) {
    return handleCreateElementSyncException(document, tagName, isolate,
                                            exceptionState);
  }
  DCHECK_EQ(element->getCustomElementState(), CustomElementState::Custom);
  return toHTMLElement(element);
}

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
bool ScriptCustomElementDefinition::runConstructor(Element* element) {
  if (!m_scriptState->contextIsValid())
    return false;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();

  // Step 5 says to rethrow the exception; but there is no one to
  // catch it. The side effect is to report the error.
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);

  Element* result = runConstructor();

  // To report exception thrown from runConstructor()
  if (tryCatch.HasCaught())
    return false;

  // To report InvalidStateError Exception, when the constructor returns some
  // different object
  if (result != element) {
    const String& message =
        "custom element constructors must call super() first and must "
        "not return a different object";
    v8::Local<v8::Value> exception = V8ThrowException::createDOMException(
        m_scriptState->isolate(), InvalidStateError, message);
    dispatchErrorEvent(isolate, exception, constructor());
    return false;
  }

  return true;
}

Element* ScriptCustomElementDefinition::runConstructor() {
  v8::Isolate* isolate = m_scriptState->isolate();
  DCHECK(ScriptState::current(isolate) == m_scriptState);
  ExecutionContext* executionContext = m_scriptState->getExecutionContext();
  v8::Local<v8::Value> result;
  if (!v8Call(V8ScriptRunner::callAsConstructor(isolate, constructor(),
                                                executionContext, 0, nullptr),
              result)) {
    return nullptr;
  }
  return V8Element::toImplWithTypeCheck(isolate, result);
}

v8::Local<v8::Object> ScriptCustomElementDefinition::constructor() const {
  DCHECK(!m_constructor.isEmpty());
  return m_constructor.newLocal(m_scriptState->isolate());
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::getConstructorForScript() {
  return ScriptValue(m_scriptState.get(), constructor());
}

bool ScriptCustomElementDefinition::hasConnectedCallback() const {
  return !m_connectedCallback.isEmpty();
}

bool ScriptCustomElementDefinition::hasDisconnectedCallback() const {
  return !m_disconnectedCallback.isEmpty();
}

bool ScriptCustomElementDefinition::hasAdoptedCallback() const {
  return !m_adoptedCallback.isEmpty();
}

void ScriptCustomElementDefinition::runCallback(
    v8::Local<v8::Function> callback,
    Element* element,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(ScriptState::current(m_scriptState->isolate()) == m_scriptState);
  v8::Isolate* isolate = m_scriptState->isolate();

  // Invoke custom element reactions
  // https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
  // If this throws any exception, then report the exception.
  v8::TryCatch tryCatch(isolate);
  tryCatch.SetVerbose(true);

  ExecutionContext* executionContext = m_scriptState->getExecutionContext();
  v8::Local<v8::Value> elementHandle =
      toV8(element, m_scriptState->context()->Global(), isolate);
  V8ScriptRunner::callFunction(callback, executionContext, elementHandle, argc,
                               argv, isolate);
}

void ScriptCustomElementDefinition::runConnectedCallback(Element* element) {
  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();
  runCallback(m_connectedCallback.newLocal(isolate), element);
}

void ScriptCustomElementDefinition::runDisconnectedCallback(Element* element) {
  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();
  runCallback(m_disconnectedCallback.newLocal(isolate), element);
}

void ScriptCustomElementDefinition::runAdoptedCallback(Element* element,
                                                       Document* oldOwner,
                                                       Document* newOwner) {
  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::Value> argv[] = {
      toV8(oldOwner, m_scriptState->context()->Global(), isolate),
      toV8(newOwner, m_scriptState->context()->Global(), isolate)};
  runCallback(m_adoptedCallback.newLocal(isolate), element,
              WTF_ARRAY_LENGTH(argv), argv);
}

void ScriptCustomElementDefinition::runAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& oldValue,
    const AtomicString& newValue) {
  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::Value> argv[] = {
      v8String(isolate, name.localName()), v8StringOrNull(isolate, oldValue),
      v8StringOrNull(isolate, newValue),
      v8StringOrNull(isolate, name.namespaceURI()),
  };
  runCallback(m_attributeChangedCallback.newLocal(isolate), element,
              WTF_ARRAY_LENGTH(argv), argv);
}

}  // namespace blink
