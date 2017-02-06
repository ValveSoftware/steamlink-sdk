// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/transforms/Rotation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(RotationTest, GetCommonAxisTest)
{
    FloatPoint3D axis;
    double angleA;
    double angleB;

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(0, 0, 0), 0),
        Rotation(FloatPoint3D(1, 2, 3), 100),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(0, angleA);
    EXPECT_EQ(100, angleB);

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(1, 2, 3), 100),
        Rotation(FloatPoint3D(0, 0, 0), 0),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(100, angleA);
    EXPECT_EQ(0, angleB);

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(0, 0, 0), 100),
        Rotation(FloatPoint3D(1, 2, 3), 100),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(0, angleA);
    EXPECT_EQ(100, angleB);

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(3, 2, 1), 0),
        Rotation(FloatPoint3D(1, 2, 3), 100),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(0, angleA);
    EXPECT_EQ(100, angleB);

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(1, 2, 3), 50),
        Rotation(FloatPoint3D(1, 2, 3), 100),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(50, angleA);
    EXPECT_EQ(100, angleB);

    EXPECT_TRUE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(1, 2, 3), 50),
        Rotation(FloatPoint3D(2, 4, 6), 100),
        axis, angleA, angleB));
    EXPECT_EQ(FloatPoint3D(1, 2, 3), axis);
    EXPECT_EQ(50, angleA);
    EXPECT_EQ(100, angleB);

    EXPECT_FALSE(Rotation::getCommonAxis(
        Rotation(FloatPoint3D(1, 2, 3), 50),
        Rotation(FloatPoint3D(3, 2, 1), 100),
        axis, angleA, angleB));
}

} // namespace blink
