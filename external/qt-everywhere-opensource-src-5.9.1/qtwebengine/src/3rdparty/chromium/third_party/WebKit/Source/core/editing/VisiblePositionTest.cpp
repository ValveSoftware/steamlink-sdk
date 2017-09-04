// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisiblePosition.h"

#include "core/css/CSSStyleDeclaration.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/VisibleUnits.h"

namespace blink {

class VisiblePositionTest : public EditingTestBase {};

TEST_F(VisiblePositionTest, ShadowV0DistributedNodes) {
  const char* bodyContent =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadowContent =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Element* body = document().body();
  Element* one = body->querySelector("#one");
  Element* two = body->querySelector("#two");
  Element* four = shadowRoot->querySelector("#s4");
  Element* five = shadowRoot->querySelector("#s5");

  EXPECT_EQ(Position(one->firstChild(), 0),
            canonicalPositionOf(Position(one, 0)));
  EXPECT_EQ(Position(one->firstChild(), 0),
            createVisiblePosition(Position(one, 0)).deepEquivalent());
  EXPECT_EQ(Position(one->firstChild(), 2),
            canonicalPositionOf(Position(two, 0)));
  EXPECT_EQ(Position(one->firstChild(), 2),
            createVisiblePosition(Position(two, 0)).deepEquivalent());

  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2),
            canonicalPositionOf(PositionInFlatTree(one, 0)));
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2),
            createVisiblePosition(PositionInFlatTree(one, 0)).deepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four->firstChild(), 2),
            canonicalPositionOf(PositionInFlatTree(two, 0)));
  EXPECT_EQ(PositionInFlatTree(four->firstChild(), 2),
            createVisiblePosition(PositionInFlatTree(two, 0)).deepEquivalent());
}

#if DCHECK_IS_ON()

TEST_F(VisiblePositionTest, NullIsValid) {
  EXPECT_TRUE(VisiblePosition().isValid());
}

TEST_F(VisiblePositionTest, NonNullIsValidBeforeMutation) {
  setBodyContent("<p>one</p>");

  Element* paragraph = document().querySelector("p");
  Position position(paragraph->firstChild(), 1);
  EXPECT_TRUE(createVisiblePosition(position).isValid());
}

TEST_F(VisiblePositionTest, NonNullInvalidatedAfterDOMChange) {
  setBodyContent("<p>one</p>");

  Element* paragraph = document().querySelector("p");
  Position position(paragraph->firstChild(), 1);
  VisiblePosition nullVisiblePosition;
  VisiblePosition nonNullVisiblePosition = createVisiblePosition(position);

  Element* div = document().createElement("div");
  document().body()->appendChild(div);

  EXPECT_TRUE(nullVisiblePosition.isValid());
  EXPECT_FALSE(nonNullVisiblePosition.isValid());

  updateAllLifecyclePhases();

  // Invalid VisiblePosition can never become valid again.
  EXPECT_FALSE(nonNullVisiblePosition.isValid());
}

TEST_F(VisiblePositionTest, NonNullInvalidatedAfterStyleChange) {
  setBodyContent("<div>one</div><p>two</p>");

  Element* paragraph = document().querySelector("p");
  Element* div = document().querySelector("div");
  Position position(paragraph->firstChild(), 1);

  VisiblePosition visiblePosition1 = createVisiblePosition(position);
  div->style()->setProperty("color", "red", "important", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(visiblePosition1.isValid());

  updateAllLifecyclePhases();

  VisiblePosition visiblePosition2 = createVisiblePosition(position);
  div->style()->setProperty("display", "none", "important",
                            ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(visiblePosition2.isValid());

  updateAllLifecyclePhases();

  // Invalid VisiblePosition can never become valid again.
  EXPECT_FALSE(visiblePosition1.isValid());
  EXPECT_FALSE(visiblePosition2.isValid());
}

#endif

}  // namespace blink
