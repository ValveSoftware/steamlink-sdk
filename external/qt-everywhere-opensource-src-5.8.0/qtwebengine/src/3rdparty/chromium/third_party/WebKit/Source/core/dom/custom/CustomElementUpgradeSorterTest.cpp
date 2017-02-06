// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementUpgradeSorter.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/html/HTMLDocument.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class CustomElementUpgradeSorterTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_page = DummyPageHolder::create(IntSize(1, 1));
    }

    void TearDown() override
    {
        m_page = nullptr;
    }

    Element* createElementWithId(const char* localName, const char* id)
    {
        NonThrowableExceptionState noExceptions;
        Element* element =
            document()->createElement(localName, AtomicString(), noExceptions);
        element->setAttribute(HTMLNames::idAttr, id);
        return element;
    }

    Document* document() { return &m_page->document(); }

    ScriptState* scriptState()
    {
        return ScriptState::forMainWorld(&m_page->frame());
    }

    ShadowRoot* attachShadowTo(Element* element)
    {
        NonThrowableExceptionState noExceptions;
        ShadowRootInit shadowRootInit;
        return
            element->attachShadow(scriptState(), shadowRootInit, noExceptions);
    }

private:
    std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(CustomElementUpgradeSorterTest, inOtherDocument_notInSet)
{
    NonThrowableExceptionState noExceptions;
    Element* element =
        document()->createElement("a-a", AtomicString(), noExceptions);

    Document* otherDocument = HTMLDocument::create();
    otherDocument->appendChild(element);
    EXPECT_EQ(otherDocument, element->ownerDocument())
        << "sanity: another document should have adopted an element on append";

    CustomElementUpgradeSorter sorter;
    sorter.add(element);

    HeapVector<Member<Element>> elements;
    sorter.sorted(&elements, document());
    EXPECT_EQ(0u, elements.size())
        << "the adopted-away candidate should not have been included";
}

TEST_F(CustomElementUpgradeSorterTest, oneCandidate)
{
    NonThrowableExceptionState noExceptions;
    Element* element =
        document()->createElement("a-a", AtomicString(), noExceptions);
    document()->documentElement()->appendChild(element);

    CustomElementUpgradeSorter sorter;
    sorter.add(element);

    HeapVector<Member<Element>> elements;
    sorter.sorted(&elements, document());
    EXPECT_EQ(1u, elements.size())
        << "exactly one candidate should be in the result set";
    EXPECT_TRUE(elements.contains(element))
        << "the candidate should be the element that was added";
}

TEST_F(CustomElementUpgradeSorterTest, candidatesInDocumentOrder)
{
    Element* a = createElementWithId("a-a", "a");
    Element* b = createElementWithId("a-a", "b");
    Element* c = createElementWithId("a-a", "c");

    document()->documentElement()->appendChild(a);
    a->appendChild(b);
    document()->documentElement()->appendChild(c);

    CustomElementUpgradeSorter sorter;
    sorter.add(b);
    sorter.add(a);
    sorter.add(c);

    HeapVector<Member<Element>> elements;
    sorter.sorted(&elements, document());
    EXPECT_EQ(3u, elements.size());
    EXPECT_EQ(a, elements[0].get());
    EXPECT_EQ(b, elements[1].get());
    EXPECT_EQ(c, elements[2].get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_ancestorInSet)
{
    // A*
    // + B
    //   + C*
    Element* a = createElementWithId("a-a", "a");
    Element* b = createElementWithId("a-a", "b");
    Element* c = createElementWithId("a-a", "c");

    document()->documentElement()->appendChild(a);
    a->appendChild(b);
    b->appendChild(c);

    CustomElementUpgradeSorter sort;
    sort.add(c);
    sort.add(a);

    HeapVector<Member<Element>> elements;
    sort.sorted(&elements, document());
    EXPECT_EQ(2u, elements.size());
    EXPECT_EQ(a, elements[0].get());
    EXPECT_EQ(c, elements[1].get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_deepShallow)
{
    // A
    // + B*
    // C*
    Element* a = createElementWithId("a-a", "a");
    Element* b = createElementWithId("a-a", "b");
    Element* c = createElementWithId("a-a", "c");

    document()->documentElement()->appendChild(a);
    a->appendChild(b);
    document()->documentElement()->appendChild(c);

    CustomElementUpgradeSorter sort;
    sort.add(b);
    sort.add(c);

    HeapVector<Member<Element>> elements;
    sort.sorted(&elements, document());
    EXPECT_EQ(2u, elements.size());
    EXPECT_EQ(b, elements[0].get());
    EXPECT_EQ(c, elements[1].get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_shallowDeep)
{
    // A*
    // B
    // + C*
    Element* a = createElementWithId("a-a", "a");
    Element* b = createElementWithId("a-a", "b");
    Element* c = createElementWithId("a-a", "c");

    document()->documentElement()->appendChild(a);
    document()->documentElement()->appendChild(b);
    b->appendChild(c);

    CustomElementUpgradeSorter sort;
    sort.add(a);
    sort.add(c);

    HeapVector<Member<Element>> elements;
    sort.sorted(&elements, document());
    EXPECT_EQ(2u, elements.size());
    EXPECT_EQ(a, elements[0].get());
    EXPECT_EQ(c, elements[1].get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_shadow)
{
    // A*
    // + {ShadowRoot}
    // | + B
    // |   + C*
    // + D*
    Element* a = createElementWithId("a-a", "a");
    Element* b = createElementWithId("a-a", "b");
    Element* c = createElementWithId("a-a", "c");
    Element* d = createElementWithId("a-a", "d");

    document()->documentElement()->appendChild(a);
    ShadowRoot* s = attachShadowTo(a);
    a->appendChild(d);

    s->appendChild(b);
    b->appendChild(c);

    CustomElementUpgradeSorter sort;
    sort.add(a);
    sort.add(c);
    sort.add(d);

    HeapVector<Member<Element>> elements;
    sort.sorted(&elements, document());
    EXPECT_EQ(3u, elements.size());
    EXPECT_EQ(a, elements[0].get());
    EXPECT_EQ(c, elements[1].get());
    EXPECT_EQ(d, elements[2].get());
}

} // namespace blink
