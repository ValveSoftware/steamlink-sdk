// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatableDoubleAndBool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AnimationAnimatableDoubleAndBoolTest, Create)
{
    EXPECT_TRUE(static_cast<bool>(AnimatableDoubleAndBool::create(30, false).get()));
    EXPECT_TRUE(static_cast<bool>(AnimatableDoubleAndBool::create(270, true).get()));
}

TEST(AnimationAnimatableDoubleAndBoolTest, Equal)
{
    EXPECT_TRUE(AnimatableDoubleAndBool::create(30, false)->equals(AnimatableDoubleAndBool::create(30, false).get()));
    EXPECT_TRUE(AnimatableDoubleAndBool::create(270, true)->equals(AnimatableDoubleAndBool::create(270, true).get()));
    EXPECT_FALSE(AnimatableDoubleAndBool::create(30, false)->equals(AnimatableDoubleAndBool::create(270, true).get()));
    EXPECT_FALSE(AnimatableDoubleAndBool::create(30, false)->equals(AnimatableDoubleAndBool::create(270, false).get()));
    EXPECT_FALSE(AnimatableDoubleAndBool::create(30, false)->equals(AnimatableDoubleAndBool::create(30, true).get()));
}

TEST(AnimationAnimatableDoubleAndBoolTest, ToDouble)
{
    EXPECT_EQ(5.9, AnimatableDoubleAndBool::create(5.9, false)->toDouble());
    EXPECT_EQ(-10, AnimatableDoubleAndBool::create(-10, true)->toDouble());
}

TEST(AnimationAnimatableDoubleAndBoolTest, Flag)
{
    EXPECT_FALSE(AnimatableDoubleAndBool::create(5.9, false)->flag());
    EXPECT_TRUE(AnimatableDoubleAndBool::create(-10, true)->flag());
}

TEST(AnimationAnimatableDoubleAndBoolTest, InterpolateFalse)
{
    RefPtr<AnimatableDoubleAndBool> from10 = AnimatableDoubleAndBool::create(10, false);
    RefPtr<AnimatableDoubleAndBool> to20 = AnimatableDoubleAndBool::create(20, false);
    EXPECT_FALSE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->flag());
    EXPECT_EQ(5, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->toDouble());
    EXPECT_EQ(10, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0).get())->toDouble());
    EXPECT_EQ(14, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.4).get())->toDouble());
    EXPECT_EQ(15, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.5).get())->toDouble());
    EXPECT_EQ(16, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.6).get())->toDouble());
    EXPECT_EQ(20, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1).get())->toDouble());
    EXPECT_EQ(25, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->toDouble());
    EXPECT_FALSE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->flag());
}

TEST(AnimationAnimatableDoubleAndBoolTest, InterpolateTrue)
{
    RefPtr<AnimatableDoubleAndBool> from10 = AnimatableDoubleAndBool::create(10, true);
    RefPtr<AnimatableDoubleAndBool> to20 = AnimatableDoubleAndBool::create(20, true);
    EXPECT_TRUE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->flag());
    EXPECT_EQ(5, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->toDouble());
    EXPECT_EQ(10, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0).get())->toDouble());
    EXPECT_EQ(14, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.4).get())->toDouble());
    EXPECT_EQ(15, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.5).get())->toDouble());
    EXPECT_EQ(16, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.6).get())->toDouble());
    EXPECT_EQ(20, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1).get())->toDouble());
    EXPECT_EQ(25, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->toDouble());
    EXPECT_TRUE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->flag());
}

TEST(AnimationAnimatableDoubleAndBoolTest, Step)
{
    RefPtr<AnimatableDoubleAndBool> from10 = AnimatableDoubleAndBool::create(10, true);
    RefPtr<AnimatableDoubleAndBool> to20 = AnimatableDoubleAndBool::create(20, false);
    EXPECT_TRUE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->flag());
    EXPECT_EQ(10, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), -0.5).get())->toDouble());
    EXPECT_EQ(10, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0).get())->toDouble());
    EXPECT_EQ(10, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.4).get())->toDouble());
    EXPECT_TRUE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.4).get())->flag());

    EXPECT_FALSE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.6).get())->flag());
    EXPECT_EQ(20, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 0.6).get())->toDouble());
    EXPECT_EQ(20, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1).get())->toDouble());
    EXPECT_EQ(20, toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->toDouble());
    EXPECT_FALSE(toAnimatableDoubleAndBool(AnimatableValue::interpolate(from10.get(), to20.get(), 1.5).get())->flag());
}

} // namespace blink
