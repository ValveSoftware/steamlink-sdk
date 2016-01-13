// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/android/scroller.h"

namespace gfx {

namespace {

const float kDefaultStartX = 7.f;
const float kDefaultStartY = 25.f;
const float kDefaultDeltaX = -20.f;
const float kDefaultDeltaY = 73.f;
const float kDefaultVelocityX = -35.f;
const float kDefaultVelocityY = 22.f;
const float kEpsilon = 1e-3f;

Scroller::Config DefaultConfig() {
  return Scroller::Config();
}

}  // namespace

class ScrollerTest : public testing::Test {};

TEST_F(ScrollerTest, Scroll) {
  Scroller scroller(DefaultConfig());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Start a scroll and verify initialized values.
  scroller.StartScroll(kDefaultStartX,
                       kDefaultStartY,
                       kDefaultDeltaX,
                       kDefaultDeltaY,
                       start_time);

  EXPECT_EQ(kDefaultStartX, scroller.GetStartX());
  EXPECT_EQ(kDefaultStartY, scroller.GetStartY());
  EXPECT_EQ(kDefaultStartX, scroller.GetCurrX());
  EXPECT_EQ(kDefaultStartY, scroller.GetCurrY());
  EXPECT_EQ(kDefaultStartX + kDefaultDeltaX, scroller.GetFinalX());
  EXPECT_EQ(kDefaultStartY + kDefaultDeltaY, scroller.GetFinalY());
  EXPECT_FALSE(scroller.IsFinished());
  EXPECT_EQ(base::TimeDelta(), scroller.GetTimePassed());

  // Advance halfway through the scroll.
  const base::TimeDelta scroll_duration = scroller.GetDuration();
  scroller.ComputeScrollOffset(start_time + scroll_duration / 2);

  // Ensure we've moved in the direction of the delta, but have yet to reach
  // the target.
  EXPECT_GT(kDefaultStartX, scroller.GetCurrX());
  EXPECT_LT(kDefaultStartY, scroller.GetCurrY());
  EXPECT_LT(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_GT(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_FALSE(scroller.IsFinished());

  // Ensure our velocity is non-zero and in the same direction as the delta.
  EXPECT_GT(0.f, scroller.GetCurrVelocityX() * kDefaultDeltaX);
  EXPECT_GT(0.f, scroller.GetCurrVelocityY() * kDefaultDeltaY);
  EXPECT_TRUE(scroller.IsScrollingInDirection(kDefaultDeltaX, kDefaultDeltaY));

  // Repeated offset computations at the same timestamp should yield identical
  // results.
  float curr_x = scroller.GetCurrX();
  float curr_y = scroller.GetCurrY();
  float curr_velocity_x = scroller.GetCurrVelocityX();
  float curr_velocity_y = scroller.GetCurrVelocityY();
  scroller.ComputeScrollOffset(start_time + scroll_duration / 2);
  EXPECT_EQ(curr_x, scroller.GetCurrX());
  EXPECT_EQ(curr_y, scroller.GetCurrY());
  EXPECT_EQ(curr_velocity_x, scroller.GetCurrVelocityX());
  EXPECT_EQ(curr_velocity_y, scroller.GetCurrVelocityY());

  // Advance to the end.
  scroller.ComputeScrollOffset(start_time + scroll_duration);
  EXPECT_EQ(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_EQ(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
  EXPECT_NEAR(0.f, scroller.GetCurrVelocityX(), kEpsilon);
  EXPECT_NEAR(0.f, scroller.GetCurrVelocityY(), kEpsilon);

  // Try to advance further; nothing should change.
  scroller.ComputeScrollOffset(start_time + scroll_duration * 2);
  EXPECT_EQ(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_EQ(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
}

TEST_F(ScrollerTest, Fling) {
  Scroller scroller(DefaultConfig());
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Start a fling and verify initialized values.
  scroller.Fling(kDefaultStartX,
                 kDefaultStartY,
                 kDefaultVelocityX,
                 kDefaultVelocityY,
                 INT_MIN,
                 INT_MAX,
                 INT_MIN,
                 INT_MAX,
                 start_time);

  EXPECT_EQ(kDefaultStartX, scroller.GetStartX());
  EXPECT_EQ(kDefaultStartY, scroller.GetStartY());
  EXPECT_EQ(kDefaultStartX, scroller.GetCurrX());
  EXPECT_EQ(kDefaultStartY, scroller.GetCurrY());
  EXPECT_GT(kDefaultStartX, scroller.GetFinalX());
  EXPECT_LT(kDefaultStartY, scroller.GetFinalY());
  EXPECT_FALSE(scroller.IsFinished());
  EXPECT_EQ(base::TimeDelta(), scroller.GetTimePassed());

  // Advance halfway through the fling.
  const base::TimeDelta scroll_duration = scroller.GetDuration();
  scroller.ComputeScrollOffset(start_time + scroll_duration / 2);

  // Ensure we've moved in the direction of the velocity, but have yet to reach
  // the target.
  EXPECT_GT(kDefaultStartX, scroller.GetCurrX());
  EXPECT_LT(kDefaultStartY, scroller.GetCurrY());
  EXPECT_LT(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_GT(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_FALSE(scroller.IsFinished());

  // Ensure our velocity is non-zero and in the same direction as the original
  // velocity.
  EXPECT_LT(0.f, scroller.GetCurrVelocityX() * kDefaultVelocityX);
  EXPECT_LT(0.f, scroller.GetCurrVelocityY() * kDefaultVelocityY);
  EXPECT_TRUE(
      scroller.IsScrollingInDirection(kDefaultVelocityX, kDefaultVelocityY));

  // Repeated offset computations at the same timestamp should yield identical
  // results.
  float curr_x = scroller.GetCurrX();
  float curr_y = scroller.GetCurrY();
  float curr_velocity_x = scroller.GetCurrVelocityX();
  float curr_velocity_y = scroller.GetCurrVelocityY();
  scroller.ComputeScrollOffset(start_time + scroll_duration / 2);
  EXPECT_EQ(curr_x, scroller.GetCurrX());
  EXPECT_EQ(curr_y, scroller.GetCurrY());
  EXPECT_EQ(curr_velocity_x, scroller.GetCurrVelocityX());
  EXPECT_EQ(curr_velocity_y, scroller.GetCurrVelocityY());

  // Advance to the end.
  scroller.ComputeScrollOffset(start_time + scroll_duration);
  EXPECT_EQ(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_EQ(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
  EXPECT_NEAR(0.f, scroller.GetCurrVelocityX(), kEpsilon);
  EXPECT_NEAR(0.f, scroller.GetCurrVelocityY(), kEpsilon);

  // Try to advance further; nothing should change.
  scroller.ComputeScrollOffset(start_time + scroll_duration * 2);
  EXPECT_EQ(scroller.GetFinalX(), scroller.GetCurrX());
  EXPECT_EQ(scroller.GetFinalY(), scroller.GetCurrY());
  EXPECT_TRUE(scroller.IsFinished());
  EXPECT_EQ(scroll_duration, scroller.GetTimePassed());
}

}  // namespace gfx
