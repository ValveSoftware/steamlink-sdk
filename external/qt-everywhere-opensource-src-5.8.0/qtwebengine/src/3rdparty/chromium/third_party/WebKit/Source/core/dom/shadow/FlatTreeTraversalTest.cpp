// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/FlatTreeTraversal.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Compiler.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class FlatTreeTraversalTest : public ::testing::Test {
protected:
    HTMLDocument& document() const;

    // Sets |mainHTML| to BODY element with |innerHTML| property and attaches
    // shadow root to child with |shadowHTML|, then update distribution for
    // calling member functions in |FlatTreeTraversal|.
    void setupSampleHTML(const char* mainHTML, const char* shadowHTML, unsigned);

    void setupDocumentTree(const char* mainHTML);

    void attachV0ShadowRoot(Element& shadowHost, const char* shadowInnerHTML);
    void attachOpenShadowRoot(Element& shadowHost, const char* shadowInnerHTML);

private:
    void SetUp() override;

    Persistent<HTMLDocument> m_document;
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void FlatTreeTraversalTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    DCHECK(m_document);
}

HTMLDocument& FlatTreeTraversalTest::document() const
{
    return *m_document;
}

void FlatTreeTraversalTest::setupSampleHTML(const char* mainHTML, const char* shadowHTML, unsigned index)
{
    Element* body = document().body();
    body->setInnerHTML(String::fromUTF8(mainHTML), ASSERT_NO_EXCEPTION);
    Element* shadowHost = toElement(NodeTraversal::childAt(*body, index));
    ShadowRoot* shadowRoot = shadowHost->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowHTML), ASSERT_NO_EXCEPTION);
    body->updateDistribution();
}

void FlatTreeTraversalTest::setupDocumentTree(const char* mainHTML)
{
    Element* body = document().body();
    body->setInnerHTML(String::fromUTF8(mainHTML), ASSERT_NO_EXCEPTION);
}

void FlatTreeTraversalTest::attachV0ShadowRoot(Element& shadowHost, const char* shadowInnerHTML)
{
    ShadowRoot* shadowRoot = shadowHost.createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowInnerHTML), ASSERT_NO_EXCEPTION);
    document().body()->updateDistribution();
}

void FlatTreeTraversalTest::attachOpenShadowRoot(Element& shadowHost, const char* shadowInnerHTML)
{
    ShadowRoot* shadowRoot = shadowHost.createShadowRootInternal(ShadowRootType::Open, ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowInnerHTML), ASSERT_NO_EXCEPTION);
    document().body()->updateDistribution();
}

void testCommonAncestor(Node* expectedResult, const Node& nodeA, const Node& nodeB)
{
    Node* result1 = FlatTreeTraversal::commonAncestor(nodeA, nodeB);
    EXPECT_EQ(expectedResult, result1) << "commonAncestor(" << nodeA.textContent() << "," << nodeB.textContent() << ")";
    Node* result2 = FlatTreeTraversal::commonAncestor(nodeB, nodeA);
    EXPECT_EQ(expectedResult, result2) << "commonAncestor(" << nodeB.textContent() << "," << nodeA.textContent() << ")";
}

