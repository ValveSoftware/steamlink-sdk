// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotScopedTraversal.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class SlotScopedTraversalTest : public ::testing::Test {
 protected:
  Document& document() const;

  void setupSampleHTML(const char* mainHTML, const char* shadowHTML, unsigned);
  void attachOpenShadowRoot(Element& shadowHost, const char* shadowInnerHTML);
  void testExpectedSequence(const Vector<Element*>& list);

 private:
  void SetUp() override;

  Persistent<Document> m_document;
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void SlotScopedTraversalTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_document = &m_dummyPageHolder->document();
  DCHECK(m_document);
}

Document& SlotScopedTraversalTest::document() const {
  return *m_document;
}

void SlotScopedTraversalTest::setupSampleHTML(const char* mainHTML,
                                              const char* shadowHTML,
                                              unsigned index) {
  Element* body = document().body();
  body->setInnerHTML(String::fromUTF8(mainHTML));
  if (shadowHTML) {
    Element* shadowHost = toElement(NodeTraversal::childAt(*body, index));
    attachOpenShadowRoot(*shadowHost, shadowHTML);
  }
}

void SlotScopedTraversalTest::attachOpenShadowRoot(
    Element& shadowHost,
    const char* shadowInnerHTML) {
  ShadowRoot* shadowRoot = shadowHost.createShadowRootInternal(
      ShadowRootType::Open, ASSERT_NO_EXCEPTION);
  shadowRoot->setInnerHTML(String::fromUTF8(shadowInnerHTML));
  document().body()->updateDistribution();
}

TEST_F(SlotScopedTraversalTest, emptySlot) {
  const char* mainHTML = "<div id='host'></div>";
  const char* shadowHTML = "<slot></slot>";
  setupSampleHTML(mainHTML, shadowHTML, 0);

  Element* host = document().querySelector("#host");
  ShadowRoot* shadowRoot = host->openShadowRoot();
  Element* slotElement = shadowRoot->querySelector("slot");
  DCHECK(isHTMLSlotElement(slotElement));
  HTMLSlotElement* slot = toHTMLSlotElement(slotElement);

  EXPECT_EQ(nullptr, SlotScopedTraversal::firstAssignedToSlot(*slot));
  EXPECT_EQ(nullptr, SlotScopedTraversal::lastAssignedToSlot(*slot));
}

void SlotScopedTraversalTest::testExpectedSequence(
    const Vector<Element*>& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    const Element* expected = i == list.size() - 1 ? nullptr : list[i + 1];
    EXPECT_EQ(expected, SlotScopedTraversal::next(*list[i]));
  }

  for (size_t i = list.size(); i > 0; --i) {
    const Element* expected = i == 1 ? nullptr : list[i - 2];
    EXPECT_EQ(expected, SlotScopedTraversal::previous(*list[i - 1]));
  }
}

TEST_F(SlotScopedTraversalTest, simpleSlot) {
  const char* mainHTML =
      "<div id='host'>"
      "<div id='inner1'></div>"
      "<div id='inner2'></div>"
      "</div>";

  const char* shadowHTML = "<slot></slot>";

  setupSampleHTML(mainHTML, shadowHTML, 0);

  Element* host = document().querySelector("#host");
  Element* inner1 = document().querySelector("#inner1");
  Element* inner2 = document().querySelector("#inner2");
  ShadowRoot* shadowRoot = host->openShadowRoot();
  Element* slotElement = shadowRoot->querySelector("slot");
  DCHECK(isHTMLSlotElement(slotElement));
  HTMLSlotElement* slot = toHTMLSlotElement(slotElement);

  EXPECT_EQ(inner1, SlotScopedTraversal::firstAssignedToSlot(*slot));
  EXPECT_EQ(inner2, SlotScopedTraversal::lastAssignedToSlot(*slot));
  EXPECT_FALSE(SlotScopedTraversal::isSlotScoped(*host));
  EXPECT_FALSE(SlotScopedTraversal::isSlotScoped(*slot));
  EXPECT_TRUE(SlotScopedTraversal::isSlotScoped(*inner1));
  EXPECT_TRUE(SlotScopedTraversal::isSlotScoped(*inner2));
  EXPECT_EQ(inner1, SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(
                        *inner1));
  EXPECT_EQ(inner2, SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(
                        *inner2));
  EXPECT_EQ(slot, SlotScopedTraversal::findScopeOwnerSlot(*inner1));
  EXPECT_EQ(slot, SlotScopedTraversal::findScopeOwnerSlot(*inner2));

  Vector<Element*> expectedSequence({inner1, inner2});
  testExpectedSequence(expectedSequence);
}

