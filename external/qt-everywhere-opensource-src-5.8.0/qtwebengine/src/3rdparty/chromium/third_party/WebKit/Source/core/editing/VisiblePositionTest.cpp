// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisiblePosition.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/VisibleUnits.h"

namespace blink {

class VisiblePositionTest : public EditingTestBase {
};

TEST_F(VisiblePositionTest, ShadowV0DistributedNodes)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<a><span id='s4'>44</span><content select=#two></content><span id='s5'>55</span><content select=#one></content><span id='s6'>66</span></a>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Element* body = document().body();
    Element* one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    Element* two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    Element* four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    Element* five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(Position(one->firstChild(), 0), canonicalPositionOf(Position(one, 0)));
    EXPECT_EQ(Position(one->firstChild(), 0), createVisiblePosition(Position(one, 0)).deepEquivalent());
    EXPECT_EQ(Position(one->firstChild(), 2), canonicalPositionOf(Position(two, 0)));
    EXPECT_EQ(Position(one->firstChild(), 2), createVisiblePosition(Position(two, 0)).deepEquivalent());

    EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2), canonicalPositionOf(PositionInFlatTree(one, 0)));
    EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2), createVisiblePosition(PositionInFlatTree(one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four->firstChild(), 2), canonicalPositionOf(PositionInFlatTree(two, 0)));
    EXPECT_EQ(PositionInFlatTree(four->firstChild(), 2), createVisiblePosition(PositionInFlatTree(two, 0)).deepEquivalent());
}

} // namespace blink