// Test case for
//  - childAt
//  - countChildren
//  - hasChildren
//  - index
//  - isDescendantOf
TEST_F(FlatTreeTraversalTest, childAt)
{
    const char* mainHTML =
        "<div id='m0'>"
            "<span id='m00'>m00</span>"
            "<span id='m01'>m01</span>"
        "</div>";
    const char* shadowHTML =
        "<a id='s00'>s00</a>"
        "<content select='#m01'></content>"
        "<a id='s02'>s02</a>"
        "<a id='s03'><content select='#m00'></content></a>"
        "<a id='s04'>s04</a>";
    setupSampleHTML(mainHTML, shadowHTML, 0);

    Element* body = document().body();
    Element* m0 = body->querySelector("#m0", ASSERT_NO_EXCEPTION);
    Element* m00 = m0->querySelector("#m00", ASSERT_NO_EXCEPTION);
    Element* m01 = m0->querySelector("#m01", ASSERT_NO_EXCEPTION);

    Element* shadowHost = m0;
    ShadowRoot* shadowRoot = shadowHost->openShadowRoot();
    Element* s00 = shadowRoot->querySelector("#s00", ASSERT_NO_EXCEPTION);
    Element* s02 = shadowRoot->querySelector("#s02", ASSERT_NO_EXCEPTION);
    Element* s03 = shadowRoot->querySelector("#s03", ASSERT_NO_EXCEPTION);
    Element* s04 = shadowRoot->querySelector("#s04", ASSERT_NO_EXCEPTION);

    const unsigned numberOfChildNodes = 5;
    Node* expectedChildNodes[5] = { s00, m01, s02, s03, s04 };

    ASSERT_EQ(numberOfChildNodes, FlatTreeTraversal::countChildren(*shadowHost));
    EXPECT_TRUE(FlatTreeTraversal::hasChildren(*shadowHost));

    for (unsigned index = 0; index < numberOfChildNodes; ++index) {
        Node* child = FlatTreeTraversal::childAt(*shadowHost, index);
        EXPECT_EQ(expectedChildNodes[index], child)
            << "FlatTreeTraversal::childAt(*shadowHost, " << index << ")";
        EXPECT_EQ(index, FlatTreeTraversal::index(*child))
            << "FlatTreeTraversal::index(FlatTreeTraversal(*shadowHost, " << index << "))";
        EXPECT_TRUE(FlatTreeTraversal::isDescendantOf(*child, *shadowHost))
            << "FlatTreeTraversal::isDescendantOf(*FlatTreeTraversal(*shadowHost, " << index << "), *shadowHost)";
    }
    EXPECT_EQ(nullptr, FlatTreeTraversal::childAt(*shadowHost, numberOfChildNodes + 1))
        << "Out of bounds childAt() returns nullptr.";

    // Distribute node |m00| is child of node in shadow tree |s03|.
    EXPECT_EQ(m00, FlatTreeTraversal::childAt(*s03, 0));
}

