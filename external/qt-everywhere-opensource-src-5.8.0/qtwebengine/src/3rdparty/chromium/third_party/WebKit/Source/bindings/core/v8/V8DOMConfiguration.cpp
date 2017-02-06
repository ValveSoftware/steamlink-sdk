/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8DOMConfiguration.h"

#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace {

void installAttributeInternal(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const V8DOMConfiguration::AttributeConfiguration& attribute, const DOMWrapperWorld& world)
{
    if (attribute.exposeConfiguration == V8DOMConfiguration::OnlyExposedToPrivateScript
        && !world.isPrivateScriptIsolatedWorld())
        return;

    v8::Local<v8::Name> name = v8AtomicString(isolate, attribute.name);
    v8::AccessorNameGetterCallback getter = attribute.getter;
    v8::AccessorNameSetterCallback setter = attribute.setter;
    if (world.isMainWorld()) {
        if (attribute.getterForMainWorld)
            getter = attribute.getterForMainWorld;
        if (attribute.setterForMainWorld)
            setter = attribute.setterForMainWorld;
    }
    v8::Local<v8::Value> data = v8::External::New(isolate, const_cast<WrapperTypeInfo*>(attribute.data));

    DCHECK(attribute.propertyLocationConfiguration);
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnInstance)
        instanceTemplate->SetNativeDataProperty(name, getter, setter, data, static_cast<v8::PropertyAttribute>(attribute.attribute), v8::Local<v8::AccessorSignature>(), static_cast<v8::AccessControl>(attribute.settings));
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnPrototype)
        prototypeTemplate->SetNativeDataProperty(name, getter, setter, data, static_cast<v8::PropertyAttribute>(attribute.attribute), v8::Local<v8::AccessorSignature>(), static_cast<v8::AccessControl>(attribute.settings));
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnInterface)
        NOTREACHED();
}

void installAttributeInternal(v8::Isolate* isolate, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, const V8DOMConfiguration::AttributeConfiguration& attribute, const DOMWrapperWorld& world)
{
    if (attribute.exposeConfiguration == V8DOMConfiguration::OnlyExposedToPrivateScript
        && !world.isPrivateScriptIsolatedWorld())
        return;

    v8::Local<v8::Name> name = v8AtomicString(isolate, attribute.name);

    // This method is only being used for installing interfaces which are
    // enabled through origin trials. Assert here that it is being called with
    // an attribute configuration for a constructor.
    // TODO(iclelland): Relax this constraint and allow arbitrary data-type
    // properties to be added here.
    DCHECK_EQ(&v8ConstructorAttributeGetter, attribute.getter);

    V8PerContextData* perContextData = V8PerContextData::from(isolate->GetCurrentContext());
    v8::Local<v8::Function> data = perContextData->constructorForType(attribute.data);

    DCHECK(attribute.propertyLocationConfiguration);
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnInstance)
        v8CallOrCrash(instance->DefineOwnProperty(isolate->GetCurrentContext(), name, data, static_cast<v8::PropertyAttribute>(attribute.attribute)));
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnPrototype)
        v8CallOrCrash(prototype->DefineOwnProperty(isolate->GetCurrentContext(), name, data, static_cast<v8::PropertyAttribute>(attribute.attribute)));
    if (attribute.propertyLocationConfiguration & V8DOMConfiguration::OnInterface)
        NOTREACHED();
}

template<class FunctionOrTemplate>
v8::Local<FunctionOrTemplate> createAccessorFunctionOrTemplate(v8::Isolate*, v8::FunctionCallback, v8::Local<v8::Value> data, v8::Local<v8::Signature>, int length);

template<>
v8::Local<v8::FunctionTemplate> createAccessorFunctionOrTemplate<v8::FunctionTemplate>(v8::Isolate* isolate, v8::FunctionCallback callback, v8::Local<v8::Value> data, v8::Local<v8::Signature> signature, int length)
{
    v8::Local<v8::FunctionTemplate> functionTemplate;
    if (callback) {
        functionTemplate = v8::FunctionTemplate::New(isolate, callback, data, signature, length);
        if (!functionTemplate.IsEmpty()) {
            functionTemplate->RemovePrototype();
            functionTemplate->SetAcceptAnyReceiver(false);
        }
    }
    return functionTemplate;
}