TEST_F(SlotScopedTraversalTest, multipleSlots) {
  const char* mainHTML =
      "<div id='host'>"
      "<div id='inner0' slot='slot0'></div>"
      "<div id='inner1' slot='slot1'></div>"
      "<div id='inner2'></div>"
      "<div id='inner3' slot='slot1'></div>"
      "<div id='inner4' slot='slot0'></div>"
      "<div id='inner5'></div>"
      "</div>";

  const char* shadowHTML =
      "<slot id='unnamedslot'></slot>"
      "<slot id='slot0' name='slot0'></slot>"
      "<slot id='slot1' name='slot1'></slot>";

  setupSampleHTML(mainHTML, shadowHTML, 0);

  Element* host = document().querySelector("#host");
  Element* inner[6];
  inner[0] = document().querySelector("#inner0");
  inner[1] = document().querySelector("#inner1");
  inner[2] = document().querySelector("#inner2");
  inner[3] = document().querySelector("#inner3");
  inner[4] = document().querySelector("#inner4");
  inner[5] = document().querySelector("#inner5");

  ShadowRoot* shadowRoot = host->openShadowRoot();
  Element* slotElement[3];
  slotElement[0] = shadowRoot->querySelector("#slot0");
  slotElement[1] = shadowRoot->querySelector("#slot1");
  slotElement[2] = shadowRoot->querySelector("#unnamedslot");

  HTMLSlotElement* slot[3];
  slot[0] = toHTMLSlotElement(slotElement[0]);
  slot[1] = toHTMLSlotElement(slotElement[1]);
  slot[2] = toHTMLSlotElement(slotElement[2]);

  {
    // <slot id='slot0'> : Expected assigned nodes: inner0, inner4
    EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot[0]));
    EXPECT_EQ(inner[4], SlotScopedTraversal::lastAssignedToSlot(*slot[0]));
    Vector<Element*> expectedSequence({inner[0], inner[4]});
    testExpectedSequence(expectedSequence);
  }

  {
    // <slot name='slot1'> : Expected assigned nodes: inner1, inner3
    EXPECT_EQ(inner[1], SlotScopedTraversal::firstAssignedToSlot(*slot[1]));
    EXPECT_EQ(inner[3], SlotScopedTraversal::lastAssignedToSlot(*slot[1]));
    Vector<Element*> expectedSequence({inner[1], inner[3]});
    testExpectedSequence(expectedSequence);
  }

  {
    // <slot id='unnamedslot'> : Expected assigned nodes: inner2, inner5
    EXPECT_EQ(inner[2], SlotScopedTraversal::firstAssignedToSlot(*slot[2]));
    EXPECT_EQ(inner[5], SlotScopedTraversal::lastAssignedToSlot(*slot[2]));
    Vector<Element*> expectedSequence({inner[2], inner[5]});
    testExpectedSequence(expectedSequence);
  }
}

TEST_F(SlotScopedTraversalTest, shadowHostAtTopLevel) {
  // This covers a shadow host is directly assigned to a slot.
  //
  // We build the following tree:
  //         host
  //           |
  //       ShadowRoot
  //           |
  //     ____<slot>____
  //     |      |      |
  //   inner0 inner1 inner2
  //     |      |      |
  //   child0 child1 child2
  //
  // And iterate on inner0, inner1, and inner2 to attach shadow on each of
  // them, and check if elements that are slotted to another slot are skipped
  // in traversal.

  const char* mainHTML =
      "<div id='host'>"
      "<div id='inner0'><div id='child0'></div></div>"
      "<div id='inner1'><div id='child1'></div></div>"
      "<div id='inner2'><div id='child2'></div></div>"
      "</div>";

  const char* shadowHTML = "<slot></slot>";

  for (int i = 0; i < 3; ++i) {
    setupSampleHTML(mainHTML, shadowHTML, 0);

    Element* host = document().querySelector("#host");
    Element* inner[3];
    inner[0] = document().querySelector("#inner0");
    inner[1] = document().querySelector("#inner1");
    inner[2] = document().querySelector("#inner2");
    Element* child[3];
    child[0] = document().querySelector("#child0");
    child[1] = document().querySelector("#child1");
    child[2] = document().querySelector("#child2");

    attachOpenShadowRoot(*inner[i], shadowHTML);

    ShadowRoot* shadowRoot = host->openShadowRoot();
    Element* slotElement = shadowRoot->querySelector("slot");
    DCHECK(isHTMLSlotElement(slotElement));
    HTMLSlotElement* slot = toHTMLSlotElement(slotElement);

    switch (i) {
      case 0: {
        // inner0 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(child[2], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*child[0]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*child[0]));

        Vector<Element*> expectedSequence(
            {inner[0], inner[1], child[1], inner[2], child[2]});
        testExpectedSequence(expectedSequence);
      } break;

      case 1: {
        // inner1 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(child[2], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*child[1]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*child[1]));

        Vector<Element*> expectedSequence(
            {inner[0], child[0], inner[1], inner[2], child[2]});
        testExpectedSequence(expectedSequence);
      } break;

      case 2: {
        // inner2 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(inner[2], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*child[2]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*child[2]));

        Vector<Element*> expectedSequence(
            {inner[0], child[0], inner[1], child[1], inner[2]});
        testExpectedSequence(expectedSequence);
      } break;
    }
  }
}

