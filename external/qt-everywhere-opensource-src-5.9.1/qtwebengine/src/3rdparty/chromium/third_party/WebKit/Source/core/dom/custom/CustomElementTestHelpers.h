// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementTestHelpers_h
#define CustomElementTestHelpers_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDefinitionOptions.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "core/html/HTMLDocument.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

#include <utility>
#include <vector>

namespace blink {

class CustomElementDescriptor;

class TestCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(TestCustomElementDefinitionBuilder);

 public:
  TestCustomElementDefinitionBuilder() {}
  bool checkConstructorIntrinsics() override { return true; }
  bool checkConstructorNotRegistered() override { return true; }
  bool checkPrototype() override { return true; }
  bool rememberOriginalProperties() override { return true; }
  CustomElementDefinition* build(const CustomElementDescriptor&) override;
};

class TestCustomElementDefinition : public CustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(TestCustomElementDefinition);

 public:
  TestCustomElementDefinition(const CustomElementDescriptor& descriptor)
      : CustomElementDefinition(descriptor) {}

  TestCustomElementDefinition(const CustomElementDescriptor& descriptor,
                              const HashSet<AtomicString>& observedAttributes)
      : CustomElementDefinition(descriptor, observedAttributes) {}

  ~TestCustomElementDefinition() override = default;

  ScriptValue getConstructorForScript() override { return ScriptValue(); }

  bool runConstructor(Element* element) override {
    if (constructionStack().isEmpty() || constructionStack().last() != element)
      return false;
    constructionStack().last().clear();
    return true;
  }

  HTMLElement* createElementSync(Document& document,
                                 const QualifiedName&) override {
    return createElementForConstructor(document);
  }

  bool hasConnectedCallback() const override { return false; }
  bool hasDisconnectedCallback() const override { return false; }
  bool hasAdoptedCallback() const override { return false; }

  void runConnectedCallback(Element*) override {
    NOTREACHED() << "definition does not have connected callback";
  }

  void runDisconnectedCallback(Element*) override {
    NOTREACHED() << "definition does not have disconnected callback";
  }

  void runAdoptedCallback(Element*,
                          Document* oldOwner,
                          Document* newOwner) override {
    NOTREACHED() << "definition does not have adopted callback";
  }

  void runAttributeChangedCallback(Element*,
                                   const QualifiedName&,
                                   const AtomicString& oldValue,
                                   const AtomicString& newValue) override {
    NOTREACHED() << "definition does not have attribute changed callback";
  }
};

class CreateElement {
  STACK_ALLOCATED()
 public:
  CreateElement(const AtomicString& localName)
      : m_namespaceURI(HTMLNames::xhtmlNamespaceURI), m_localName(localName) {}

  CreateElement& inDocument(Document* document) {
    m_document = document;
    return *this;
  }

  CreateElement& inNamespace(const AtomicString& uri) {
    m_namespaceURI = uri;
    return *this;
  }

  CreateElement& withId(const AtomicString& id) {
    m_attributes.push_back(std::make_pair(HTMLNames::idAttr, id));
    return *this;
  }

  CreateElement& withIsAttribute(const AtomicString& value) {
    m_attributes.push_back(std::make_pair(HTMLNames::isAttr, value));
    return *this;
  }

  operator Element*() const {
    Document* document = m_document.get();
    if (!document)
      document = HTMLDocument::create();
    NonThrowableExceptionState noExceptions;
    Element* element =
        document->createElementNS(m_namespaceURI, m_localName, noExceptions);
    for (const auto& attribute : m_attributes)
      element->setAttribute(attribute.first, attribute.second);
    return element;
  }

 private:
  Member<Document> m_document;
  AtomicString m_namespaceURI;
  AtomicString m_localName;
  std::vector<std::pair<QualifiedName, AtomicString>> m_attributes;
};

}  // namespace blink

#endif  // CustomElementTestHelpers_h
