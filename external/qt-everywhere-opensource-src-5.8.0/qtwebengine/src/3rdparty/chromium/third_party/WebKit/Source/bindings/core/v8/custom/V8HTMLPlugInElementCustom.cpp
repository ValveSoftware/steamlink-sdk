/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#include "bindings/core/v8/SharedPersistent.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HTMLEmbedElement.h"
#include "bindings/core/v8/V8HTMLObjectElement.h"
#include "core/frame/UseCounter.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

template <typename ElementType, typename PropertyType>
void getScriptableObjectProperty(PropertyType property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* impl = ElementType::toImpl(info.Holder());
    RefPtr<SharedPersistent<v8::Object>> wrapper = impl->pluginWrapper();
    if (!wrapper)
        return;

    v8::Local<v8::Object> instance = wrapper->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    if (!v8CallBoolean(instance->HasOwnProperty(info.GetIsolate()->GetCurrentContext(), property)))
        return;

    v8::Local<v8::Value> value;
    if (!instance->Get(info.GetIsolate()->GetCurrentContext(), property).ToLocal(&value))
        return;

    v8SetReturnValue(info, value);
}

template <typename ElementType, typename PropertyType>
void setScriptableObjectProperty(PropertyType property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    ASSERT(!value.IsEmpty());
    HTMLPlugInElement* impl = ElementType::toImpl(info.Holder());
    RefPtr<SharedPersistent<v8::Object>> wrapper = impl->pluginWrapper();
    if (!wrapper)
        return;

    v8::Local<v8::Object> instance = wrapper->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    // FIXME: The gTalk pepper plugin is the only plugin to make use of
    // SetProperty and that is being deprecated. This can be removed as soon as
    // it goes away.
    // Call SetProperty on a pepper plugin's scriptable object. Note that we
    // never set the return value here which would indicate that the plugin has
    // intercepted the SetProperty call, which means that the property on the
    // DOM element will also be set. For plugin's that don't intercept the call
    // (all except gTalk) this makes no difference at all. For gTalk the fact
    // that the property on the DOM element also gets set is inconsequential.
    v8CallBoolean(instance->CreateDataProperty(info.GetIsolate()->GetCurrentContext(), property, value));
}
} // namespace

void V8HTMLEmbedElement::namedPropertyGetterCustom(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    getScriptableObjectProperty<V8HTMLEmbedElement>(name.As<v8::String>(), info);
}

void V8HTMLObjectElement::namedPropertyGetterCustom(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    getScriptableObjectProperty<V8HTMLObjectElement>(name.As<v8::String>(), info);
}

void V8HTMLEmbedElement::namedPropertySetterCustom(v8::Local<v8::Name> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    setScriptableObjectProperty<V8HTMLEmbedElement>(name.As<v8::String>(), value, info);
}

void V8HTMLObjectElement::namedPropertySetterCustom(v8::Local<v8::Name> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    setScriptableObjectProperty<V8HTMLObjectElement>(name.As<v8::String>(), value, info);
}

void V8HTMLEmbedElement::indexedPropertyGetterCustom(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    getScriptableObjectProperty<V8HTMLEmbedElement>(index, info);
}

void V8HTMLObjectElement::indexedPropertyGetterCustom(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    getScriptableObjectProperty<V8HTMLObjectElement>(index, info);
}

void V8HTMLEmbedElement::indexedPropertySetterCustom(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    setScriptableObjectProperty<V8HTMLEmbedElement>(index, value, info);
}

void V8HTMLObjectElement::indexedPropertySetterCustom(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    setScriptableObjectProperty<V8HTMLObjectElement>(index, value, info);
}

namespace {

template <typename ElementType>
void invokeOnScriptableObject(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    HTMLPlugInElement* impl = ElementType::toImpl(info.Holder());
    RefPtr<SharedPersistent<v8::Object>> wrapper = impl->pluginWrapper();
    if (!wrapper)
        return;

    v8::Local<v8::Object> instance = wrapper->newLocal(info.GetIsolate());
    if (instance.IsEmpty())
        return;

    std::unique_ptr<v8::Local<v8::Value>[] > arguments = wrapArrayUnique(new v8::Local<v8::Value>[info.Length()]);
    for (int i = 0; i < info.Length(); ++i)
        arguments[i] = info[i];

    v8::Local<v8::Value> retVal;
    if (!instance->CallAsFunction(info.GetIsolate()->GetCurrentContext(), info.Holder(), info.Length(), arguments.get()).ToLocal(&retVal))
        return;
    v8SetReturnValue(info, retVal);
}

} // namespace

void V8HTMLEmbedElement::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    invokeOnScriptableObject<V8HTMLEmbedElement>(info);
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), V8HTMLEmbedElement::toImpl(info.Holder())->document(), UseCounter::HTMLEmbedElementLegacyCall);
}

void V8HTMLObjectElement::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    invokeOnScriptableObject<V8HTMLObjectElement>(info);
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), V8HTMLObjectElement::toImpl(info.Holder())->document(), UseCounter::HTMLObjectElementLegacyCall);
}

} // namespace blink
