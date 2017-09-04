/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/dom/custom/V0CustomElementRegistrationContext.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementDefinition.h"
#include "core/dom/custom/V0CustomElementScheduler.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "core/svg/SVGUnknownElement.h"

namespace blink {

V0CustomElementRegistrationContext::V0CustomElementRegistrationContext()
    : m_candidates(V0CustomElementUpgradeCandidateMap::create()) {}

void V0CustomElementRegistrationContext::registerElement(
    Document* document,
    V0CustomElementConstructorBuilder* constructorBuilder,
    const AtomicString& type,
    V0CustomElement::NameSet validNames,
    ExceptionState& exceptionState) {
  V0CustomElementDefinition* definition = m_registry.registerElement(
      document, constructorBuilder, type, validNames, exceptionState);

  if (!definition)
    return;

  // Upgrade elements that were waiting for this definition.
  V0CustomElementUpgradeCandidateMap::ElementSet* upgradeCandidates =
      m_candidates->takeUpgradeCandidatesFor(definition->descriptor());

  if (!upgradeCandidates)
    return;

  for (const auto& candidate : *upgradeCandidates)
    V0CustomElement::define(candidate, definition);
}

Element* V0CustomElementRegistrationContext::createCustomTagElement(
    Document& document,
    const QualifiedName& tagName) {
  DCHECK(V0CustomElement::isValidName(tagName.localName()));

  Element* element;

  if (HTMLNames::xhtmlNamespaceURI == tagName.namespaceURI()) {
    element = HTMLElement::create(tagName, document);
  } else if (SVGNames::svgNamespaceURI == tagName.namespaceURI()) {
    element = SVGUnknownElement::create(tagName, document);
  } else {
    // XML elements are not custom elements, so return early.
    return Element::create(tagName, &document);
  }

  element->setV0CustomElementState(Element::V0WaitingForUpgrade);
  resolveOrScheduleResolution(element, nullAtom);
  return element;
}

void V0CustomElementRegistrationContext::didGiveTypeExtension(
    Element* element,
    const AtomicString& type) {
  resolveOrScheduleResolution(element, type);
}

void V0CustomElementRegistrationContext::resolveOrScheduleResolution(
    Element* element,
    const AtomicString& typeExtension) {
  // If an element has a custom tag name it takes precedence over
  // the "is" attribute (if any).
  const AtomicString& type = V0CustomElement::isValidName(element->localName())
                                 ? element->localName()
                                 : typeExtension;
  DCHECK(!type.isNull());

  V0CustomElementDescriptor descriptor(type, element->namespaceURI(),
                                       element->localName());
  DCHECK_EQ(element->getV0CustomElementState(), Element::V0WaitingForUpgrade);

  V0CustomElementScheduler::resolveOrScheduleResolution(this, element,
                                                        descriptor);
}

void V0CustomElementRegistrationContext::resolve(
    Element* element,
    const V0CustomElementDescriptor& descriptor) {
  V0CustomElementDefinition* definition = m_registry.find(descriptor);
  if (definition) {
    V0CustomElement::define(element, definition);
  } else {
    DCHECK_EQ(element->getV0CustomElementState(), Element::V0WaitingForUpgrade);
    m_candidates->add(descriptor, element);
  }
}

void V0CustomElementRegistrationContext::setIsAttributeAndTypeExtension(
    Element* element,
    const AtomicString& type) {
  DCHECK(element);
  DCHECK(!type.isEmpty());
  element->setAttribute(HTMLNames::isAttr, type);
  setTypeExtension(element, type);
}

void V0CustomElementRegistrationContext::setTypeExtension(
    Element* element,
    const AtomicString& type) {
  if (!element->isHTMLElement() && !element->isSVGElement())
    return;

  V0CustomElementRegistrationContext* context =
      element->document().registrationContext();
  if (!context)
    return;

  if (element->isV0CustomElement()) {
    // This can happen if:
    // 1. The element has a custom tag, which takes precedence over
    //    type extensions.
    // 2. Undoing a command (eg ReplaceNodeWithSpan) recycles an
    //    element but tries to overwrite its attribute list.
    return;
  }

  // Custom tags take precedence over type extensions
  DCHECK(!V0CustomElement::isValidName(element->localName()));

  if (!V0CustomElement::isValidName(type))
    return;

  element->setV0CustomElementState(Element::V0WaitingForUpgrade);
  context->didGiveTypeExtension(element,
                                element->document().convertLocalName(type));
}

bool V0CustomElementRegistrationContext::nameIsDefined(
    const AtomicString& name) const {
  return m_registry.nameIsDefined(name);
}

void V0CustomElementRegistrationContext::setV1(
    const CustomElementRegistry* v1) {
  m_registry.setV1(v1);
}

DEFINE_TRACE(V0CustomElementRegistrationContext) {
  visitor->trace(m_candidates);
  visitor->trace(m_registry);
}

}  // namespace blink