template<>
v8::Local<v8::Function> createAccessorFunctionOrTemplate<v8::Function>(v8::Isolate* isolate, v8::FunctionCallback callback, v8::Local<v8::Value> data, v8::Local<v8::Signature> signature, int length)
{
    if (!callback)
        return v8::Local<v8::Function>();

    v8::Local<v8::FunctionTemplate> functionTemplate = createAccessorFunctionOrTemplate<v8::FunctionTemplate>(isolate, callback, data, signature, length);
    if (functionTemplate.IsEmpty())
        return v8::Local<v8::Function>();

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Function> function;
    if (!functionTemplate->GetFunction(context).ToLocal(&function))
        return v8::Local<v8::Function>();
    return function;
}

template<class ObjectOrTemplate, class FunctionOrTemplate>
void installAccessorInternal(v8::Isolate* isolate, v8::Local<ObjectOrTemplate> instanceOrTemplate, v8::Local<ObjectOrTemplate> prototypeOrTemplate, v8::Local<FunctionOrTemplate> interfaceOrTemplate, v8::Local<v8::Signature> signature, const V8DOMConfiguration::AccessorConfiguration& accessor, const DOMWrapperWorld& world)
{
    if (accessor.exposeConfiguration == V8DOMConfiguration::OnlyExposedToPrivateScript
        && !world.isPrivateScriptIsolatedWorld())
        return;

    v8::Local<v8::Name> name = v8AtomicString(isolate, accessor.name);
    v8::FunctionCallback getterCallback = accessor.getter;
    v8::FunctionCallback setterCallback = accessor.setter;
    if (world.isMainWorld()) {
        if (accessor.getterForMainWorld)
            getterCallback = accessor.getterForMainWorld;
        if (accessor.setterForMainWorld)
            setterCallback = accessor.setterForMainWorld;
    }
    // Support [LenientThis] by not specifying the signature.  V8 does not do
    // the type checking against holder if no signature is specified.  Note that
    // info.Holder() passed to callbacks will be *unsafe*.
    if (accessor.holderCheckConfiguration == V8DOMConfiguration::DoNotCheckHolder)
        signature = v8::Local<v8::Signature>();
    v8::Local<v8::Value> data = v8::External::New(isolate, const_cast<WrapperTypeInfo*>(accessor.data));

    DCHECK(accessor.propertyLocationConfiguration);
    if (accessor.propertyLocationConfiguration &
        (V8DOMConfiguration::OnInstance | V8DOMConfiguration::OnPrototype)) {
        v8::Local<FunctionOrTemplate> getter = createAccessorFunctionOrTemplate<FunctionOrTemplate>(isolate, getterCallback, data, signature, 0);
        v8::Local<FunctionOrTemplate> setter = createAccessorFunctionOrTemplate<FunctionOrTemplate>(isolate, setterCallback, data, signature, 1);
        if (accessor.propertyLocationConfiguration & V8DOMConfiguration::OnInstance)
            instanceOrTemplate->SetAccessorProperty(name, getter, setter, static_cast<v8::PropertyAttribute>(accessor.attribute), static_cast<v8::AccessControl>(accessor.settings));
        if (accessor.propertyLocationConfiguration & V8DOMConfiguration::OnPrototype)
            prototypeOrTemplate->SetAccessorProperty(name, getter, setter, static_cast<v8::PropertyAttribute>(accessor.attribute), static_cast<v8::AccessControl>(accessor.settings));
    }
    if (accessor.propertyLocationConfiguration & V8DOMConfiguration::OnInterface) {
        // Attributes installed on the interface object must be static
        // attributes, so no need to specify a signature, i.e. no need to do
        // type check against a holder.
        v8::Local<FunctionOrTemplate> getter = createAccessorFunctionOrTemplate<FunctionOrTemplate>(isolate, getterCallback, data, v8::Local<v8::Signature>(), 0);
        v8::Local<FunctionOrTemplate> setter = createAccessorFunctionOrTemplate<FunctionOrTemplate>(isolate, setterCallback, data, v8::Local<v8::Signature>(), 1);
        interfaceOrTemplate->SetAccessorProperty(name, getter, setter, static_cast<v8::PropertyAttribute>(accessor.attribute), static_cast<v8::AccessControl>(accessor.settings));
    }
}

