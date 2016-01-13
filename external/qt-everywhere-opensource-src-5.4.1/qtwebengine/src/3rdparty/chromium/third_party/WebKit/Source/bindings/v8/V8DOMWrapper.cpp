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

#include "config.h"
#include "bindings/v8/V8DOMWrapper.h"

#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8PerContextData.h"
#include "bindings/v8/V8ScriptRunner.h"

namespace WebCore {

static v8::Local<v8::Object> wrapInShadowTemplate(v8::Local<v8::Object> wrapper, Node* impl, v8::Isolate* isolate)
{
    static int shadowTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Handle<v8::FunctionTemplate> shadowTemplate = data->existingDOMTemplate(&shadowTemplateKey);
    if (shadowTemplate.IsEmpty()) {
        shadowTemplate = v8::FunctionTemplate::New(isolate);
        if (shadowTemplate.IsEmpty())
            return v8::Local<v8::Object>();
        shadowTemplate->SetClassName(v8AtomicString(isolate, "HTMLDocument"));
        shadowTemplate->Inherit(V8HTMLDocument::domTemplate(isolate));
        shadowTemplate->InstanceTemplate()->SetInternalFieldCount(V8HTMLDocument::internalFieldCount);
        data->setDOMTemplate(&shadowTemplateKey, shadowTemplate);
    }

    v8::Local<v8::Function> shadowConstructor = shadowTemplate->GetFunction();
    if (shadowConstructor.IsEmpty())
        return v8::Local<v8::Object>();

    v8::Local<v8::Object> shadow = V8ScriptRunner::instantiateObject(isolate, shadowConstructor);
    if (shadow.IsEmpty())
        return v8::Local<v8::Object>();
    shadow->SetPrototype(wrapper);
    V8DOMWrapper::setNativeInfoForHiddenWrapper(wrapper, &V8HTMLDocument::wrapperTypeInfo, impl);
    return shadow;
}

v8::Local<v8::Object> V8DOMWrapper::createWrapper(v8::Handle<v8::Object> creationContext, const WrapperTypeInfo* type, void* impl, v8::Isolate* isolate)
{
    V8WrapperInstantiationScope scope(creationContext, isolate);

    V8PerContextData* perContextData = V8PerContextData::from(scope.context());
    v8::Local<v8::Object> wrapper = perContextData ? perContextData->createWrapperFromCache(type) : V8ObjectConstructor::newInstance(isolate, type->domTemplate(isolate)->GetFunction());

    if (type == &V8HTMLDocument::wrapperTypeInfo && !wrapper.IsEmpty())
        wrapper = wrapInShadowTemplate(wrapper, static_cast<Node*>(impl), isolate);

    return wrapper;
}

bool V8DOMWrapper::isDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    if (v8::Handle<v8::Object>::Cast(value)->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;

    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(value);
    ASSERT(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
    ASSERT(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));

    const WrapperTypeInfo* typeInfo = static_cast<const WrapperTypeInfo*>(wrapper->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));
    // FIXME: We should add a more strict way to check if the typeInfo is a typeInfo of some DOM wrapper.
    // Even if it's a typeInfo of Blink, it's not guaranteed that it's a typeInfo of a DOM wrapper.
    return typeInfo->ginEmbedder == gin::kEmbedderBlink;
}

}  // namespace WebCore