// Test case for
//  - commonAncestor
//  - isDescendantOf
TEST_F(FlatTreeTraversalTest, commonAncestor)
{
    // We build following flat tree:
    //             ____BODY___
    //             |    |     |
    //            m0    m1    m2       m1 is shadow host having m10, m11, m12.
    //            _|_   |   __|__
    //           |   |  |   |    |
    //          m00 m01 |   m20 m21
    //             _____|_____________
    //             |  |   |    |     |
    //            s10 s11 s12 s13  s14
    //                         |
    //                       __|__
    //                |      |    |
    //                m12    m10 m11 <-- distributed
    // where: each symbol consists with prefix, child index, child-child index.
    //  prefix "m" means node in main tree,
    //  prefix "d" means node in main tree and distributed
    //  prefix "s" means node in shadow tree
    const char* mainHTML =
        "<a id='m0'><b id='m00'>m00</b><b id='m01'>m01</b></a>"
        "<a id='m1'>"
            "<b id='m10'>m10</b>"
            "<b id='m11'>m11</b>"
            "<b id='m12'>m12</b>"
        "</a>"
        "<a id='m2'><b id='m20'>m20</b><b id='m21'>m21</b></a>";
    const char* shadowHTML =
        "<a id='s10'>s10</a>"
        "<a id='s11'><content select='#m12'></content></a>"
        "<a id='s12'>s12</a>"
        "<a id='s13'>"
            "<content select='#m10'></content>"
            "<content select='#m11'></content>"
        "</a>"
        "<a id='s14'>s14</a>";
    setupSampleHTML(mainHTML, shadowHTML, 1);
    Element* body = document().body();
    Element* m0 = body->querySelector("#m0", ASSERT_NO_EXCEPTION);
    Element* m1 = body->querySelector("#m1", ASSERT_NO_EXCEPTION);
    Element* m2 = body->querySelector("#m2", ASSERT_NO_EXCEPTION);

    Element* m00 = body->querySelector("#m00", ASSERT_NO_EXCEPTION);
    Element* m01 = body->querySelector("#m01", ASSERT_NO_EXCEPTION);
    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);
    Element* m11 = body->querySelector("#m11", ASSERT_NO_EXCEPTION);
    Element* m12 = body->querySelector("#m12", ASSERT_NO_EXCEPTION);
    Element* m20 = body->querySelector("#m20", ASSERT_NO_EXCEPTION);
    Element* m21 = body->querySelector("#m21", ASSERT_NO_EXCEPTION);

    ShadowRoot* shadowRoot = m1->openShadowRoot();
    Element* s10 = shadowRoot->querySelector("#s10", ASSERT_NO_EXCEPTION);
    Element* s11 = shadowRoot->querySelector("#s11", ASSERT_NO_EXCEPTION);
    Element* s12 = shadowRoot->querySelector("#s12", ASSERT_NO_EXCEPTION);
    Element* s13 = shadowRoot->querySelector("#s13", ASSERT_NO_EXCEPTION);
    Element* s14 = shadowRoot->querySelector("#s14", ASSERT_NO_EXCEPTION);

    testCommonAncestor(body, *m0, *m1);
    testCommonAncestor(body, *m1, *m2);
    testCommonAncestor(body, *m1, *m20);
    testCommonAncestor(body, *s14, *m21);

    testCommonAncestor(m0, *m0, *m0);
    testCommonAncestor(m0, *m00, *m01);

    testCommonAncestor(m1, *m1, *m1);
    testCommonAncestor(m1, *s10, *s14);
    testCommonAncestor(m1, *s10, *m12);
    testCommonAncestor(m1, *s12, *m12);
    testCommonAncestor(m1, *m10, *m12);

    testCommonAncestor(m01, *m01, *m01);
    testCommonAncestor(s11, *s11, *m12);
    testCommonAncestor(s13, *m10, *m11);

    s12->remove(ASSERT_NO_EXCEPTION);
    testCommonAncestor(s12, *s12, *s12);
    testCommonAncestor(nullptr, *s12, *s11);
    testCommonAncestor(nullptr, *s12, *m01);
    testCommonAncestor(nullptr, *s12, *m20);

    m20->remove(ASSERT_NO_EXCEPTION);
    testCommonAncestor(m20, *m20, *m20);
    testCommonAncestor(nullptr, *m20, *s12);
    testCommonAncestor(nullptr, *m20, *m1);
}

