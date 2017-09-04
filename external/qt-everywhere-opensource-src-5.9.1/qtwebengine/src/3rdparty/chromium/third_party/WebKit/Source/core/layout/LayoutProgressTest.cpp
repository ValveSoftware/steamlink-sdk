// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutProgress.h"

#include "core/HTMLNames.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutProgressTest : public RenderingTest {
 public:
  static bool isAnimationTimerActive(const LayoutProgress* o) {
    return o->isAnimationTimerActive();
  }
  static bool isAnimatiing(const LayoutProgress* o) { return o->isAnimating(); }
};

TEST_F(LayoutProgressTest, AnimationScheduling) {
  RenderingTest::setBodyInnerHTML(
      "<progress id=\"progressElement\" value=0.3 max=1.0></progress>");
  document().view()->updateAllLifecyclePhases();
  Element* progressElement =
      document().getElementById(AtomicString("progressElement"));
  LayoutProgress* layoutProgress =
      toLayoutProgress(progressElement->layoutObject());

  // Verify that we do not schedule a timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::isAnimationTimerActive(layoutProgress));
  EXPECT_FALSE(LayoutProgressTest::isAnimatiing(layoutProgress));

  progressElement->removeAttribute("value");
  document().view()->updateAllLifecyclePhases();

  // Verify that we schedule a timer for an indeterminant progress element
  EXPECT_TRUE(LayoutProgressTest::isAnimationTimerActive(layoutProgress));
  EXPECT_TRUE(LayoutProgressTest::isAnimatiing(layoutProgress));

  progressElement->setAttribute(HTMLNames::valueAttr, "0.7");
  document().view()->updateAllLifecyclePhases();

  // Verify that we cancel the timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::isAnimationTimerActive(layoutProgress));
  EXPECT_FALSE(LayoutProgressTest::isAnimatiing(layoutProgress));
}

}  // namespace blink
