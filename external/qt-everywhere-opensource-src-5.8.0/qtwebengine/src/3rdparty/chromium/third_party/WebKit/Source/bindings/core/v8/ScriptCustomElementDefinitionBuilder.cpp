// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

ScriptCustomElementDefinitionBuilder* ScriptCustomElementDefinitionBuilder
    ::s_stack = nullptr;

ScriptCustomElementDefinitionBuilder::ScriptCustomElementDefinitionBuilder(
    ScriptState* scriptState,
    CustomElementsRegistry* registry,
    const ScriptValue& constructor,
    ExceptionState& exceptionState)
    : m_prev(s_stack)
    , m_scriptState(scriptState)
    , m_registry(registry)
    , m_constructorValue(constructor.v8Value())
    , m_exceptionState(exceptionState)
{
    s_stack = this;
}

ScriptCustomElementDefinitionBuilder::~ScriptCustomElementDefinitionBuilder()
{
    s_stack = m_prev;
}

bool ScriptCustomElementDefinitionBuilder::checkConstructorIntrinsics()
{
    DCHECK(m_scriptState->world().isMainWorld());

    // The signature of CustomElementsRegistry.define says this is a
    // Function
    // https://html.spec.whatwg.org/multipage/scripting.html#customelementsregistry
    CHECK(m_constructorValue->IsFunction());
    m_constructor = m_constructorValue.As<v8::Object>();
    if (!m_constructor->IsConstructor()) {
        m_exceptionState.throwTypeError(
            "constructor argument is not a constructor");
        return false;
    }
    return true;
}

bool ScriptCustomElementDefinitionBuilder::checkConstructorNotRegistered()
{
    if (ScriptCustomElementDefinition::forConstructor(
        m_scriptState.get(),
        m_registry,
        m_constructor)) {

        // Constructor is already registered.
        m_exceptionState.throwDOMException(
            NotSupportedError,
            "this constructor has already been used with this registry");
        return false;
    }
    for (auto builder = m_prev; builder; builder = builder->m_prev) {
        CHECK(!builder->m_constructor.IsEmpty());
        if (m_registry != builder->m_registry
            || m_constructor != builder->m_constructor) {
            continue;
        }
        m_exceptionState.throwDOMException(
            NotSupportedError,
            "this constructor is already being defined in this registry");
        return false;
    }
    return true;
}

bool ScriptCustomElementDefinitionBuilder::valueForName(
    const v8::Local<v8::Object>& object, const String& name,
    v8::Local<v8::Value>& value) const
{
    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Context> context = m_scriptState->context();
    v8::Local<v8::String> nameString = v8String(isolate, name);
    v8::TryCatch tryCatch(isolate);
    if (!v8Call(object->Get(context, nameString), value, tryCatch)) {
        m_exceptionState.rethrowV8Exception(tryCatch.Exception());
        return false;
    }
    return true;
}

bool ScriptCustomElementDefinitionBuilder::checkPrototype()
{
    v8::Local<v8::Value> prototypeValue;
    if (!valueForName(m_constructor, "prototype", prototypeValue))
        return false;
    if (!prototypeValue->IsObject()) {
        m_exceptionState.throwTypeError(
            "constructor prototype is not an object");
        return false;
    }
    m_prototype = prototypeValue.As<v8::Object>();
    // If retrieving the prototype destroyed the context, indicate that
    // defining the element should not proceed.
    return true;
}

bool ScriptCustomElementDefinitionBuilder::callableForName(const String& name,
    v8::Local<v8::Function>& callback) const
{
    v8::Local<v8::Value> value;
    if (!valueForName(m_prototype, name, value))
        return false;
    // "undefined" means "omitted", so return true.
    if (value->IsUndefined())
        return true;
    if (!value->IsFunction()) {
        m_exceptionState.throwTypeError(
            String::format("\"%s\" is not a callable object", name.ascii().data()));
        return false;
    }
    callback = value.As<v8::Function>();
    return true;
}

bool ScriptCustomElementDefinitionBuilder::retrieveObservedAttributes()
{
    const String kObservedAttributes = "observedAttributes";
    v8::Local<v8::Value> observedAttributesValue;
    if (!valueForName(m_constructor, kObservedAttributes, observedAttributesValue))
        return false;
    if (observedAttributesValue->IsUndefined())
        return true;
    Vector<AtomicString> list = toImplArray<Vector<AtomicString>>(
        observedAttributesValue, 0, m_scriptState->isolate(), m_exceptionState);
    if (m_exceptionState.hadException())
        return false;
    if (list.isEmpty())
        return true;
    m_observedAttributes.reserveCapacityForSize(list.size());
    for (const auto& attribute : list)
        m_observedAttributes.add(attribute);
    return true;
}

bool ScriptCustomElementDefinitionBuilder::rememberOriginalProperties()
{
    // Spec requires to use values of these properties at the point
    // CustomElementDefinition is built, even if JS changes them afterwards.
    const String kConnectedCallback = "connectedCallback";
    const String kDisconnectedCallback = "disconnectedCallback";
    const String kAttributeChangedCallback = "attributeChangedCallback";
    return callableForName(kConnectedCallback, m_connectedCallback)
        && callableForName(kDisconnectedCallback, m_disconnectedCallback)
        && callableForName(kAttributeChangedCallback, m_attributeChangedCallback)
        && (m_attributeChangedCallback.IsEmpty() || retrieveObservedAttributes());
}

CustomElementDefinition* ScriptCustomElementDefinitionBuilder::build(
    const CustomElementDescriptor& descriptor)
{
    return ScriptCustomElementDefinition::create(
        m_scriptState.get(),
        m_registry,
        descriptor,
        m_constructor,
        m_prototype,
        m_connectedCallback,
        m_disconnectedCallback,
        m_attributeChangedCallback,
        m_observedAttributes);
}

} // namespace blink
