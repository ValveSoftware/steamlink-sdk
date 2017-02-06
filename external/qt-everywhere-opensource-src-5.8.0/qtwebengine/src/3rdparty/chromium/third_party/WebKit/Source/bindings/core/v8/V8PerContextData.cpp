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

#include "bindings/core/v8/V8PerContextData.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "core/inspector/InstanceCounters.h"
#include "wtf/PtrUtil.h"
#include "wtf/StringExtras.h"
#include <memory>
#include <stdlib.h>

namespace blink {

V8PerContextData::V8PerContextData(v8::Local<v8::Context> context)
    : m_isolate(context->GetIsolate())
    , m_wrapperBoilerplates(m_isolate)
    , m_constructorMap(m_isolate)
    , m_contextHolder(wrapUnique(new gin::ContextHolder(m_isolate)))
    , m_context(m_isolate, context)
    , m_activityLogger(0)
    , m_compiledPrivateScript(m_isolate)
{
    m_contextHolder->SetContext(context);

    v8::Context::Scope contextScope(context);
    ASSERT(m_errorPrototype.isEmpty());
    v8::Local<v8::Value> objectValue = context->Global()->Get(context, v8AtomicString(m_isolate, "Error")).ToLocalChecked();
    v8::Local<v8::Value> prototypeValue = objectValue.As<v8::Object>()->Get(context, v8AtomicString(m_isolate, "prototype")).ToLocalChecked();
    m_errorPrototype.set(m_isolate, prototypeValue);

    if (isMainThread())
        InstanceCounters::incrementCounter(InstanceCounters::V8PerContextDataCounter);
}

V8PerContextData::~V8PerContextData()
{
    if (isMainThread())
        InstanceCounters::decrementCounter(InstanceCounters::V8PerContextDataCounter);
}

std::unique_ptr<V8PerContextData> V8PerContextData::create(v8::Local<v8::Context> context)
{
    return wrapUnique(new V8PerContextData(context));
}

V8PerContextData* V8PerContextData::from(v8::Local<v8::Context> context)
{
    return ScriptState::from(context)->perContextData();
}

v8::Local<v8::Object> V8PerContextData::createWrapperFromCacheSlowCase(const WrapperTypeInfo* type)
{
    ASSERT(!m_errorPrototype.isEmpty());

    v8::Context::Scope scope(context());
    v8::Local<v8::Function> interfaceObject = constructorForType(type);
    if (interfaceObject.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Local<v8::Object> instanceTemplate;
    if (!V8ObjectConstructor::newInstance(m_isolate, interfaceObject).ToLocal(&instanceTemplate))
        return v8::Local<v8::Object>();
    m_wrapperBoilerplates.Set(type, instanceTemplate);
    return instanceTemplate->Clone();
}

v8::Local<v8::Function> V8PerContextData::constructorForTypeSlowCase(const WrapperTypeInfo* type)
{
    ASSERT(!m_errorPrototype.isEmpty());

    v8::Local<v8::Context> currentContext = context();
    v8::Context::Scope scope(currentContext);
    const DOMWrapperWorld& world = DOMWrapperWorld::world(currentContext);
    // We shouldn't reach this point for the types that are implemented in v8 such as typed arrays and
    // hence don't have domTemplateFunction.
    ASSERT(type->domTemplateFunction);
    v8::Local<v8::FunctionTemplate> interfaceTemplate = type->domTemplate(m_isolate, world);
    // Getting the function might fail if we're running out of stack or memory.
    v8::Local<v8::Function> interfaceObject;
    if (!interfaceTemplate->GetFunction(currentContext).ToLocal(&interfaceObject))
        return v8::Local<v8::Function>();

    if (type->parentClass) {
        v8::Local<v8::Object> prototypeTemplate = constructorForType(type->parentClass);
        if (prototypeTemplate.IsEmpty())
            return v8::Local<v8::Function>();
        if (!v8CallBoolean(interfaceObject->SetPrototype(currentContext, prototypeTemplate)))
            return v8::Local<v8::Function>();
    }

    v8::Local<v8::Value> prototypeValue;
    if (!interfaceObject->Get(currentContext, v8AtomicString(m_isolate, "prototype")).ToLocal(&prototypeValue) || !prototypeValue->IsObject())
        return v8::Local<v8::Function>();
    v8::Local<v8::Object> prototypeObject = prototypeValue.As<v8::Object>();
    if (prototypeObject->InternalFieldCount() == v8PrototypeInternalFieldcount
        && type->wrapperTypePrototype == WrapperTypeInfo::WrapperTypeObjectPrototype)
        prototypeObject->SetAlignedPointerInInternalField(v8PrototypeTypeIndex, const_cast<WrapperTypeInfo*>(type));
    type->preparePrototypeAndInterfaceObject(currentContext, world, prototypeObject, interfaceObject, interfaceTemplate);
    if (type->wrapperTypePrototype == WrapperTypeInfo::WrapperTypeExceptionPrototype) {
        if (!v8CallBoolean(prototypeObject->SetPrototype(currentContext, m_errorPrototype.newLocal(m_isolate))))
            return v8::Local<v8::Function>();
    }

    m_constructorMap.Set(type, interfaceObject);

    return interfaceObject;
}

v8::Local<v8::Object> V8PerContextData::prototypeForType(const WrapperTypeInfo* type)
{
    v8::Local<v8::Object> constructor = constructorForType(type);
    if (constructor.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Local<v8::Value> prototypeValue;
    if (!constructor->Get(context(), v8String(m_isolate, "prototype")).ToLocal(&prototypeValue) || !prototypeValue->IsObject())
        return v8::Local<v8::Object>();
    return prototypeValue.As<v8::Object>();
}

void V8PerContextData::addCustomElementBinding(std::unique_ptr<V0CustomElementBinding> binding)
{
    m_customElementBindings.append(std::move(binding));
}

v8::Local<v8::Value> V8PerContextData::compiledPrivateScript(String className)
{
    return m_compiledPrivateScript.Get(className);
}

void V8PerContextData::setCompiledPrivateScript(String className, v8::Local<v8::Value> compiledObject)
{
    m_compiledPrivateScript.Set(className, compiledObject);
}

} // namespace blink
