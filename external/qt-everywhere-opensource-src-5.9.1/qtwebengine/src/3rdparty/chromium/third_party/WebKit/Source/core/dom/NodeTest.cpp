// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Node.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class NodeTest : public EditingTestBase {};

TEST_F(NodeTest, canStartSelection) {
  const char* bodyContent =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  setBodyContent(bodyContent);
  Node* one = document().getElementById("one");
  Node* two = document().getElementById("two");

  EXPECT_FALSE(one->canStartSelection());
  EXPECT_FALSE(one->firstChild()->canStartSelection());
  EXPECT_TRUE(two->canStartSelection());
  EXPECT_TRUE(two->firstChild()->canStartSelection());
}

TEST_F(NodeTest, canStartSelectionWithShadowDOM) {
  const char* bodyContent = "<div id=host><span id=one>one</span></div>";
  const char* shadowContent =
      "<a href='http://www.msn.com'><content></content></a>";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent, "host");
  Node* one = document().getElementById("one");

  EXPECT_FALSE(one->canStartSelection());
  EXPECT_FALSE(one->firstChild()->canStartSelection());
}

TEST_F(NodeTest, customElementState) {
  const char* bodyContent = "<div id=div></div>";
  setBodyContent(bodyContent);
  Element* div = document().getElementById("div");
  EXPECT_EQ(CustomElementState::Uncustomized, div->getCustomElementState());
  EXPECT_TRUE(div->isDefined());
  EXPECT_EQ(Node::V0NotCustomElement, div->getV0CustomElementState());

  div->setCustomElementState(CustomElementState::Undefined);
  EXPECT_EQ(CustomElementState::Undefined, div->getCustomElementState());
  EXPECT_FALSE(div->isDefined());
  EXPECT_EQ(Node::V0NotCustomElement, div->getV0CustomElementState());

  div->setCustomElementState(CustomElementState::Custom);
  EXPECT_EQ(CustomElementState::Custom, div->getCustomElementState());
  EXPECT_TRUE(div->isDefined());
  EXPECT_EQ(Node::V0NotCustomElement, div->getV0CustomElementState());
}

}  // namespace blink
