// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

TEST(RangeTest, EmptyInit) {
  gfx::Range r;
  EXPECT_EQ(0U, r.start());
  EXPECT_EQ(0U, r.end());
  EXPECT_EQ(0U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(0U, r.GetMin());
  EXPECT_EQ(0U, r.GetMax());
}

TEST(RangeTest, StartEndInit) {
  gfx::Range r(10, 15);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(15U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_FALSE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(10U, r.GetMin());
  EXPECT_EQ(15U, r.GetMax());
}

TEST(RangeTest, StartEndReversedInit) {
  gfx::Range r(10, 5);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_FALSE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(5U, r.GetMin());
  EXPECT_EQ(10U, r.GetMax());
}

TEST(RangeTest, PositionInit) {
  gfx::Range r(12);
  EXPECT_EQ(12U, r.start());
  EXPECT_EQ(12U, r.end());
  EXPECT_EQ(0U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(12U, r.GetMin());
  EXPECT_EQ(12U, r.GetMax());
}

TEST(RangeTest, InvalidRange) {
  gfx::Range r(gfx::Range::InvalidRange());
  EXPECT_EQ(0U, r.length());
  EXPECT_EQ(r.start(), r.end());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_FALSE(r.IsValid());
}

TEST(RangeTest, Equality) {
  gfx::Range r1(10, 4);
  gfx::Range r2(10, 4);
  gfx::Range r3(10, 2);
  EXPECT_EQ(r1, r2);
  EXPECT_NE(r1, r3);
  EXPECT_NE(r2, r3);

  gfx::Range r4(11, 4);
  EXPECT_NE(r1, r4);
  EXPECT_NE(r2, r4);
  EXPECT_NE(r3, r4);

  gfx::Range r5(12, 5);
  EXPECT_NE(r1, r5);
  EXPECT_NE(r2, r5);
  EXPECT_NE(r3, r5);
}

TEST(RangeTest, EqualsIgnoringDirection) {
  gfx::Range r1(10, 5);
  gfx::Range r2(5, 10);
  EXPECT_TRUE(r1.EqualsIgnoringDirection(r2));
}

TEST(RangeTest, SetStart) {
  gfx::Range r(10, 20);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(10U, r.length());

  r.set_start(42);
  EXPECT_EQ(42U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(22U, r.length());
  EXPECT_TRUE(r.is_reversed());
}

TEST(RangeTest, SetEnd) {
  gfx::Range r(10, 13);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(3U, r.length());

  r.set_end(20);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(10U, r.length());
}

TEST(RangeTest, SetStartAndEnd) {
  gfx::Range r;
  r.set_end(5);
  r.set_start(1);
  EXPECT_EQ(1U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(4U, r.length());
  EXPECT_EQ(1U, r.GetMin());
  EXPECT_EQ(5U, r.GetMax());
}

TEST(RangeTest, ReversedRange) {
  gfx::Range r(10, 5);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(5U, r.GetMin());
  EXPECT_EQ(10U, r.GetMax());
}

TEST(RangeTest, SetReversedRange) {
  gfx::Range r(10, 20);
  r.set_start(25);
  EXPECT_EQ(25U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());

  r.set_end(21);
  EXPECT_EQ(25U, r.start());
  EXPECT_EQ(21U, r.end());
  EXPECT_EQ(4U, r.length());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(21U, r.GetMin());
  EXPECT_EQ(25U, r.GetMax());
}

void TestContainsAndIntersects(const gfx::Range& r1,
                               const gfx::Range& r2,
                               const gfx::Range& r3) {
  EXPECT_TRUE(r1.Intersects(r1));
  EXPECT_TRUE(r1.Contains(r1));
  EXPECT_EQ(gfx::Range(10, 12), r1.Intersect(r1));

  EXPECT_FALSE(r1.Intersects(r2));
  EXPECT_FALSE(r1.Contains(r2));
  EXPECT_TRUE(r1.Intersect(r2).is_empty());
  EXPECT_FALSE(r2.Intersects(r1));
  EXPECT_FALSE(r2.Contains(r1));
  EXPECT_TRUE(r2.Intersect(r1).is_empty());

  EXPECT_TRUE(r1.Intersects(r3));
  EXPECT_TRUE(r3.Intersects(r1));
  EXPECT_TRUE(r3.Contains(r1));
  EXPECT_FALSE(r1.Contains(r3));
  EXPECT_EQ(gfx::Range(10, 12), r1.Intersect(r3));
  EXPECT_EQ(gfx::Range(10, 12), r3.Intersect(r1));

  EXPECT_TRUE(r2.Intersects(r3));
  EXPECT_TRUE(r3.Intersects(r2));
  EXPECT_FALSE(r3.Contains(r2));
  EXPECT_FALSE(r2.Contains(r3));
  EXPECT_EQ(gfx::Range(5, 8), r2.Intersect(r3));
  EXPECT_EQ(gfx::Range(5, 8), r3.Intersect(r2));
}

TEST(RangeTest, ContainAndIntersect) {
  {
    SCOPED_TRACE("contain and intersect");
    gfx::Range r1(10, 12);
    gfx::Range r2(1, 8);
    gfx::Range r3(5, 12);
    TestContainsAndIntersects(r1, r2, r3);
  }
  {
    SCOPED_TRACE("contain and intersect: reversed");
    gfx::Range r1(12, 10);
    gfx::Range r2(8, 1);
    gfx::Range r3(12, 5);
    TestContainsAndIntersects(r1, r2, r3);
  }
  // Invalid rect tests
  gfx::Range r1(10, 12);
  gfx::Range r2(8, 1);
  gfx::Range invalid = r1.Intersect(r2);
  EXPECT_FALSE(invalid.IsValid());
  EXPECT_FALSE(invalid.Contains(invalid));
  EXPECT_FALSE(invalid.Contains(r1));
  EXPECT_FALSE(invalid.Intersects(invalid));
  EXPECT_FALSE(invalid.Intersects(r1));
  EXPECT_FALSE(r1.Contains(invalid));
  EXPECT_FALSE(r1.Intersects(invalid));
}
