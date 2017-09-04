// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/OutlineValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(OutlineValueTest, VisuallyEqualStyle) {
  OutlineValue outline1;
  OutlineValue outline2;

  // Outlines visually equal if their styles are all BNONE.
  EXPECT_TRUE(outline1.visuallyEqual(outline2));
  outline2.setOffset(10);
  EXPECT_TRUE(outline1.visuallyEqual(outline2));

  outline2.setStyle(BorderStyleDotted);
  outline1.setOffset(10);
  EXPECT_FALSE(outline1.visuallyEqual(outline2));
}

TEST(OutlineValueTest, VisuallyEqualOffset) {
  OutlineValue outline1;
  OutlineValue outline2;

  outline1.setStyle(BorderStyleDotted);
  outline2.setStyle(BorderStyleDotted);
  EXPECT_TRUE(outline1.visuallyEqual(outline2));

  outline1.setOffset(10);
  EXPECT_FALSE(outline1.visuallyEqual(outline2));

  outline2.setOffset(10);
  EXPECT_TRUE(outline1.visuallyEqual(outline2));
}

TEST(OutlineValueTest, VisuallyEqualIsAuto) {
  OutlineValue outline1;
  OutlineValue outline2;

  outline1.setStyle(BorderStyleDotted);
  outline2.setStyle(BorderStyleDotted);
  EXPECT_TRUE(outline1.visuallyEqual(outline2));

  outline1.setIsAuto(OutlineIsAutoOn);
  EXPECT_FALSE(outline1.visuallyEqual(outline2));

  outline2.setIsAuto(OutlineIsAutoOn);
  EXPECT_TRUE(outline1.visuallyEqual(outline2));
}

}  // namespace blink
