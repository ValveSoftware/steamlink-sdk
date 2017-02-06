// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TreeScope.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TreeScopeTest, CommonAncestorOfSameTrees)
{
    Document* document = Document::create();
    EXPECT_EQ(document, document->commonAncestorTreeScope(*document));

    Element* html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    ShadowRoot* shadowRoot = html->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(shadowRoot, shadowRoot->commonAncestorTreeScope(*shadowRoot));
}

TEST(TreeScopeTest, CommonAncestorOfInclusiveTrees)
{
    //  document
    //     |      : Common ancestor is document.
    // shadowRoot

    Document* document = Document::create();
    Element* html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    ShadowRoot* shadowRoot = html->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document, document->commonAncestorTreeScope(*shadowRoot));
    EXPECT_EQ(document, shadowRoot->commonAncestorTreeScope(*document));
}

TEST(TreeScopeTest, CommonAncestorOfSiblingTrees)
{
    //  document
    //   /    \  : Common ancestor is document.
    //  A      B

    Document* document = Document::create();
    Element* html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    Element* head = document->createElement("head", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(head);
    Element* body = document->createElement("body", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(body);

    ShadowRoot* shadowRootA = head->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    ShadowRoot* shadowRootB = body->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document, shadowRootA->commonAncestorTreeScope(*shadowRootB));
    EXPECT_EQ(document, shadowRootB->commonAncestorTreeScope(*shadowRootA));
}

TEST(TreeScopeTest, CommonAncestorOfTreesAtDifferentDepths)
{
    //  document
    //    / \    : Common ancestor is document.
    //   Y   B
    //  /
    // A

    Document* document = Document::create();
    Element* html = document->createElement("html", nullAtom, ASSERT_NO_EXCEPTION);
    document->appendChild(html, ASSERT_NO_EXCEPTION);
    Element* head = document->createElement("head", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(head);
    Element* body = document->createElement("body", nullAtom, ASSERT_NO_EXCEPTION);
    html->appendChild(body);

    ShadowRoot* shadowRootY = head->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    ShadowRoot* shadowRootB = body->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    Element* divInY = document->createElement("div", nullAtom, ASSERT_NO_EXCEPTION);
    shadowRootY->appendChild(divInY);
    ShadowRoot* shadowRootA = divInY->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);

    EXPECT_EQ(document, shadowRootA->commonAncestorTreeScope(*shadowRootB));
    EXPECT_EQ(document, shadowRootB->commonAncestorTreeScope(*shadowRootA));
}

TEST(TreeScopeTest, CommonAncestorOfTreesInDifferentDocuments)
{
    Document* document1 = Document::create();
    Document* document2 = Document::create();
    EXPECT_EQ(0, document1->commonAncestorTreeScope(*document2));
    EXPECT_EQ(0, document2->commonAncestorTreeScope(*document1));
}

} // namespace blink
