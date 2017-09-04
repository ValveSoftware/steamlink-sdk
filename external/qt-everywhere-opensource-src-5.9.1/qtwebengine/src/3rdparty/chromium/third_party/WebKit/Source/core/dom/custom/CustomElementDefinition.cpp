// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

#include "core/dom/Attr.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementAdoptedCallbackReaction.h"
#include "core/dom/custom/CustomElementAttributeChangedCallbackReaction.h"
#include "core/dom/custom/CustomElementConnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementDisconnectedCallbackReaction.h"
#include "core/dom/custom/CustomElementReaction.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"
#include "core/html/HTMLElement.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor)
    : m_descriptor(descriptor) {}

CustomElementDefinition::CustomElementDefinition(
    const CustomElementDescriptor& descriptor,
    const HashSet<AtomicString>& observedAttributes)
    : m_descriptor(descriptor),
      m_observedAttributes(observedAttributes),
      m_hasStyleAttributeChangedCallback(
          observedAttributes.contains(HTMLNames::styleAttr.localName())) {}

CustomElementDefinition::~CustomElementDefinition() {}

DEFINE_TRACE(CustomElementDefinition) {
  visitor->trace(m_constructionStack);
}

static String errorMessageForConstructorResult(Element* element,
                                               Document& document,
                                               const QualifiedName& tagName) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.4. If result's attribute list is not empty, then throw a
  // NotSupportedError.
  if (element->hasAttributes())
    return "The result must not have attributes";
  // 6.1.5. If result has children, then throw a NotSupportedError.
  if (element->hasChildren())
    return "The result must not have children";
  // 6.1.6. If result's parent is not null, then throw a NotSupportedError.
  if (element->parentNode())
    return "The result must not have a parent";
  // 6.1.7. If result's node document is not document, then throw a
  // NotSupportedError.
  if (&element->document() != &document)
    return "The result must be in the same document";
  // 6.1.8. If result's namespace is not the HTML namespace, then throw a
  // NotSupportedError.
  if (element->namespaceURI() != HTMLNames::xhtmlNamespaceURI)
    return "The result must have HTML namespace";
  // 6.1.9. If result's local name is not equal to localName, then throw a
  // NotSupportedError.
  if (element->localName() != tagName.localName())
    return "The result must have the same localName";
  return String();
}

void CustomElementDefinition::checkConstructorResult(
    Element* element,
    Document& document,
    const QualifiedName& tagName,
    ExceptionState& exceptionState) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6.1.3. If result does not implement the HTMLElement interface, throw a
  // TypeError.
  // See https://github.com/whatwg/html/issues/1402 for more clarifications.
  if (!element || !element->isHTMLElement()) {
    exceptionState.throwTypeError(
        "The result must implement HTMLElement interface");
    return;
  }

  // 6.1.4. through 6.1.9.
  const String message =
      errorMessageForConstructorResult(element, document, tagName);
  if (!message.isEmpty())
    exceptionState.throwDOMException(NotSupportedError, message);
}

HTMLElement* CustomElementDefinition::createElementForConstructor(
    Document& document) {
  // TODO(kojii): When HTMLElementFactory has an option not to queue
  // upgrade, call that instead of HTMLElement. HTMLElement is enough
  // for now, but type extension will require HTMLElementFactory.
  HTMLElement* element =
      HTMLElement::create(QualifiedName(nullAtom, descriptor().localName(),
                                        HTMLNames::xhtmlNamespaceURI),
                          document);
  // TODO(davaajav): write this as one call to setCustomElementState instead of
  // two
  element->setCustomElementState(CustomElementState::Undefined);
  element->setCustomElementDefinition(this);
  return element;
}