// Test case for
//  - nextSkippingChildren
//  - previousSkippingChildren
TEST_F(FlatTreeTraversalTest, nextSkippingChildren)
{
    const char* mainHTML =
        "<div id='m0'>m0</div>"
        "<div id='m1'>"
            "<span id='m10'>m10</span>"
            "<span id='m11'>m11</span>"
        "</div>"
        "<div id='m2'>m2</div>";
    const char* shadowHTML =
        "<content select='#m11'></content>"
        "<a id='s11'>s11</a>"
        "<a id='s12'>"
            "<b id='s120'>s120</b>"
            "<content select='#m10'></content>"
        "</a>";
    setupSampleHTML(mainHTML, shadowHTML, 1);

    Element* body = document().body();
    Element* m0 = body->querySelector("#m0", ASSERT_NO_EXCEPTION);
    Element* m1 = body->querySelector("#m1", ASSERT_NO_EXCEPTION);
    Element* m2 = body->querySelector("#m2", ASSERT_NO_EXCEPTION);

    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);
    Element* m11 = body->querySelector("#m11", ASSERT_NO_EXCEPTION);

    ShadowRoot* shadowRoot = m1->openShadowRoot();
    Element* s11 = shadowRoot->querySelector("#s11", ASSERT_NO_EXCEPTION);
    Element* s12 = shadowRoot->querySelector("#s12", ASSERT_NO_EXCEPTION);
    Element* s120 = shadowRoot->querySelector("#s120", ASSERT_NO_EXCEPTION);

    // Main tree node to main tree node
    EXPECT_EQ(*m1, FlatTreeTraversal::nextSkippingChildren(*m0));
    EXPECT_EQ(*m0, FlatTreeTraversal::previousSkippingChildren(*m1));

    // Distribute node to main tree node
    EXPECT_EQ(*m2, FlatTreeTraversal::nextSkippingChildren(*m10));
    EXPECT_EQ(*m1, FlatTreeTraversal::previousSkippingChildren(*m2));

    // Distribute node to node in shadow tree
    EXPECT_EQ(*s11, FlatTreeTraversal::nextSkippingChildren(*m11));
    EXPECT_EQ(*m11, FlatTreeTraversal::previousSkippingChildren(*s11));

    // Node in shadow tree to distributed node
    EXPECT_EQ(*s11, FlatTreeTraversal::nextSkippingChildren(*m11));
    EXPECT_EQ(*m11, FlatTreeTraversal::previousSkippingChildren(*s11));

    EXPECT_EQ(*m10, FlatTreeTraversal::nextSkippingChildren(*s120));
    EXPECT_EQ(*s120, FlatTreeTraversal::previousSkippingChildren(*m10));

    // Node in shadow tree to main tree
    EXPECT_EQ(*m2, FlatTreeTraversal::nextSkippingChildren(*s12));
    EXPECT_EQ(*m1, FlatTreeTraversal::previousSkippingChildren(*m2));
}

// Test case for
//  - lastWithin
//  - lastWithinOrSelf
TEST_F(FlatTreeTraversalTest, lastWithin)
{
    const char* mainHTML =
        "<div id='m0'>m0</div>"
        "<div id='m1'>"
            "<span id='m10'>m10</span>"
            "<span id='m11'>m11</span>"
            "<span id='m12'>m12</span>" // #m12 is not distributed.
        "</div>"
        "<div id='m2'></div>";
    const char* shadowHTML =
        "<content select='#m11'></content>"
        "<a id='s11'>s11</a>"
        "<a id='s12'>"
            "<content select='#m10'></content>"
        "</a>";
    setupSampleHTML(mainHTML, shadowHTML, 1);

    Element* body = document().body();
    Element* m0 = body->querySelector("#m0", ASSERT_NO_EXCEPTION);
    Element* m1 = body->querySelector("#m1", ASSERT_NO_EXCEPTION);
    Element* m2 = body->querySelector("#m2", ASSERT_NO_EXCEPTION);

    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);

    ShadowRoot* shadowRoot = m1->openShadowRoot();
    Element* s11 = shadowRoot->querySelector("#s11", ASSERT_NO_EXCEPTION);
    Element* s12 = shadowRoot->querySelector("#s12", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(m0->firstChild(), FlatTreeTraversal::lastWithin(*m0));
    EXPECT_EQ(*m0->firstChild(), FlatTreeTraversal::lastWithinOrSelf(*m0));

    EXPECT_EQ(m10->firstChild(), FlatTreeTraversal::lastWithin(*m1));
    EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::lastWithinOrSelf(*m1));

    EXPECT_EQ(nullptr, FlatTreeTraversal::lastWithin(*m2));
    EXPECT_EQ(*m2, FlatTreeTraversal::lastWithinOrSelf(*m2));

    EXPECT_EQ(s11->firstChild(), FlatTreeTraversal::lastWithin(*s11));
    EXPECT_EQ(*s11->firstChild(), FlatTreeTraversal::lastWithinOrSelf(*s11));

    EXPECT_EQ(m10->firstChild(), FlatTreeTraversal::lastWithin(*s12));
    EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::lastWithinOrSelf(*s12));
}

