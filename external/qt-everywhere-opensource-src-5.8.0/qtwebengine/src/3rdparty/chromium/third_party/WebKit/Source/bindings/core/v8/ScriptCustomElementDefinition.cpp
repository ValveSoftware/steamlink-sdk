// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8CustomElementsRegistry.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "v8.h"
#include "wtf/Allocator.h"

namespace blink {

// Retrieves the custom elements constructor -> name map, creating it
// if necessary. The same map is used to keep prototypes alive.
static v8::Local<v8::Map> ensureCustomElementsRegistryMap(
    ScriptState* scriptState,
    CustomElementsRegistry* registry)
{
    CHECK(scriptState->world().isMainWorld());
    v8::Local<v8::String> name = V8HiddenValue::customElementsRegistryMap(
        scriptState->isolate());
    v8::Local<v8::Object> wrapper =
        toV8(registry, scriptState).As<v8::Object>();
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
    CustomElementsRegistry* registry,
    const v8::Local<v8::Value>& constructor)
{
    v8::Local<v8::Map> map =
        ensureCustomElementsRegistryMap(scriptState, registry);
    v8::Local<v8::Value> nameValue = v8CallOrCrash(
        map->Get(scriptState->context(), constructor));
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
    // 2. CustomElementsRegistry does not overwrite definitions with a
    //    given name--see the CHECK in CustomElementsRegistry::define
    //    --and adds ScriptCustomElementDefinitions to the map without
    //    fail.
    //
    // 3. The relationship between the CustomElementsRegistry and its
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
static void keepAlive(v8::Local<v8::Array>& array, uint32_t index,
    const v8::Local<T>& value,
    ScopedPersistent<T>& persistent,
    ScriptState* scriptState)
{
    if (value.IsEmpty())
        return;

    v8CallOrCrash(array->Set(scriptState->context(), index, value));

    persistent.set(scriptState->isolate(), value);
    persistent.setPhantom();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::create(
    ScriptState* scriptState,
    CustomElementsRegistry* registry,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Object>& prototype,
    const v8::Local<v8::Function>& connectedCallback,
    const v8::Local<v8::Function>& disconnectedCallback,
    const v8::Local<v8::Function>& attributeChangedCallback,
    const HashSet<AtomicString>& observedAttributes)
{
    ScriptCustomElementDefinition* definition =
        new ScriptCustomElementDefinition(
            scriptState,
            descriptor,
            constructor,
            prototype,
            connectedCallback,
            disconnectedCallback,
            attributeChangedCallback,
            observedAttributes);

    // Add a constructor -> name mapping to the registry.
    v8::Local<v8::Value> nameValue =
        v8String(scriptState->isolate(), descriptor.name());
    v8::Local<v8::Map> map =
        ensureCustomElementsRegistryMap(scriptState, registry);
    v8CallOrCrash(map->Set(scriptState->context(), constructor, nameValue));
    definition->m_constructor.setPhantom();

    // We add the prototype and callbacks here to keep them alive. We use the
    // name as the key because it is unique per-registry.
    v8::Local<v8::Array> array = v8::Array::New(scriptState->isolate(), 4);
    keepAlive(array, 0, prototype, definition->m_prototype, scriptState);
    keepAlive(array, 1, connectedCallback, definition->m_connectedCallback, scriptState);
    keepAlive(array, 2, disconnectedCallback, definition->m_disconnectedCallback, scriptState);
    keepAlive(array, 3, attributeChangedCallback, definition->m_attributeChangedCallback, scriptState);
    v8CallOrCrash(map->Set(scriptState->context(), nameValue, array));

    return definition;
}

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    ScriptState* scriptState,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Object>& prototype,
    const v8::Local<v8::Function>& connectedCallback,
    const v8::Local<v8::Function>& disconnectedCallback,
    const v8::Local<v8::Function>& attributeChangedCallback,
    const HashSet<AtomicString>& observedAttributes)
    : CustomElementDefinition(descriptor, observedAttributes)
    , m_scriptState(scriptState)
    , m_constructor(scriptState->isolate(), constructor)
{
}

HTMLElement* ScriptCustomElementDefinition::createElementSync(
    Document& document, const QualifiedName& tagName,
    ExceptionState& exceptionState)
{
    DCHECK(ScriptState::current(m_scriptState->isolate()) == m_scriptState);

    // Create an element
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null
    // 6.1. If the synchronous custom elements flag is set:
    // 6.1.2. Set result to Construct(C). Rethrow any exceptions.
    Element* element = nullptr;
    {
        v8::TryCatch tryCatch(m_scriptState->isolate());
        element = runConstructor();
        if (tryCatch.HasCaught()) {
            exceptionState.rethrowV8Exception(tryCatch.Exception());
            return nullptr;
        }
    }

    // 6.1.3. through 6.1.9.
    checkConstructorResult(element, document, tagName, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    DCHECK_EQ(element->getCustomElementState(), CustomElementState::Custom);
    return toHTMLElement(element);
}

HTMLElement* ScriptCustomElementDefinition::createElementSync(
    Document& document, const QualifiedName& tagName)
{
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();

    // When invoked from "create an element for a token":
    // https://html.spec.whatwg.org/multipage/syntax.html#create-an-element-for-the-token

    ExceptionState exceptionState(ExceptionState::ConstructionContext,
        "CustomElement", constructor(), isolate);
    HTMLElement* element = createElementSync(document, tagName, exceptionState);

    if (exceptionState.hadException() || !element) {
        // 7. If this step throws an exception, then report the exception, ...
        {
            v8::TryCatch tryCatch(isolate);
            tryCatch.SetVerbose(true);
            exceptionState.throwIfNeeded();
        }

        // ...and let element be instead a new element that implements
        // HTMLUnknownElement, with no attributes, namespace set to given
        // namespace, namespace prefix set to null, custom element state
        // "undefined", and node document set to document.
        element = HTMLUnknownElement::create(tagName, document);
        element->setCustomElementState(CustomElementState::Undefined);
    }
    return element;
}

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
bool ScriptCustomElementDefinition::runConstructor(Element* element)
{
    if (!m_scriptState->contextIsValid())
        return false;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();

    // Step 5 says to rethrow the exception; but there is no one to
    // catch it. The side effect is to report the error.
    v8::TryCatch tryCatch(isolate);
    tryCatch.SetVerbose(true);

    Element* result = runConstructor();
    if (!result)
        return false;

    if (result != element) {
        V8ThrowException::throwException(
            V8ThrowException::createDOMException(
                m_scriptState->isolate(),
                InvalidStateError,
                "custom element constructors must call super() first and must "
                "not return a different object",
                constructor()),
            m_scriptState->isolate());
        return false;
    }

    return true;
}

Element* ScriptCustomElementDefinition::runConstructor()
{
    v8::Isolate* isolate = m_scriptState->isolate();
    DCHECK(ScriptState::current(isolate) == m_scriptState);
    ExecutionContext* executionContext = m_scriptState->getExecutionContext();
    v8::Local<v8::Value> result;
    if (!v8Call(V8ScriptRunner::callAsConstructor(
        isolate,
        constructor(),
        executionContext,
        0,
        nullptr),
        result)) {
        return nullptr;
    }
    return V8Element::toImplWithTypeCheck(isolate, result);
}

v8::Local<v8::Object> ScriptCustomElementDefinition::constructor() const
{
    DCHECK(!m_constructor.isEmpty());
    return m_constructor.newLocal(m_scriptState->isolate());
}

v8::Local<v8::Object> ScriptCustomElementDefinition::prototype() const
{
    DCHECK(!m_prototype.isEmpty());
    return m_prototype.newLocal(m_scriptState->isolate());
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::getConstructorForScript()
{
    return ScriptValue(m_scriptState.get(), constructor());
}

bool ScriptCustomElementDefinition::hasConnectedCallback() const
{
    return !m_connectedCallback.isEmpty();
}

bool ScriptCustomElementDefinition::hasDisconnectedCallback() const
{
    return !m_disconnectedCallback.isEmpty();
}

void ScriptCustomElementDefinition::runCallback(
    v8::Local<v8::Function> callback,
    Element* element, int argc, v8::Local<v8::Value> argv[])
{
    DCHECK(ScriptState::current(m_scriptState->isolate()) == m_scriptState);
    v8::Isolate* isolate = m_scriptState->isolate();

    // Invoke custom element reactions
    // https://html.spec.whatwg.org/multipage/scripting.html#invoke-custom-element-reactions
    // If this throws any exception, then report the exception.
    v8::TryCatch tryCatch(isolate);
    tryCatch.SetVerbose(true);

    ExecutionContext* executionContext = m_scriptState->getExecutionContext();
    v8::Local<v8::Value> elementHandle = toV8(element,
        m_scriptState->context()->Global(), isolate);
    V8ScriptRunner::callFunction(
        callback,
        executionContext,
        elementHandle,
        argc, argv, isolate);
}

void ScriptCustomElementDefinition::runConnectedCallback(Element* element)
{
    if (!m_scriptState->contextIsValid())
        return;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();
    runCallback(m_connectedCallback.newLocal(isolate), element);
}

void ScriptCustomElementDefinition::runDisconnectedCallback(Element* element)
{
    if (!m_scriptState->contextIsValid())
        return;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();
    runCallback(m_disconnectedCallback.newLocal(isolate), element);
}

void ScriptCustomElementDefinition::runAttributeChangedCallback(
    Element* element, const QualifiedName& name,
    const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!m_scriptState->contextIsValid())
        return;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();
    const int argc = 4;
    v8::Local<v8::Value> argv[argc] = {
        v8String(isolate, name.localName()),
        v8StringOrNull(isolate, oldValue),
        v8StringOrNull(isolate, newValue),
        v8String(isolate, name.namespaceURI()),
    };
    runCallback(m_attributeChangedCallback.newLocal(isolate), element,
        argc, argv);
}

} // namespace blink
