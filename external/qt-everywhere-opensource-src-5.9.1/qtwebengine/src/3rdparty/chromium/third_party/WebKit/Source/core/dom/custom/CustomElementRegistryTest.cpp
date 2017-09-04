// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDefinitionOptions.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementTestHelpers.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class CustomElementRegistryTest : public ::testing::Test {
 protected:
  void SetUp() {
    m_page.reset(DummyPageHolder::create(IntSize(1, 1)).release());
  }

  void TearDown() { m_page = nullptr; }

  Document& document() { return m_page->document(); }

  CustomElementRegistry& registry() {
    return *m_page->frame().localDOMWindow()->customElements();
  }

  ScriptState* scriptState() {
    return ScriptState::forMainWorld(&m_page->frame());
  }

  void collectCandidates(const CustomElementDescriptor& desc,
                         HeapVector<Member<Element>>* elements) {
    registry().collectCandidates(desc, elements);
  }

  ShadowRoot* attachShadowTo(Element* element) {
    NonThrowableExceptionState noExceptions;
    ShadowRootInit shadowRootInit;
    shadowRootInit.setMode("open");
    return element->attachShadow(scriptState(), shadowRootInit, noExceptions);
  }

 private:
  std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsRemovedFromDocument) {
  Element* element = CreateElement("a-a").inDocument(&document());
  registry().addCandidate(element);

  HeapVector<Member<Element>> elements;
  collectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_TRUE(elements.isEmpty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.contains(element))
      << "the out-of-document candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldNotIncludeElementsInDifferentDocument) {
  Element* element = CreateElement("a-a").inDocument(&document());
  registry().addCandidate(element);

  Document* otherDocument = HTMLDocument::create();
  otherDocument->appendChild(element);
  EXPECT_EQ(otherDocument, element->ownerDocument())
      << "sanity: another document should have adopted an element on append";

  HeapVector<Member<Element>> elements;
  collectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_TRUE(elements.isEmpty())
      << "no candidates should have been found, but we have "
      << elements.size();
  EXPECT_FALSE(elements.contains(element))
      << "the adopted-away candidate should not have been found";
}

TEST_F(CustomElementRegistryTest,
       collectCandidates_shouldOnlyIncludeCandidatesMatchingDescriptor) {
  CustomElementDescriptor descriptor("hello-world", "hello-world");

  // Does not match: namespace is not HTML
  Element* elementA = CreateElement("hello-world")
                          .inDocument(&document())
                          .inNamespace("data:text/date,1981-03-10");
  // Matches
  Element* elementB = CreateElement("hello-world").inDocument(&document());
  // Does not match: local name is not hello-world
  Element* elementC = CreateElement("button")
                          .inDocument(&document())
                          .withIsAttribute("hello-world");
  document().documentElement()->appendChild(elementA);
  elementA->appendChild(elementB);
  elementA->appendChild(elementC);

  registry().addCandidate(elementA);
  registry().addCandidate(elementB);
  registry().addCandidate(elementC);

  HeapVector<Member<Element>> elements;
  collectCandidates(descriptor, &elements);

  EXPECT_EQ(1u, elements.size())
      << "only one candidates should have been found";
  EXPECT_EQ(elementB, elements[0])
      << "the matching element should have been found";
}

TEST_F(CustomElementRegistryTest, collectCandidates_oneCandidate) {
  Element* element = CreateElement("a-a").inDocument(&document());
  registry().addCandidate(element);
  document().documentElement()->appendChild(element);

  HeapVector<Member<Element>> elements;
  collectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_EQ(1u, elements.size())
      << "exactly one candidate should have been found";
  EXPECT_TRUE(elements.contains(element))
      << "the candidate should be the element that was added";
}

TEST_F(CustomElementRegistryTest, collectCandidates_shouldBeInDocumentOrder) {
  CreateElement factory = CreateElement("a-a");
  factory.inDocument(&document());
  Element* elementA = factory.withId("a");
  Element* elementB = factory.withId("b");
  Element* elementC = factory.withId("c");

  registry().addCandidate(elementB);
  registry().addCandidate(elementA);
  registry().addCandidate(elementC);

  document().documentElement()->appendChild(elementA);
  elementA->appendChild(elementB);
  document().documentElement()->appendChild(elementC);

  HeapVector<Member<Element>> elements;
  collectCandidates(CustomElementDescriptor("a-a", "a-a"), &elements);

  EXPECT_EQ(elementA, elements[0].get());
  EXPECT_EQ(elementB, elements[1].get());
  EXPECT_EQ(elementC, elements[2].get());
}