TEST_F(FlatTreeTraversalTest, previousPostOrder)
{
    const char* mainHTML =
        "<div id='m0'>m0</div>"
        "<div id='m1'>"
            "<span id='m10'>m10</span>"
            "<span id='m11'>m11</span>"
        "</div>"
        "<div id='m2'>m2</div>";
    const char* shadowHTML =
        "<content select='#m11'></content>"
        "<a id='s11'>s11</a>"
        "<a id='s12'>"
            "<b id='s120'>s120</b>"
            "<content select='#m10'></content>"
        "</a>";
    setupSampleHTML(mainHTML, shadowHTML, 1);

    Element* body = document().body();
    Element* m0 = body->querySelector("#m0", ASSERT_NO_EXCEPTION);
    Element* m1 = body->querySelector("#m1", ASSERT_NO_EXCEPTION);
    Element* m2 = body->querySelector("#m2", ASSERT_NO_EXCEPTION);

    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);
    Element* m11 = body->querySelector("#m11", ASSERT_NO_EXCEPTION);

    ShadowRoot* shadowRoot = m1->openShadowRoot();
    Element* s11 = shadowRoot->querySelector("#s11", ASSERT_NO_EXCEPTION);
    Element* s12 = shadowRoot->querySelector("#s12", ASSERT_NO_EXCEPTION);
    Element* s120 = shadowRoot->querySelector("#s120", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(*m0->firstChild(), FlatTreeTraversal::previousPostOrder(*m0));
    EXPECT_EQ(*s12, FlatTreeTraversal::previousPostOrder(*m1));
    EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::previousPostOrder(*m10));
    EXPECT_EQ(*s120, FlatTreeTraversal::previousPostOrder(*m10->firstChild()));
    EXPECT_EQ(*s120, FlatTreeTraversal::previousPostOrder(*m10->firstChild(), s12));
    EXPECT_EQ(*m11->firstChild(), FlatTreeTraversal::previousPostOrder(*m11));
    EXPECT_EQ(*m0, FlatTreeTraversal::previousPostOrder(*m11->firstChild()));
    EXPECT_EQ(nullptr, FlatTreeTraversal::previousPostOrder(*m11->firstChild(), m11));
    EXPECT_EQ(*m2->firstChild(), FlatTreeTraversal::previousPostOrder(*m2));

    EXPECT_EQ(*s11->firstChild(), FlatTreeTraversal::previousPostOrder(*s11));
    EXPECT_EQ(*m10, FlatTreeTraversal::previousPostOrder(*s12));
    EXPECT_EQ(*s120->firstChild(), FlatTreeTraversal::previousPostOrder(*s120));
    EXPECT_EQ(*s11, FlatTreeTraversal::previousPostOrder(*s120->firstChild()));
    EXPECT_EQ(nullptr, FlatTreeTraversal::previousPostOrder(*s120->firstChild(), s12));
}

TEST_F(FlatTreeTraversalTest, nextSiblingNotInDocumentFlatTree)
{
    const char* mainHTML =
        "<div id='m0'>m0</div>"
        "<div id='m1'>"
            "<span id='m10'>m10</span>"
            "<span id='m11'>m11</span>"
        "</div>"
        "<div id='m2'>m2</div>";
    const char* shadowHTML =
        "<content select='#m11'></content>";
    setupSampleHTML(mainHTML, shadowHTML, 1);

    Element* body = document().body();
    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(nullptr, FlatTreeTraversal::nextSibling(*m10));
    EXPECT_EQ(nullptr, FlatTreeTraversal::previousSibling(*m10));
}