v8::Local<v8::Primitive> valueForConstant(v8::Isolate* isolate, const V8DOMConfiguration::ConstantConfiguration& constant)
{
    v8::Local<v8::Primitive> value;
    switch (constant.type) {
    case V8DOMConfiguration::ConstantTypeShort:
    case V8DOMConfiguration::ConstantTypeLong:
    case V8DOMConfiguration::ConstantTypeUnsignedShort:
        value = v8::Integer::New(isolate, constant.ivalue);
        break;
    case V8DOMConfiguration::ConstantTypeUnsignedLong:
        value = v8::Integer::NewFromUnsigned(isolate, constant.ivalue);
        break;
    case V8DOMConfiguration::ConstantTypeFloat:
    case V8DOMConfiguration::ConstantTypeDouble:
        value = v8::Number::New(isolate, constant.dvalue);
        break;
    default:
        NOTREACHED();
    }
    return value;
}

void installConstantInternal(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const V8DOMConfiguration::ConstantConfiguration& constant)
{
    v8::Local<v8::String> constantName = v8AtomicString(isolate, constant.name);
    v8::PropertyAttribute attributes =
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
    v8::Local<v8::Primitive> value = valueForConstant(isolate, constant);
    interfaceTemplate->Set(constantName, value, attributes);
    prototypeTemplate->Set(constantName, value, attributes);
}

void installConstantInternal(v8::Isolate* isolate, v8::Local<v8::Function> interface, v8::Local<v8::Object> prototype, const V8DOMConfiguration::ConstantConfiguration& constant)
{
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Name> name = v8AtomicString(isolate, constant.name);
    v8::PropertyAttribute attributes =
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
    v8::Local<v8::Primitive> value = valueForConstant(isolate, constant);
    v8CallOrCrash(interface->DefineOwnProperty(context, name, value, attributes));
    v8CallOrCrash(prototype->DefineOwnProperty(context, name, value, attributes));
}

template<class Configuration>
void installMethodInternal(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::Signature> signature, const Configuration& method, const DOMWrapperWorld& world)
{
    if (method.exposeConfiguration == V8DOMConfiguration::OnlyExposedToPrivateScript
        && !world.isPrivateScriptIsolatedWorld())
        return;

    v8::Local<v8::Name> name = method.methodName(isolate);
    v8::FunctionCallback callback = method.callbackForWorld(world);

    DCHECK(method.propertyLocationConfiguration);
    if (method.propertyLocationConfiguration &
        (V8DOMConfiguration::OnInstance | V8DOMConfiguration::OnPrototype)) {
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), signature, method.length);
        functionTemplate->RemovePrototype();
        if (method.propertyLocationConfiguration & V8DOMConfiguration::OnInstance)
            instanceTemplate->Set(name, functionTemplate, static_cast<v8::PropertyAttribute>(method.attribute));
        if (method.propertyLocationConfiguration & V8DOMConfiguration::OnPrototype)
            prototypeTemplate->Set(name, functionTemplate, static_cast<v8::PropertyAttribute>(method.attribute));
    }
    if (method.propertyLocationConfiguration & V8DOMConfiguration::OnInterface) {
        // Operations installed on the interface object must be static
        // operations, so no need to specify a signature, i.e. no need to do
        // type check against a holder.
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), v8::Local<v8::Signature>(), method.length);
        functionTemplate->RemovePrototype();
        interfaceTemplate->Set(name, functionTemplate, static_cast<v8::PropertyAttribute>(method.attribute));
    }
}

