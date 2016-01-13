// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/renderer/compositor_bindings/web_animation_impl.h"
#include "content/renderer/compositor_bindings/web_float_animation_curve_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebAnimation;
using blink::WebAnimationCurve;
using blink::WebFloatAnimationCurve;

namespace content {
namespace {

TEST(WebAnimationTest, DefaultSettings) {
  scoped_ptr<WebAnimationCurve> curve(new WebFloatAnimationCurveImpl());
  scoped_ptr<WebAnimation> animation(
      new WebAnimationImpl(*curve, WebAnimation::TargetPropertyOpacity, 1, 0));

  // Ensure that the defaults are correct.
  EXPECT_EQ(1, animation->iterations());
  EXPECT_EQ(0, animation->startTime());
  EXPECT_EQ(0, animation->timeOffset());
#if WEB_ANIMATION_SUPPORTS_FULL_DIRECTION
  EXPECT_EQ(WebAnimation::DirectionNormal, animation->direction());
#else
  EXPECT_FALSE(animation->alternatesDirection());
#endif
}

TEST(WebAnimationTest, ModifiedSettings) {
  scoped_ptr<WebFloatAnimationCurve> curve(new WebFloatAnimationCurveImpl());
  scoped_ptr<WebAnimation> animation(
      new WebAnimationImpl(*curve, WebAnimation::TargetPropertyOpacity, 1, 0));
  animation->setIterations(2);
  animation->setStartTime(2);
  animation->setTimeOffset(2);
#if WEB_ANIMATION_SUPPORTS_FULL_DIRECTION
  animation->setDirection(WebAnimation::DirectionReverse);
#else
  animation->setAlternatesDirection(true);
#endif

  EXPECT_EQ(2, animation->iterations());
  EXPECT_EQ(2, animation->startTime());
  EXPECT_EQ(2, animation->timeOffset());
#if WEB_ANIMATION_SUPPORTS_FULL_DIRECTION
  EXPECT_EQ(WebAnimation::DirectionReverse, animation->direction());
#else
  EXPECT_TRUE(animation->alternatesDirection());
  animation->setAlternatesDirection(false);
  EXPECT_FALSE(animation->alternatesDirection());
#endif
}

}  // namespace
}  // namespace content

