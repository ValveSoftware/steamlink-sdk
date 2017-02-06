// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WebCompositorAnimationTest, DefaultSettings)
{
    std::unique_ptr<CompositorAnimationCurve> curve = CompositorFloatAnimationCurve::create();
    std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(
        *curve, CompositorTargetProperty::OPACITY, 1, 0);

    // Ensure that the defaults are correct.
    EXPECT_EQ(1, animation->iterations());
    EXPECT_EQ(0, animation->startTime());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::NORMAL, animation->getDirection());
}

TEST(WebCompositorAnimationTest, ModifiedSettings)
{
    std::unique_ptr<CompositorFloatAnimationCurve> curve = CompositorFloatAnimationCurve::create();
    std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(
        *curve, CompositorTargetProperty::OPACITY, 1, 0);
    animation->setIterations(2);
    animation->setStartTime(2);
    animation->setTimeOffset(2);
    animation->setDirection(CompositorAnimation::Direction::REVERSE);

    EXPECT_EQ(2, animation->iterations());
    EXPECT_EQ(2, animation->startTime());
    EXPECT_EQ(2, animation->timeOffset());
    EXPECT_EQ(CompositorAnimation::Direction::REVERSE, animation->getDirection());
}

} // namespace blink