// Classes which use trace macros cannot be local because of the
// traceImpl template.
class LogUpgradeDefinition : public TestCustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(LogUpgradeDefinition);

 public:
  LogUpgradeDefinition(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(
            descriptor,
            {
                "attr1", "attr2", HTMLNames::contenteditableAttr.localName(),
            }) {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    TestCustomElementDefinition::trace(visitor);
    visitor->trace(m_element);
    visitor->trace(m_adopted);
  }

  // TODO(dominicc): Make this class collect a vector of what's
  // upgraded; it will be useful in more tests.
  Member<Element> m_element;
  enum MethodType {
    Constructor,
    ConnectedCallback,
    DisconnectedCallback,
    AdoptedCallback,
    AttributeChangedCallback,
  };
  Vector<MethodType> m_logs;

  struct AttributeChanged {
    QualifiedName name;
    AtomicString oldValue;
    AtomicString newValue;
  };
  Vector<AttributeChanged> m_attributeChanged;

  struct Adopted : public GarbageCollected<Adopted> {
    Adopted(Document* oldOwner, Document* newOwner)
        : m_oldOwner(oldOwner), m_newOwner(newOwner) {}

    Member<Document> m_oldOwner;
    Member<Document> m_newOwner;

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_oldOwner);
      visitor->trace(m_newOwner);
    }
  };
  HeapVector<Member<Adopted>> m_adopted;

  void clear() {
    m_logs.clear();
    m_attributeChanged.clear();
  }

  bool runConstructor(Element* element) override {
    m_logs.append(Constructor);
    m_element = element;
    return TestCustomElementDefinition::runConstructor(element);
  }

  bool hasConnectedCallback() const override { return true; }
  bool hasDisconnectedCallback() const override { return true; }
  bool hasAdoptedCallback() const override { return true; }

  void runConnectedCallback(Element* element) override {
    m_logs.append(ConnectedCallback);
    EXPECT_EQ(element, m_element);
  }

  void runDisconnectedCallback(Element* element) override {
    m_logs.append(DisconnectedCallback);
    EXPECT_EQ(element, m_element);
  }

  void runAdoptedCallback(Element* element,
                          Document* oldOwner,
                          Document* newOwner) override {
    m_logs.append(AdoptedCallback);
    EXPECT_EQ(element, m_element);
    m_adopted.append(new Adopted(oldOwner, newOwner));
  }

  void runAttributeChangedCallback(Element* element,
                                   const QualifiedName& name,
                                   const AtomicString& oldValue,
                                   const AtomicString& newValue) override {
    m_logs.append(AttributeChangedCallback);
    EXPECT_EQ(element, m_element);
    m_attributeChanged.append(AttributeChanged{name, oldValue, newValue});
  }
};

class LogUpgradeBuilder final : public TestCustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(LogUpgradeBuilder);

 public:
  LogUpgradeBuilder() {}

  CustomElementDefinition* build(
      const CustomElementDescriptor& descriptor) override {
    return new LogUpgradeDefinition(descriptor);
  }
};

