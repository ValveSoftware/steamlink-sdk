// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/Position.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class PositionTest : public EditingTestBase {};

TEST_F(PositionTest, NodeAsRangeLastNodeNull) {
  EXPECT_EQ(nullptr, Position().nodeAsRangeLastNode());
  EXPECT_EQ(nullptr, PositionInFlatTree().nodeAsRangeLastNode());
}

TEST_F(PositionTest, editingPositionOfWithEditingIgnoresContent) {
  const char* bodyContent =
      "<textarea id=textarea></textarea><a id=child1>1</a><b id=child2>2</b>";
  setBodyContent(bodyContent);
  Node* textarea = document().getElementById("textarea");

  EXPECT_EQ(Position::beforeNode(textarea),
            Position::editingPositionOf(textarea, 0));
  EXPECT_EQ(Position::afterNode(textarea),
            Position::editingPositionOf(textarea, 1));
  EXPECT_EQ(Position::afterNode(textarea),
            Position::editingPositionOf(textarea, 2));

  // Change DOM tree to
  // <textarea id=textarea><a id=child1>1</a><b id=child2>2</b></textarea>
  Node* child1 = document().getElementById("child1");
  Node* child2 = document().getElementById("child2");
  textarea->appendChild(child1);
  textarea->appendChild(child2);

  EXPECT_EQ(Position::beforeNode(textarea),
            Position::editingPositionOf(textarea, 0));
  EXPECT_EQ(Position::afterNode(textarea),
            Position::editingPositionOf(textarea, 1));
  EXPECT_EQ(Position::afterNode(textarea),
            Position::editingPositionOf(textarea, 2));
  EXPECT_EQ(Position::afterNode(textarea),
            Position::editingPositionOf(textarea, 3));
}

TEST_F(PositionTest, NodeAsRangeLastNode) {
  const char* bodyContent = "<p id='p1'>11</p><p id='p2'></p><p id='p3'>33</p>";
  setBodyContent(bodyContent);
  Node* p1 = document().getElementById("p1");
  Node* p2 = document().getElementById("p2");
  Node* p3 = document().getElementById("p3");
  Node* body = EditingStrategy::parent(*p1);
  Node* t1 = EditingStrategy::firstChild(*p1);
  Node* t3 = EditingStrategy::firstChild(*p3);

  EXPECT_EQ(body, Position::inParentBeforeNode(*p1).nodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::inParentBeforeNode(*p2).nodeAsRangeLastNode());
  EXPECT_EQ(p2, Position::inParentBeforeNode(*p3).nodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::inParentAfterNode(*p1).nodeAsRangeLastNode());
  EXPECT_EQ(p2, Position::inParentAfterNode(*p2).nodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::inParentAfterNode(*p3).nodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::afterNode(p3).nodeAsRangeLastNode());

  EXPECT_EQ(body,
            PositionInFlatTree::inParentBeforeNode(*p1).nodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::inParentBeforeNode(*p2).nodeAsRangeLastNode());
  EXPECT_EQ(p2,
            PositionInFlatTree::inParentBeforeNode(*p3).nodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::inParentAfterNode(*p1).nodeAsRangeLastNode());
  EXPECT_EQ(p2,
            PositionInFlatTree::inParentAfterNode(*p2).nodeAsRangeLastNode());
  EXPECT_EQ(t3,
            PositionInFlatTree::inParentAfterNode(*p3).nodeAsRangeLastNode());
  EXPECT_EQ(t3, PositionInFlatTree::afterNode(p3).nodeAsRangeLastNode());
}