HTMLElement* CustomElementDefinition::createElementAsync(
    Document& document,
    const QualifiedName& tagName) {
  // https://dom.spec.whatwg.org/#concept-create-element
  // 6. If definition is non-null, then:
  // 6.2. If the synchronous custom elements flag is not set:
  // 6.2.1. Set result to a new element that implements the HTMLElement
  // interface, with no attributes, namespace set to the HTML namespace,
  // namespace prefix set to prefix, local name set to localName, custom
  // element state set to "undefined", and node document set to document.
  HTMLElement* element = HTMLElement::create(tagName, document);
  element->setCustomElementState(CustomElementState::Undefined);
  // 6.2.2. Enqueue a custom element upgrade reaction given result and
  // definition.
  enqueueUpgradeReaction(element);
  return element;
}

CustomElementDefinition::ConstructionStackScope::ConstructionStackScope(
    CustomElementDefinition* definition,
    Element* element)
    : m_constructionStack(definition->m_constructionStack), m_element(element) {
  // Push the construction stack.
  m_constructionStack.append(element);
  m_depth = m_constructionStack.size();
}

CustomElementDefinition::ConstructionStackScope::~ConstructionStackScope() {
  // Pop the construction stack.
  DCHECK(!m_constructionStack.last() ||
         m_constructionStack.last() == m_element);
  DCHECK_EQ(m_constructionStack.size(), m_depth);  // It's a *stack*.
  m_constructionStack.pop_back();
}

// https://html.spec.whatwg.org/multipage/scripting.html#concept-upgrade-an-element
void CustomElementDefinition::upgrade(Element* element) {
  DCHECK_EQ(element->getCustomElementState(), CustomElementState::Undefined);

  if (!m_observedAttributes.isEmpty())
    enqueueAttributeChangedCallbackForAllAttributes(element);

  if (element->isConnected() && hasConnectedCallback())
    enqueueConnectedCallback(element);

  bool succeeded = false;
  {
    ConstructionStackScope constructionStackScope(this, element);
    succeeded = runConstructor(element);
  }
  if (!succeeded) {
    element->setCustomElementState(CustomElementState::Failed);
    CustomElementReactionStack::current().clearQueue(element);
    return;
  }

  element->setCustomElementDefinition(this);
}

bool CustomElementDefinition::hasAttributeChangedCallback(
    const QualifiedName& name) const {
  return m_observedAttributes.contains(name.localName());
}

bool CustomElementDefinition::hasStyleAttributeChangedCallback() const {
  return m_hasStyleAttributeChangedCallback;
}

void CustomElementDefinition::enqueueUpgradeReaction(Element* element) {
  CustomElement::enqueue(element, new CustomElementUpgradeReaction(this));
}

void CustomElementDefinition::enqueueConnectedCallback(Element* element) {
  CustomElement::enqueue(element,
                         new CustomElementConnectedCallbackReaction(this));
}

void CustomElementDefinition::enqueueDisconnectedCallback(Element* element) {
  CustomElement::enqueue(element,
                         new CustomElementDisconnectedCallbackReaction(this));
}

void CustomElementDefinition::enqueueAdoptedCallback(Element* element,
                                                     Document* oldDocument,
                                                     Document* newDocument) {
  CustomElementReaction* reaction =
      new CustomElementAdoptedCallbackReaction(this, oldDocument, newDocument);
  CustomElement::enqueue(element, reaction);
}

void CustomElementDefinition::enqueueAttributeChangedCallback(
    Element* element,
    const QualifiedName& name,
    const AtomicString& oldValue,
    const AtomicString& newValue) {
  CustomElement::enqueue(element,
                         new CustomElementAttributeChangedCallbackReaction(
                             this, name, oldValue, newValue));
}

void CustomElementDefinition::enqueueAttributeChangedCallbackForAllAttributes(
    Element* element) {
  // Avoid synchronizing all attributes unless it is needed, while enqueing
  // callbacks "in order" as defined in the spec.
  // https://html.spec.whatwg.org/multipage/scripting.html#concept-upgrade-an-element
  for (const AtomicString& name : m_observedAttributes)
    element->synchronizeAttribute(name);
  for (const auto& attribute : element->attributesWithoutUpdate()) {
    if (hasAttributeChangedCallback(attribute.name())) {
      enqueueAttributeChangedCallback(element, attribute.name(), nullAtom,
                                      attribute.value());
    }
  }
}

}  // namespace blink
