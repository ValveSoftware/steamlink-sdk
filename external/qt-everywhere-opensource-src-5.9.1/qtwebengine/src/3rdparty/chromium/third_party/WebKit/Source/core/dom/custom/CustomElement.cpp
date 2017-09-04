// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "core/dom/custom/CustomElementRegistry.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

CustomElementRegistry* CustomElement::registry(const Element& element) {
  return registry(element.document());
}

CustomElementRegistry* CustomElement::registry(const Document& document) {
  if (LocalDOMWindow* window = document.executingWindow())
    return window->customElements();
  return nullptr;
}

static CustomElementDefinition* definitionForElementWithoutCheck(
    const Element& element) {
  DCHECK_EQ(element.getCustomElementState(), CustomElementState::Custom);
  return element.customElementDefinition();
}

CustomElementDefinition* CustomElement::definitionForElement(
    const Element* element) {
  if (!element ||
      element->getCustomElementState() != CustomElementState::Custom)
    return nullptr;
  return definitionForElementWithoutCheck(*element);
}

bool CustomElement::isHyphenatedSpecElementName(const AtomicString& name) {
  // Even if Blink does not implement one of the related specs, (for
  // example annotation-xml is from MathML, which Blink does not
  // implement) we must prohibit using the name because that is
  // required by the HTML spec which we *do* implement. Don't remove
  // names from this list without removing them from the HTML spec
  // first.
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenatedSpecElementNames,
                      ({
                          "annotation-xml", "color-profile", "font-face",
                          "font-face-src", "font-face-uri", "font-face-format",
                          "font-face-name", "missing-glyph",
                      }));
  return hyphenatedSpecElementNames.contains(name);
}

bool CustomElement::shouldCreateCustomElement(const AtomicString& localName) {
  return RuntimeEnabledFeatures::customElementsV1Enabled() &&
         isValidName(localName);
}

bool CustomElement::shouldCreateCustomElement(const QualifiedName& tagName) {
  return shouldCreateCustomElement(tagName.localName()) &&
         tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

static CustomElementDefinition* definitionFor(
    const Document& document,
    const CustomElementDescriptor desc) {
  if (CustomElementRegistry* registry = CustomElement::registry(document))
    return registry->definitionFor(desc);
  return nullptr;
}

HTMLElement* CustomElement::createCustomElementSync(
    Document& document,
    const AtomicString& localName) {
  return createCustomElementSync(
      document,
      QualifiedName(nullAtom, localName, HTMLNames::xhtmlNamespaceURI));
}

HTMLElement* CustomElement::createCustomElementSync(
    Document& document,
    const QualifiedName& tagName) {
  DCHECK(shouldCreateCustomElement(tagName));
  if (CustomElementDefinition* definition = definitionFor(
          document,
          CustomElementDescriptor(tagName.localName(), tagName.localName())))
    return definition->createElementSync(document, tagName);
  return createUndefinedElement(document, tagName);
}

HTMLElement* CustomElement::createCustomElementAsync(
    Document& document,
    const QualifiedName& tagName) {
  DCHECK(shouldCreateCustomElement(tagName));

  // To create an element:
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6. If definition is non-null, then:
  // 6.2. If the synchronous custom elements flag is not set:
  if (CustomElementDefinition* definition = definitionFor(
          document,
          CustomElementDescriptor(tagName.localName(), tagName.localName())))
    return definition->createElementAsync(document, tagName);

  return createUndefinedElement(document, tagName);
}

HTMLElement* CustomElement::createUndefinedElement(
    Document& document,
    const QualifiedName& tagName) {
  DCHECK(shouldCreateCustomElement(tagName));

  HTMLElement* element;
  if (V0CustomElement::isValidName(tagName.localName()) &&
      document.registrationContext()) {
    Element* v0element = document.registrationContext()->createCustomTagElement(
        document, tagName);
    SECURITY_DCHECK(v0element->isHTMLElement());
    element = toHTMLElement(v0element);
  } else {
    element = HTMLElement::create(tagName, document);
  }

  element->setCustomElementState(CustomElementState::Undefined);

  return element;
}

HTMLElement* CustomElement::createFailedElement(Document& document,
                                                const QualifiedName& tagName) {
  DCHECK(shouldCreateCustomElement(tagName));

  // "create an element for a token":
  // https://html.spec.whatwg.org/multipage/syntax.html#create-an-element-for-the-token

  // 7. If this step throws an exception, let element be instead a new element
  // that implements HTMLUnknownElement, with no attributes, namespace set to
  // given namespace, namespace prefix set to null, custom element state set
  // to "failed", and node document set to document.

  HTMLElement* element = HTMLUnknownElement::create(tagName, document);
  element->setCustomElementState(CustomElementState::Failed);
  return element;
}

void CustomElement::enqueue(Element* element, CustomElementReaction* reaction) {
  // To enqueue an element on the appropriate element queue
  // https://html.spec.whatwg.org/multipage/scripting.html#enqueue-an-element-on-the-appropriate-element-queue

  // If the custom element reactions stack is not empty, then
  // Add element to the current element queue.
  if (CEReactionsScope* current = CEReactionsScope::current()) {
    current->enqueueToCurrentQueue(element, reaction);
    return;
  }

  // If the custom element reactions stack is empty, then
  // Add element to the backup element queue.
  CustomElementReactionStack::current().enqueueToBackupQueue(element, reaction);
}

void CustomElement::enqueueConnectedCallback(Element* element) {
  CustomElementDefinition* definition =
      definitionForElementWithoutCheck(*element);
  if (definition->hasConnectedCallback())
    definition->enqueueConnectedCallback(element);
}

void CustomElement::enqueueDisconnectedCallback(Element* element) {
  CustomElementDefinition* definition =
      definitionForElementWithoutCheck(*element);
  if (definition->hasDisconnectedCallback())
    definition->enqueueDisconnectedCallback(element);
}

void CustomElement::enqueueAdoptedCallback(Element* element,
                                           Document* oldOwner,
                                           Document* newOwner) {
  DCHECK_EQ(element->getCustomElementState(), CustomElementState::Custom);
  CustomElementDefinition* definition =
      definitionForElementWithoutCheck(*element);
  if (definition->hasAdoptedCallback())
    definition->enqueueAdoptedCallback(element, oldOwner, newOwner);
}

void CustomElement::enqueueAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& oldValue,
    const AtomicString& newValue) {
  CustomElementDefinition* definition =
      definitionForElementWithoutCheck(*element);
  if (definition->hasAttributeChangedCallback(name))
    definition->enqueueAttributeChangedCallback(element, name, oldValue,
                                                newValue);
}

void CustomElement::tryToUpgrade(Element* element) {
  // Try to upgrade an element
  // https://html.spec.whatwg.org/multipage/scripting.html#concept-try-upgrade

  DCHECK_EQ(element->getCustomElementState(), CustomElementState::Undefined);

  CustomElementRegistry* registry = CustomElement::registry(*element);
  if (!registry)
    return;
  if (CustomElementDefinition* definition = registry->definitionFor(
          CustomElementDescriptor(element->localName(), element->localName())))
    definition->enqueueUpgradeReaction(element);
  else
    registry->addCandidate(element);
}

}  // namespace blink