TEST_F(CustomElementRegistryTest, define_upgradesInDocumentElements) {
  ScriptForbiddenScope doNotRelyOnScript;

  Element* element = CreateElement("a-a").inDocument(&document());
  element->setAttribute(
      QualifiedName(nullAtom, "attr1", HTMLNames::xhtmlNamespaceURI), "v1");
  element->setBooleanAttribute(HTMLNames::contenteditableAttr, true);
  document().documentElement()->appendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState shouldNotThrow;
  {
    CEReactionsScope reactions;
    registry().define("a-a", builder, ElementDefinitionOptions(),
                      shouldNotThrow);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(registry().definitionForName("a-a"));
  EXPECT_EQ(LogUpgradeDefinition::Constructor, definition->m_logs[0])
      << "defining the element should have 'upgraded' the existing element";
  EXPECT_EQ(element, definition->m_element)
      << "the existing a-a element should have been upgraded";

  EXPECT_EQ(LogUpgradeDefinition::AttributeChangedCallback,
            definition->m_logs[1])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("attr1", definition->m_attributeChanged[0].name);
  EXPECT_EQ(nullAtom, definition->m_attributeChanged[0].oldValue);
  EXPECT_EQ("v1", definition->m_attributeChanged[0].newValue);

  EXPECT_EQ(LogUpgradeDefinition::AttributeChangedCallback,
            definition->m_logs[2])
      << "Upgrade should invoke attributeChangedCallback for all attributes";
  EXPECT_EQ("contenteditable", definition->m_attributeChanged[1].name);
  EXPECT_EQ(nullAtom, definition->m_attributeChanged[1].oldValue);
  EXPECT_EQ(emptyAtom, definition->m_attributeChanged[1].newValue);
  EXPECT_EQ(2u, definition->m_attributeChanged.size())
      << "Upgrade should invoke attributeChangedCallback for all attributes";

  EXPECT_EQ(LogUpgradeDefinition::ConnectedCallback, definition->m_logs[3])
      << "upgrade should invoke connectedCallback";

  EXPECT_EQ(4u, definition->m_logs.size())
      << "upgrade should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, attributeChangedCallback) {
  ScriptForbiddenScope doNotRelyOnScript;

  Element* element = CreateElement("a-a").inDocument(&document());
  document().documentElement()->appendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState shouldNotThrow;
  {
    CEReactionsScope reactions;
    registry().define("a-a", builder, ElementDefinitionOptions(),
                      shouldNotThrow);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(registry().definitionForName("a-a"));

  definition->clear();
  {
    CEReactionsScope reactions;
    element->setAttribute(
        QualifiedName(nullAtom, "attr2", HTMLNames::xhtmlNamespaceURI), "v2");
  }
  EXPECT_EQ(LogUpgradeDefinition::AttributeChangedCallback,
            definition->m_logs[0])
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ(1u, definition->m_attributeChanged.size())
      << "Adding an attribute should invoke attributeChangedCallback";
  EXPECT_EQ("attr2", definition->m_attributeChanged[0].name);
  EXPECT_EQ(nullAtom, definition->m_attributeChanged[0].oldValue);
  EXPECT_EQ("v2", definition->m_attributeChanged[0].newValue);

  EXPECT_EQ(1u, definition->m_logs.size())
      << "upgrade should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, disconnectedCallback) {
  ScriptForbiddenScope doNotRelyOnScript;

  Element* element = CreateElement("a-a").inDocument(&document());
  document().documentElement()->appendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState shouldNotThrow;
  {
    CEReactionsScope reactions;
    registry().define("a-a", builder, ElementDefinitionOptions(),
                      shouldNotThrow);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(registry().definitionForName("a-a"));

  definition->clear();
  {
    CEReactionsScope reactions;
    element->remove(shouldNotThrow);
  }
  EXPECT_EQ(LogUpgradeDefinition::DisconnectedCallback, definition->m_logs[0])
      << "remove() should invoke disconnectedCallback";

  EXPECT_EQ(1u, definition->m_logs.size())
      << "remove() should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, adoptedCallback) {
  ScriptForbiddenScope doNotRelyOnScript;

  Element* element = CreateElement("a-a").inDocument(&document());
  document().documentElement()->appendChild(element);

  LogUpgradeBuilder builder;
  NonThrowableExceptionState shouldNotThrow;
  {
    CEReactionsScope reactions;
    registry().define("a-a", builder, ElementDefinitionOptions(),
                      shouldNotThrow);
  }
  LogUpgradeDefinition* definition =
      static_cast<LogUpgradeDefinition*>(registry().definitionForName("a-a"));

  definition->clear();
  Document* otherDocument = HTMLDocument::create();
  {
    CEReactionsScope reactions;
    otherDocument->adoptNode(element, ASSERT_NO_EXCEPTION);
  }
  EXPECT_EQ(LogUpgradeDefinition::DisconnectedCallback, definition->m_logs[0])
      << "adoptNode() should invoke disconnectedCallback";

  EXPECT_EQ(LogUpgradeDefinition::AdoptedCallback, definition->m_logs[1])
      << "adoptNode() should invoke adoptedCallback";

  EXPECT_EQ(&document(), definition->m_adopted[0]->m_oldOwner.get())
      << "adoptedCallback should have been passed the old owner document";
  EXPECT_EQ(otherDocument, definition->m_adopted[0]->m_newOwner.get())
      << "adoptedCallback should have been passed the new owner document";

  EXPECT_EQ(2u, definition->m_logs.size())
      << "adoptNode() should not invoke other callbacks";
}

TEST_F(CustomElementRegistryTest, lookupCustomElementDefinition) {
  NonThrowableExceptionState shouldNotThrow;
  TestCustomElementDefinitionBuilder builder;
  CustomElementDefinition* definitionA = registry().define(
      "a-a", builder, ElementDefinitionOptions(), shouldNotThrow);
  ElementDefinitionOptions options;
  options.setExtends("div");
  CustomElementDefinition* definitionB =
      registry().define("b-b", builder, options, shouldNotThrow);
  // look up defined autonomous custom element
  CustomElementDefinition* definition = registry().definitionFor(
      CustomElementDescriptor(CustomElementDescriptor("a-a", "a-a")));
  EXPECT_NE(nullptr, definition) << "a-a, a-a should be registered";
  EXPECT_EQ(definitionA, definition);
  // look up undefined autonomous custom element
  definition = registry().definitionFor(CustomElementDescriptor("a-a", "div"));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
  // look up defined customized built-in element
  definition = registry().definitionFor(CustomElementDescriptor("b-b", "div"));
  EXPECT_NE(nullptr, definition) << "b-b, div should be registered";
  EXPECT_EQ(definitionB, definition);
  // look up undefined customized built-in element
  definition = registry().definitionFor(CustomElementDescriptor("a-a", "div"));
  EXPECT_EQ(nullptr, definition) << "a-a, div should not be registered";
}

// TODO(dominicc): Add tests which adjust the "is" attribute when type
// extensions are implemented.

}  // namespace blink
