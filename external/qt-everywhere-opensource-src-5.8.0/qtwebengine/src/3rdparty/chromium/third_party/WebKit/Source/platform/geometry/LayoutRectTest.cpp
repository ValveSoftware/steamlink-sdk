// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutRect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

#ifndef NDEBUG
TEST(LayoutRectTest, ToString)
{
    LayoutRect emptyRect = LayoutRect();
    EXPECT_EQ(String("0.000000,0.000000 0.000000x0.000000"), emptyRect.toString());

    LayoutRect rect(1, 2, 3, 4);
    EXPECT_EQ(String("1.000000,2.000000 3.000000x4.000000"), rect.toString());

    LayoutRect granularRect(LayoutUnit(1.6f), LayoutUnit(2.7f), LayoutUnit(3.8f), LayoutUnit(4.9f));
    EXPECT_EQ(String("1.593750,2.687500 3.796875x4.890625"), granularRect.toString());
}

TEST(LayoutRectTest, InclusiveIntersect)
{
    LayoutRect rect(11, 12, 0, 0);
    EXPECT_TRUE(rect.inclusiveIntersect(LayoutRect(11, 12, 13, 14)));
    EXPECT_EQ(rect, LayoutRect(11, 12, 0, 0));

    rect = LayoutRect(11, 12, 13, 14);
    EXPECT_TRUE(rect.inclusiveIntersect(LayoutRect(24, 8, 0, 7)));
    EXPECT_EQ(rect, LayoutRect(24, 12, 0, 3));

    rect = LayoutRect(11, 12, 13, 14);
    EXPECT_TRUE(rect.inclusiveIntersect(LayoutRect(9, 15, 4, 0)));
    EXPECT_EQ(rect, LayoutRect(11, 15, 2, 0));

    rect = LayoutRect(11, 12, 0, 14);
    EXPECT_FALSE(rect.inclusiveIntersect(LayoutRect(12, 13, 15, 16)));
    EXPECT_EQ(rect, LayoutRect());
}
#endif

} // namespace blink
