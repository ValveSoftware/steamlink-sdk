// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatRect.h"

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/GeometryTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(FloatRectTest, SquaredDistanceToTest)
{

    //
    //  O--x
    //  |
    //  y
    //
    //     FloatRect.x()   FloatRect.maxX()
    //            |          |
    //        1   |    2     |  3
    //      ======+==========+======   --FloatRect.y()
    //        4   |    5(in) |  6
    //      ======+==========+======   --FloatRect.maxY()
    //        7   |    8     |  9
    //

    FloatRect r1(100, 100, 250, 150);

    // `1` case
    FloatPoint p1(80, 80);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p1), 800.f);

    FloatPoint p2(-10, -10);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p2), 24200.f);

    FloatPoint p3(80, -10);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p3), 12500.f);

    // `2` case
    FloatPoint p4(110, 80);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p4), 400.f);

    FloatPoint p5(150, 0);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p5), 10000.f);

    FloatPoint p6(180, -10);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p6), 12100.f);

    // `3` case
    FloatPoint p7(400, 80);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p7), 2900.f);

    FloatPoint p8(360, -10);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p8), 12200.f);

    // `4` case
    FloatPoint p9(80, 110);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p9), 400.f);

    FloatPoint p10(-10, 180);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p10), 12100.f);

    // `5`(& In) case
    FloatPoint p11(100, 100);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p11), 0.f);

    FloatPoint p12(150, 100);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p12), 0.f);

    FloatPoint p13(350, 100);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p13), 0.f);

    FloatPoint p14(350, 150);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p14), 0.f);

    FloatPoint p15(350, 250);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p15), 0.f);

    FloatPoint p16(150, 250);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p16), 0.f);

    FloatPoint p17(100, 250);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p17), 0.f);

    FloatPoint p18(100, 150);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p18), 0.f);

    FloatPoint p19(150, 150);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p19), 0.f);

    // `6` case
    FloatPoint p20(380, 150);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p20), 900.f);

    // `7` case
    FloatPoint p21(80, 280);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p21), 1300.f);

    FloatPoint p22(-10, 300);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p22), 14600.f);

    // `8` case
    FloatPoint p23(180, 300);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p23), 2500.f);

    // `9` case
    FloatPoint p24(450, 450);
    EXPECT_PRED_FORMAT2(GeometryTest::AssertAlmostEqual, r1.squaredDistanceTo(p24), 50000.f);
}

#ifndef NDEBUG
TEST(FloatRectTest, ToString)
{
    FloatRect emptyRect = FloatRect();
    EXPECT_EQ(String("0.000000,0.000000 0.000000x0.000000"), emptyRect.toString());

    FloatRect rect(1, 2, 3, 4);
    EXPECT_EQ(String("1.000000,2.000000 3.000000x4.000000"), rect.toString());

    FloatRect granularRect(1.6f, 2.7f, 3.8f, 4.9f);
    EXPECT_EQ(String("1.600000,2.700000 3.800000x4.900000"), granularRect.toString());
}
#endif

} // namespace blink