TEST_F(PositionTest, NodeAsRangeLastNodeShadow) {
  const char* bodyContent =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadowContent =
      "<a id='a'><content select=#two></content><content "
      "select=#one></content></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Node* host = document().getElementById("host");
  Node* n1 = document().getElementById("one");
  Node* n2 = document().getElementById("two");
  Node* t0 = EditingStrategy::firstChild(*host);
  Node* t1 = EditingStrategy::firstChild(*n1);
  Node* t2 = EditingStrategy::firstChild(*n2);
  Node* t3 = EditingStrategy::lastChild(*host);
  Node* a = shadowRoot->getElementById("a");

  EXPECT_EQ(t0, Position::inParentBeforeNode(*n1).nodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::inParentBeforeNode(*n2).nodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::inParentAfterNode(*n1).nodeAsRangeLastNode());
  EXPECT_EQ(t2, Position::inParentAfterNode(*n2).nodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::afterNode(host).nodeAsRangeLastNode());

  EXPECT_EQ(t2,
            PositionInFlatTree::inParentBeforeNode(*n1).nodeAsRangeLastNode());
  EXPECT_EQ(a,
            PositionInFlatTree::inParentBeforeNode(*n2).nodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::inParentAfterNode(*n1).nodeAsRangeLastNode());
  EXPECT_EQ(t2,
            PositionInFlatTree::inParentAfterNode(*n2).nodeAsRangeLastNode());
  EXPECT_EQ(t1, PositionInFlatTree::afterNode(host).nodeAsRangeLastNode());
}

TEST_F(PositionTest, ToPositionInFlatTreeWithActiveInsertionPoint) {
  const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
  const char* shadowContent =
      "<a id='a'><content select=#one "
      "id='content'></content><content></content></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
  Element* anchor = shadowRoot->getElementById("a");

  EXPECT_EQ(PositionInFlatTree(anchor, 0),
            toPositionInFlatTree(Position(anchor, 0)));
  EXPECT_EQ(PositionInFlatTree(anchor, 1),
            toPositionInFlatTree(Position(anchor, 1)));
  EXPECT_EQ(PositionInFlatTree(anchor, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(anchor, 2)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithInactiveInsertionPoint) {
  const char* bodyContent = "<p id='p'><content></content></p>";
  setBodyContent(bodyContent);
  Element* anchor = document().getElementById("p");

  EXPECT_EQ(PositionInFlatTree(anchor, 0),
            toPositionInFlatTree(Position(anchor, 0)));
  EXPECT_EQ(PositionInFlatTree(anchor, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(anchor, 1)));
}

// This test comes from "editing/style/block-style-progress-crash.html".
TEST_F(PositionTest, ToPositionInFlatTreeWithNotDistributed) {
  setBodyContent("<progress id=sample>foo</progress>");
  Element* sample = document().getElementById("sample");

  EXPECT_EQ(PositionInFlatTree(sample, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(sample, 0)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithShadowRoot) {
  const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
  const char* shadowContent = "<a><content select=#one></content></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
  Element* host = document().getElementById("host");

  EXPECT_EQ(PositionInFlatTree(host, 0),
            toPositionInFlatTree(Position(shadowRoot, 0)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(shadowRoot, 1)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(
                Position(shadowRoot, PositionAnchorType::AfterChildren)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::BeforeChildren),
            toPositionInFlatTree(
                Position(shadowRoot, PositionAnchorType::BeforeChildren)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::AfterAnchor),
            toPositionInFlatTree(
                Position(shadowRoot, PositionAnchorType::AfterAnchor)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::BeforeAnchor),
            toPositionInFlatTree(
                Position(shadowRoot, PositionAnchorType::BeforeAnchor)));
}

TEST_F(PositionTest,
       ToPositionInFlatTreeWithShadowRootContainingSingleContent) {
  const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
  const char* shadowContent = "<content select=#one></content>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
  Element* host = document().getElementById("host");

  EXPECT_EQ(PositionInFlatTree(host, 0),
            toPositionInFlatTree(Position(shadowRoot, 0)));
  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(shadowRoot, 1)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithEmptyShadowRoot) {
  const char* bodyContent = "<p id='host'>00<b id='one'>11</b>22</p>";
  const char* shadowContent = "";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
  Element* host = document().getElementById("host");

  EXPECT_EQ(PositionInFlatTree(host, PositionAnchorType::AfterChildren),
            toPositionInFlatTree(Position(shadowRoot, 0)));
}

}  // namespace blink