void installMethodInternal(v8::Isolate* isolate, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface, v8::Local<v8::Signature> signature, const V8DOMConfiguration::MethodConfiguration& method, const DOMWrapperWorld& world)
{
    if (method.exposeConfiguration == V8DOMConfiguration::OnlyExposedToPrivateScript
        && !world.isPrivateScriptIsolatedWorld())
        return;

    v8::Local<v8::Name> name = method.methodName(isolate);
    v8::FunctionCallback callback = method.callbackForWorld(world);

    DCHECK(method.propertyLocationConfiguration);
    if (method.propertyLocationConfiguration &
        (V8DOMConfiguration::OnInstance | V8DOMConfiguration::OnPrototype)) {
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), signature, method.length);
        functionTemplate->RemovePrototype();
        v8::Local<v8::Function> function = v8CallOrCrash(functionTemplate->GetFunction(isolate->GetCurrentContext()));
        if (method.propertyLocationConfiguration & V8DOMConfiguration::OnInstance)
            v8CallOrCrash(instance->DefineOwnProperty(isolate->GetCurrentContext(), name, function, static_cast<v8::PropertyAttribute>(method.attribute)));
        if (method.propertyLocationConfiguration & V8DOMConfiguration::OnPrototype)
            v8CallOrCrash(prototype->DefineOwnProperty(isolate->GetCurrentContext(), name, function, static_cast<v8::PropertyAttribute>(method.attribute)));
    }
    if (method.propertyLocationConfiguration & V8DOMConfiguration::OnInterface) {
        // Operations installed on the interface object must be static
        // operations, so no need to specify a signature, i.e. no need to do
        // type check against a holder.
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), v8::Local<v8::Signature>(), method.length);
        functionTemplate->RemovePrototype();
        v8::Local<v8::Function> function = v8CallOrCrash(functionTemplate->GetFunction(isolate->GetCurrentContext()));
        v8CallOrCrash(interface->DefineOwnProperty(isolate->GetCurrentContext(), name, function, static_cast<v8::PropertyAttribute>(method.attribute)));
    }
}

} // namespace

void V8DOMConfiguration::installAttributes(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const AttributeConfiguration* attributes, size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i)
        installAttributeInternal(isolate, instanceTemplate, prototypeTemplate, attributes[i], world);
}

void V8DOMConfiguration::installAttribute(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const AttributeConfiguration& attribute)
{
    installAttributeInternal(isolate, instanceTemplate, prototypeTemplate, attribute, world);
}

void V8DOMConfiguration::installAttribute(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, const AttributeConfiguration& attribute)
{
    installAttributeInternal(isolate, instance, prototype, attribute, world);
}

void V8DOMConfiguration::installAccessors(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::Signature> signature, const AccessorConfiguration* accessors, size_t accessorCount)
{
    for (size_t i = 0; i < accessorCount; ++i)
        installAccessorInternal(isolate, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, accessors[i], world);
}

void V8DOMConfiguration::installAccessor(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::Signature> signature, const AccessorConfiguration& accessor)
{
    installAccessorInternal(isolate, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, accessor, world);
}

void V8DOMConfiguration::installAccessor(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface, v8::Local<v8::Signature> signature, const AccessorConfiguration& accessor)
{
    installAccessorInternal(isolate, instance, prototype, interface, signature, accessor, world);
}

void V8DOMConfiguration::installConstants(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const ConstantConfiguration* constants, size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i)
        installConstantInternal(isolate, interfaceTemplate, prototypeTemplate, constants[i]);
}

void V8DOMConfiguration::installConstant(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const ConstantConfiguration& constant)
{
    installConstantInternal(isolate, interfaceTemplate, prototypeTemplate, constant);
}

void V8DOMConfiguration::installConstant(v8::Isolate* isolate, v8::Local<v8::Function> interface, v8::Local<v8::Object> prototype, const ConstantConfiguration& constant)
{
    installConstantInternal(isolate, interface, prototype, constant);
}

