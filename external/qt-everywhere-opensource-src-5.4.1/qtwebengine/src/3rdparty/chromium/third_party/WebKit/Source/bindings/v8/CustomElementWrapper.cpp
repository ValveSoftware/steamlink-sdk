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

#include "config.h"
#include "bindings/v8/CustomElementWrapper.h"

#include "bindings/core/v8/V8HTMLElement.h"
#include "bindings/core/v8/V8SVGElement.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/V8HTMLElementWrapperFactory.h" // FIXME: should be bindings/core/v8
#include "core/V8SVGElementWrapperFactory.h" // FIXME: should be bindings/core/v8
#include "core/dom/custom/CustomElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

template<typename ElementType>
v8::Handle<v8::Object> createDirectWrapper(ElementType*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

template<>
v8::Handle<v8::Object> createDirectWrapper<HTMLElement>(HTMLElement* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return createV8HTMLDirectWrapper(element, creationContext, isolate);
}

template<>
v8::Handle<v8::Object> createDirectWrapper<SVGElement>(SVGElement* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return createV8SVGDirectWrapper(element, creationContext, isolate);
}

template<typename ElementType>
v8::Handle<v8::Object> createFallbackWrapper(ElementType*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

template<>
v8::Handle<v8::Object> createFallbackWrapper<HTMLElement>(HTMLElement* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return createV8HTMLFallbackWrapper(toHTMLUnknownElement(element), creationContext, isolate);
}

template<>
v8::Handle<v8::Object> createFallbackWrapper<SVGElement>(SVGElement* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return createV8SVGFallbackWrapper(element, creationContext, isolate);
}

template<typename ElementType>
v8::Handle<v8::Object> createUpgradeCandidateWrapper(ElementType* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, v8::Handle<v8::Object> (*createSpecificWrapper)(ElementType* element, v8::Handle<v8::Object> creationContext, v8::Isolate*))
{
    if (CustomElement::isValidName(element->localName()))
        return createDirectWrapper(element, creationContext, isolate);
    if (createSpecificWrapper)
        return createSpecificWrapper(element, creationContext, isolate);
    return createFallbackWrapper(element, creationContext, isolate);
}

template<typename ElementType, typename WrapperType>
v8::Handle<v8::Object> CustomElementWrapper<ElementType, WrapperType>::wrap(PassRefPtrWillBeRawPtr<ElementType> element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, v8::Handle<v8::Object> (*createSpecificWrapper)(ElementType* element, v8::Handle<v8::Object> creationContext, v8::Isolate*))
{
    ASSERT(DOMDataStore::getWrapper<V8Element>(element.get(), isolate).IsEmpty());

    ASSERT(!creationContext.IsEmpty());
    v8::Handle<v8::Context> context = creationContext->CreationContext();

    if (!element->isUpgradedCustomElement() || DOMWrapperWorld::world(context).isIsolatedWorld())
        return createUpgradeCandidateWrapper(element.get(), creationContext, isolate, createSpecificWrapper);

    V8PerContextData* perContextData = V8PerContextData::from(context);
    if (!perContextData)
        return v8::Handle<v8::Object>();

    CustomElementBinding* binding = perContextData->customElementBinding(element->customElementDefinition());
    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, binding->wrapperType(), element.get(), isolate);
    if (wrapper.IsEmpty())
        return v8::Handle<v8::Object>();

    wrapper->SetPrototype(binding->prototype());

    V8DOMWrapper::associateObjectWithWrapper<WrapperType>(element, binding->wrapperType(), wrapper, isolate, WrapperConfiguration::Dependent);
    return wrapper;
}

template
class CustomElementWrapper<HTMLElement, V8HTMLElement>;

template
class CustomElementWrapper<SVGElement, V8SVGElement>;

} // namespace WebCore