TEST_F(FlatTreeTraversalTest, redistribution)
{
    const char* mainHTML =
        "<div id='m0'>m0</div>"
        "<div id='m1'>"
        "<span id='m10'>m10</span>"
        "<span id='m11'>m11</span>"
        "</div>"
        "<div id='m2'>m2</div>";
    const char* shadowHTML1 =
        "<div id='s1'>"
        "<content></content>"
        "</div>";

    setupSampleHTML(mainHTML, shadowHTML1, 1);

    const char* shadowHTML2 =
        "<div id='s2'>"
        "<content select='#m10'></content>"
        "<span id='s21'>s21</span>"
        "</div>";

    Element* body = document().body();
    Element* m1 = body->querySelector("#m1", ASSERT_NO_EXCEPTION);
    Element* m10 = body->querySelector("#m10", ASSERT_NO_EXCEPTION);

    ShadowRoot* shadowRoot1 = m1->openShadowRoot();
    Element* s1 = shadowRoot1->querySelector("#s1", ASSERT_NO_EXCEPTION);

    attachV0ShadowRoot(*s1, shadowHTML2);

    ShadowRoot* shadowRoot2 = s1->openShadowRoot();
    Element* s21 = shadowRoot2->querySelector("#s21", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(s21, FlatTreeTraversal::nextSibling(*m10));
    EXPECT_EQ(m10, FlatTreeTraversal::previousSibling(*s21));

    // FlatTreeTraversal::traverseSiblings does not work for a node which is not in a document flat tree.
    // e.g. The following test fails. The result of FlatTreeTraversal::previousSibling(*m11)) will be #m10, instead of nullptr.
    // Element* m11 = body->querySelector("#m11", ASSERT_NO_EXCEPTION);
    // EXPECT_EQ(nullptr, FlatTreeTraversal::previousSibling(*m11));
}

TEST_F(FlatTreeTraversalTest, v1Simple)
{
    const char* mainHTML =
        "<div id='host'>"
        "<div id='child1' slot='slot1'></div>"
        "<div id='child2' slot='slot2'></div>"
        "</div>";
    const char* shadowHTML =
        "<div id='shadow-child1'></div>"
        "<slot name='slot1'></slot>"
        "<slot name='slot2'></slot>"
        "<div id='shadow-child2'></div>";

    setupDocumentTree(mainHTML);
    Element* body = document().body();
    Element* host = body->querySelector("#host", ASSERT_NO_EXCEPTION);
    Element* child1 = body->querySelector("#child1", ASSERT_NO_EXCEPTION);
    Element* child2 = body->querySelector("#child2", ASSERT_NO_EXCEPTION);

    attachOpenShadowRoot(*host, shadowHTML);
    ShadowRoot* shadowRoot = host->openShadowRoot();
    Element* slot1 = shadowRoot->querySelector("[name=slot1]", ASSERT_NO_EXCEPTION);
    Element* slot2 = shadowRoot->querySelector("[name=slot2]", ASSERT_NO_EXCEPTION);
    Element* shadowChild1 = shadowRoot->querySelector("#shadow-child1", ASSERT_NO_EXCEPTION);
    Element* shadowChild2 = shadowRoot->querySelector("#shadow-child2", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(slot1);
    EXPECT_TRUE(slot2);
    EXPECT_EQ(shadowChild1, FlatTreeTraversal::firstChild(*host));
    EXPECT_EQ(child1, FlatTreeTraversal::nextSibling(*shadowChild1));
    EXPECT_EQ(child2, FlatTreeTraversal::nextSibling(*child1));
    EXPECT_EQ(shadowChild2, FlatTreeTraversal::nextSibling(*child2));
}

TEST_F(FlatTreeTraversalTest, v1Redistribution)
{
    const char* mainHTML =
        "<div id='d1'>"
        "<div id='d2' slot='d1-s1'></div>"
        "<div id='d3' slot='d1-s2'></div>"
        "<div id='d4' slot='nonexistent'></div>"
        "<div id='d5'></div>"
        "</div>"
        "<div id='d6'></div>";
    const char* shadowHTML1 =
        "<div id='d1-1'>"
        "<div id='d1-2'></div>"
        "<slot id='d1-s0'></slot>"
        "<slot name='d1-s1' slot='d1-1-s1'></slot>"
        "<slot name='d1-s2'></slot>"
        "<div id='d1-3'></div>"
        "<div id='d1-4' slot='d1-1-s1'></div>"
        "</div>";
    const char* shadowHTML2 =
        "<div id='d1-1-1'></div>"
        "<slot name='d1-1-s1'></slot>"
        "<slot name='d1-1-s2'></slot>"
        "<div id='d1-1-2'></div>";

    setupDocumentTree(mainHTML);

    Element* body = document().body();
    Element* d1 = body->querySelector("#d1", ASSERT_NO_EXCEPTION);
    Element* d2 = body->querySelector("#d2", ASSERT_NO_EXCEPTION);
    Element* d3 = body->querySelector("#d3", ASSERT_NO_EXCEPTION);
    Element* d4 = body->querySelector("#d4", ASSERT_NO_EXCEPTION);
    Element* d5 = body->querySelector("#d5", ASSERT_NO_EXCEPTION);
    Element* d6 = body->querySelector("#d6", ASSERT_NO_EXCEPTION);

    attachOpenShadowRoot(*d1, shadowHTML1);
    ShadowRoot* shadowRoot1 = d1->openShadowRoot();
    Element* d11 = shadowRoot1->querySelector("#d1-1", ASSERT_NO_EXCEPTION);
    Element* d12 = shadowRoot1->querySelector("#d1-2", ASSERT_NO_EXCEPTION);
    Element* d13 = shadowRoot1->querySelector("#d1-3", ASSERT_NO_EXCEPTION);
    Element* d14 = shadowRoot1->querySelector("#d1-4", ASSERT_NO_EXCEPTION);
    Element* d1s0 = shadowRoot1->querySelector("#d1-s0", ASSERT_NO_EXCEPTION);
    Element* d1s1 = shadowRoot1->querySelector("[name=d1-s1]", ASSERT_NO_EXCEPTION);
    Element* d1s2 = shadowRoot1->querySelector("[name=d1-s2]", ASSERT_NO_EXCEPTION);

    attachOpenShadowRoot(*d11, shadowHTML2);
    ShadowRoot* shadowRoot2 = d11->openShadowRoot();
    Element* d111 = shadowRoot2->querySelector("#d1-1-1", ASSERT_NO_EXCEPTION);
    Element* d112 = shadowRoot2->querySelector("#d1-1-2", ASSERT_NO_EXCEPTION);
    Element* d11s1 = shadowRoot2->querySelector("[name=d1-1-s1]", ASSERT_NO_EXCEPTION);
    Element* d11s2 = shadowRoot2->querySelector("[name=d1-1-s2]", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(d5);
    EXPECT_TRUE(d12);
    EXPECT_TRUE(d13);
    EXPECT_TRUE(d1s0);
    EXPECT_TRUE(d1s1);
    EXPECT_TRUE(d1s2);
    EXPECT_TRUE(d11s1);
    EXPECT_TRUE(d11s2);
    EXPECT_EQ(d11, FlatTreeTraversal::next(*d1));
    EXPECT_EQ(d111, FlatTreeTraversal::next(*d11));
    EXPECT_EQ(d2, FlatTreeTraversal::next(*d111));
    EXPECT_EQ(d14, FlatTreeTraversal::next(*d2));
    EXPECT_EQ(d112, FlatTreeTraversal::next(*d14));
    EXPECT_EQ(d6, FlatTreeTraversal::next(*d112));

    EXPECT_EQ(d112, FlatTreeTraversal::previous(*d6));

    EXPECT_EQ(d11, FlatTreeTraversal::parent(*d111));
    EXPECT_EQ(d11, FlatTreeTraversal::parent(*d112));
    EXPECT_EQ(d11, FlatTreeTraversal::parent(*d2));
    EXPECT_EQ(d11, FlatTreeTraversal::parent(*d14));
    EXPECT_EQ(nullptr, FlatTreeTraversal::parent(*d3));
    EXPECT_EQ(nullptr, FlatTreeTraversal::parent(*d4));
}

} // namespace blink