TEST_F(SlotScopedTraversalTest, shadowHostAtSecondLevel) {
  // This covers cases where a shadow host exists in a child of a slotted
  // element.
  //
  // We build the following tree:
  //            host
  //              |
  //          ShadowRoot
  //              |
  //        ____<slot>____
  //        |             |
  //     _inner0_      _inner1_
  //     |      |      |      |
  //   child0 child1 child2 child3
  //     |      |      |      |
  //    span0  span1  span2  span3
  //
  // And iterate on child0, child1, child2, and child3 to attach shadow on each
  // of them, and check if elements that are slotted to another slot are skipped
  // in traversal.

  const char* mainHTML =
      "<div id='host'>"
      "<div id='inner0'>"
      "<div id='child0'><span id='span0'></span></div>"
      "<div id='child1'><span id='span1'></span></div>"
      "</div>"
      "<div id='inner1'>"
      "<div id='child2'><span id='span2'></span></div>"
      "<div id='child3'><span id='span3'></span></div>"
      "</div>"
      "</div>";

  const char* shadowHTML = "<slot></slot>";

  for (int i = 0; i < 4; ++i) {
    setupSampleHTML(mainHTML, shadowHTML, 0);

    Element* host = document().querySelector("#host");
    Element* inner[2];
    inner[0] = document().querySelector("#inner0");
    inner[1] = document().querySelector("#inner1");
    Element* child[4];
    child[0] = document().querySelector("#child0");
    child[1] = document().querySelector("#child1");
    child[2] = document().querySelector("#child2");
    child[3] = document().querySelector("#child3");
    Element* span[4];
    span[0] = document().querySelector("#span0");
    span[1] = document().querySelector("#span1");
    span[2] = document().querySelector("#span2");
    span[3] = document().querySelector("#span3");

    attachOpenShadowRoot(*child[i], shadowHTML);

    for (int j = 0; j < 4; ++j) {
      DCHECK(child[i]);
      DCHECK(span[i]);
    }

    ShadowRoot* shadowRoot = host->openShadowRoot();
    Element* slotElement = shadowRoot->querySelector("slot");
    DCHECK(isHTMLSlotElement(slotElement));
    HTMLSlotElement* slot = toHTMLSlotElement(slotElement);

    switch (i) {
      case 0: {
        // child0 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*span[0]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*span[0]));

        const Vector<Element*> expectedSequence({inner[0], child[0], child[1],
                                                 span[1], inner[1], child[2],
                                                 span[2], child[3], span[3]});
        testExpectedSequence(expectedSequence);
      } break;

      case 1: {
        // child1 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*span[1]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*span[1]));

        const Vector<Element*> expectedSequence({inner[0], child[0], span[0],
                                                 child[1], inner[1], child[2],
                                                 span[2], child[3], span[3]});
        testExpectedSequence(expectedSequence);
      } break;

      case 2: {
        // child2 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*span[2]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*span[2]));

        const Vector<Element*> expectedSequence({inner[0], child[0], span[0],
                                                 child[1], span[1], inner[1],
                                                 child[2], child[3], span[3]});
        testExpectedSequence(expectedSequence);
      } break;

      case 3: {
        // child3 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::firstAssignedToSlot(*slot));
        EXPECT_EQ(child[3], SlotScopedTraversal::lastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::next(*span[3]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::previous(*span[3]));

        const Vector<Element*> expectedSequence({inner[0], child[0], span[0],
                                                 child[1], span[1], inner[1],
                                                 child[2], span[2], child[3]});
        testExpectedSequence(expectedSequence);
      } break;
    }
  }
}

}  // namespace blink
