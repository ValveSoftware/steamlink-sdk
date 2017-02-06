/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/dom/custom/V0CustomElementRegistry.h"

#include "bindings/core/v8/V0CustomElementConstructorBuilder.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/dom/custom/V0CustomElementException.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"

namespace blink {

V0CustomElementDefinition* V0CustomElementRegistry::registerElement(Document* document, V0CustomElementConstructorBuilder* constructorBuilder, const AtomicString& userSuppliedName, V0CustomElement::NameSet validNames, ExceptionState& exceptionState)
{
    AtomicString type = userSuppliedName.lower();

    if (!constructorBuilder->isFeatureAllowed()) {
        V0CustomElementException::throwException(V0CustomElementException::CannotRegisterFromExtension, type, exceptionState);
        return 0;
    }

    if (!V0CustomElement::isValidName(type, validNames)) {
        V0CustomElementException::throwException(V0CustomElementException::InvalidName, type, exceptionState);
        return 0;
    }

    if (m_registeredTypeNames.contains(type) || v1NameIsDefined(type)) {
        V0CustomElementException::throwException(V0CustomElementException::TypeAlreadyRegistered, type, exceptionState);
        return 0;
    }

    QualifiedName tagName = QualifiedName::null();
    if (!constructorBuilder->validateOptions(type, tagName, exceptionState))
        return 0;

    DCHECK(tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI || tagName.namespaceURI() == SVGNames::svgNamespaceURI);

    DCHECK(!m_documentWasDetached);

    V0CustomElementLifecycleCallbacks* lifecycleCallbacks = constructorBuilder->createCallbacks();

    // Consulting the constructor builder could execute script and
    // kill the document.
    if (m_documentWasDetached) {
        V0CustomElementException::throwException(V0CustomElementException::ContextDestroyedCreatingCallbacks, type, exceptionState);
        return 0;
    }

    const V0CustomElementDescriptor descriptor(type, tagName.namespaceURI(), tagName.localName());
    V0CustomElementDefinition* definition = V0CustomElementDefinition::create(descriptor, lifecycleCallbacks);

    if (!constructorBuilder->createConstructor(document, definition, exceptionState))
        return 0;

    m_definitions.add(descriptor, definition);
    m_registeredTypeNames.add(descriptor.type());

    if (!constructorBuilder->didRegisterDefinition()) {
        V0CustomElementException::throwException(V0CustomElementException::ContextDestroyedRegisteringDefinition, type, exceptionState);
        return 0;
    }

    return definition;
}

V0CustomElementDefinition* V0CustomElementRegistry::find(const V0CustomElementDescriptor& descriptor) const
{
    return m_definitions.get(descriptor);
}

bool V0CustomElementRegistry::nameIsDefined(const AtomicString& name) const
{
    return m_registeredTypeNames.contains(name);
}

void V0CustomElementRegistry::setV1(const CustomElementsRegistry* v1)
{
    DCHECK(!m_v1.get());
    m_v1 = v1;
}

bool V0CustomElementRegistry::v1NameIsDefined(const AtomicString& name) const
{
    return m_v1.get() && m_v1->nameIsDefined(name);
}

DEFINE_TRACE(V0CustomElementRegistry)
{
    visitor->trace(m_definitions);
    visitor->trace(m_v1);
}

} // namespace blink
