// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScopedOrientationChangeIndicator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ScopedOrientationChangeIndicatorTest, InitialState) {
  EXPECT_FALSE(ScopedOrientationChangeIndicator::processingOrientationChange());
}

TEST(ScopedOrientationChangeIndicatorTest, ConstructOneIndicatorWithGesture) {
  ScopedOrientationChangeIndicator indicator;

  EXPECT_TRUE(ScopedOrientationChangeIndicator::processingOrientationChange());
}

TEST(ScopedOrientationChangeIndicatorTest, MultipleIndicatorInTheSameScope) {
  ScopedOrientationChangeIndicator indicator1;

  EXPECT_TRUE(ScopedOrientationChangeIndicator::processingOrientationChange());

  ScopedOrientationChangeIndicator indicator2;

  EXPECT_TRUE(ScopedOrientationChangeIndicator::processingOrientationChange());
}

TEST(ScopedOrientationChangeIndicatorTest, DestructResetsStateUsingGesture) {
  { ScopedOrientationChangeIndicator indicator; }

  EXPECT_FALSE(ScopedOrientationChangeIndicator::processingOrientationChange());
}

TEST(ScopedOrientationChangeIndicatorTest, DestructResetsStateUsingNoGesture) {
  ScopedOrientationChangeIndicator indicator;
  { ScopedOrientationChangeIndicator indicator; }

  EXPECT_TRUE(ScopedOrientationChangeIndicator::processingOrientationChange());
}

}  // namespace blink