void V8DOMConfiguration::installConstantWithGetter(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, const char* name, v8::AccessorNameGetterCallback getter)
{
    v8::Local<v8::String> constantName = v8AtomicString(isolate, name);
    v8::PropertyAttribute attributes =
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
    interfaceTemplate->SetNativeDataProperty(constantName, getter, 0, v8::Local<v8::Value>(), attributes);
    prototypeTemplate->SetNativeDataProperty(constantName, getter, 0, v8::Local<v8::Value>(), attributes);
}

void V8DOMConfiguration::installMethods(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::Signature> signature, const MethodConfiguration* methods, size_t methodCount)
{
    for (size_t i = 0; i < methodCount; ++i)
        installMethodInternal(isolate, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, methods[i], world);
}

void V8DOMConfiguration::installMethod(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> instanceTemplate, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::FunctionTemplate> interfaceTemplate, v8::Local<v8::Signature> signature, const MethodConfiguration& method)
{
    installMethodInternal(isolate, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, method, world);
}

void V8DOMConfiguration::installMethod(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface, v8::Local<v8::Signature> signature, const MethodConfiguration& method)
{
    installMethodInternal(isolate, instance, prototype, interface, signature, method, world);
}

void V8DOMConfiguration::installMethod(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::ObjectTemplate> prototypeTemplate, v8::Local<v8::Signature> signature, const SymbolKeyedMethodConfiguration& method)
{
    installMethodInternal(isolate, v8::Local<v8::ObjectTemplate>(), prototypeTemplate, v8::Local<v8::FunctionTemplate>(), signature, method, world);
}

void V8DOMConfiguration::initializeDOMInterfaceTemplate(v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> interfaceTemplate, const char* interfaceName, v8::Local<v8::FunctionTemplate> parentInterfaceTemplate, size_t v8InternalFieldCount)
{
    interfaceTemplate->SetClassName(v8AtomicString(isolate, interfaceName));
    interfaceTemplate->ReadOnlyPrototype();
    v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
    instanceTemplate->SetInternalFieldCount(v8InternalFieldCount);
    // TODO(yukishiino): We should set the class string to the platform object
    // (|instanceTemplate|), too.  The reason that we don't set it is that
    // it prevents minor GC to collect unreachable DOM objects (a layout test
    // fast/dom/minor-dom-gc.html fails if we set the class string).
    // See also http://heycam.github.io/webidl/#es-platform-objects
    setClassString(isolate, prototypeTemplate, interfaceName);
    if (!parentInterfaceTemplate.IsEmpty()) {
        interfaceTemplate->Inherit(parentInterfaceTemplate);
        // Marks the prototype object as one of native-backed objects.
        // This is needed since bug 110436 asks WebKit to tell native-initiated prototypes from pure-JS ones.
        // This doesn't mark kinds "root" classes like Node, where setting this changes prototype chain structure.
        prototypeTemplate->SetInternalFieldCount(v8PrototypeInternalFieldcount);
    }
}

v8::Local<v8::FunctionTemplate> V8DOMConfiguration::domClassTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, WrapperTypeInfo* wrapperTypeInfo, InstallTemplateFunction configureDOMClassTemplate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> result = data->findInterfaceTemplate(world, wrapperTypeInfo);
    if (!result.IsEmpty())
        return result;

    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "BuildDOMTemplate");
    result = v8::FunctionTemplate::New(isolate, V8ObjectConstructor::isValidConstructorMode);
    configureDOMClassTemplate(isolate, world, result);
    data->setInterfaceTemplate(world, wrapperTypeInfo, result);
    return result;
}

void V8DOMConfiguration::setClassString(v8::Isolate* isolate, v8::Local<v8::ObjectTemplate> objectTemplate, const char* classString)
{
    objectTemplate->Set(v8::Symbol::GetToStringTag(isolate), v8AtomicString(isolate, classString), static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));
}

} // namespace blink
