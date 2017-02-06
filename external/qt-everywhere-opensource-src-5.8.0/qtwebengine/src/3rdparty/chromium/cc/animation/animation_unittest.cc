// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include "base/memory/ptr_util.h"
#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using base::TimeDelta;

static base::TimeTicks TicksFromSecondsF(double seconds) {
  return base::TimeTicks::FromInternalValue(seconds *
                                            base::Time::kMicrosecondsPerSecond);
}

std::unique_ptr<Animation> CreateAnimation(double iterations,
                                           double duration,
                                           double playback_rate) {
  std::unique_ptr<Animation> to_return(
      Animation::Create(base::WrapUnique(new FakeFloatAnimationCurve(duration)),
                        0, 1, TargetProperty::OPACITY));
  to_return->set_iterations(iterations);
  to_return->set_playback_rate(playback_rate);
  return to_return;
}

std::unique_ptr<Animation> CreateAnimation(double iterations, double duration) {
  return CreateAnimation(iterations, duration, 1);
}

std::unique_ptr<Animation> CreateAnimation(double iterations) {
  return CreateAnimation(iterations, 1, 1);
}

TEST(AnimationTest, TrimTimeZeroIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeOneIteration) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeOneHalfIteration) {
  std::unique_ptr<Animation> anim(CreateAnimation(1.5));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.9, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.9))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeInfiniteIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::Direction::REVERSE);
  EXPECT_EQ(
      1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)).InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateInfiniteIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateOneIteration) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateTwoIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(2));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                      .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateTwoHalfIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(2.5));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                      .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.50))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateReverseInfiniteIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateReverseOneIteration) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateReverseTwoIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(2));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                      .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                      .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                      .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeStartTime) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeStartTimeReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_start_time(TicksFromSecondsF(4));
  anim->set_direction(Animation::Direction::REVERSE);
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                     .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(6.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeTimeOffset) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(4000));
  anim->set_start_time(TicksFromSecondsF(4));
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeTimeOffsetReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(4000));
  anim->set_start_time(TicksFromSecondsF(4));
  anim->set_direction(Animation::Direction::REVERSE);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeNegativeTimeOffset) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-4000));

  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeNegativeTimeOffsetReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-4000));
  anim->set_direction(Animation::Direction::REVERSE);

  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                     .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(5.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimePauseResume) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  anim->SetRunState(Animation::PAUSED, TicksFromSecondsF(0.5));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                     .InSecondsF());
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(1024.0));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                     .InSecondsF());
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.5))
                   .InSecondsF());
}

TEST(AnimationTest, TrimTimePauseResumeReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Direction::REVERSE);
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  anim->SetRunState(Animation::PAUSED, TicksFromSecondsF(0.25));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(1024.0));
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.75))
                   .InSecondsF());
}

TEST(AnimationTest, TrimTimeSuspendResume) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  anim->Suspend(TicksFromSecondsF(0.5));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                     .InSecondsF());
  anim->Resume(TicksFromSecondsF(1024));
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                     .InSecondsF());
  EXPECT_EQ(1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.5))
                   .InSecondsF());
}

TEST(AnimationTest, TrimTimeSuspendResumeReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_direction(Animation::Direction::REVERSE);
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                      .InSecondsF());
  anim->Suspend(TicksFromSecondsF(0.75));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  anim->Resume(TicksFromSecondsF(1024));
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.0))
                      .InSecondsF());
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1024.25))
                   .InSecondsF());
}

TEST(AnimationTest, TrimTimeZeroDuration) {
  std::unique_ptr<Animation> anim(CreateAnimation(0, 0));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimeStarting) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 5.0));
  anim->SetRunState(Animation::STARTING, TicksFromSecondsF(0.0));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  anim->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  anim->set_start_time(TicksFromSecondsF(1.0));
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(3.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeNeedsSynchronizedStartTime) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 5.0));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  anim->set_needs_synchronized_start_time(true);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  anim->set_time_offset(TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  anim->set_start_time(TicksFromSecondsF(1.0));
  anim->set_needs_synchronized_start_time(false);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(3.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
}

TEST(AnimationTest, IsFinishedAtZeroIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(0));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, IsFinishedAtOneIteration) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.0)));
}

TEST(AnimationTest, IsFinishedAtInfiniteIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.5)));
}

TEST(AnimationTest, IsFinishedNegativeTimeOffset) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(-500));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));

  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.5)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(2.5)));
}

TEST(AnimationTest, IsFinishedPositiveTimeOffset) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_time_offset(TimeDelta::FromMilliseconds(500));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));

  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(-1.0)));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.5)));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, IsFinishedAtNotRunning) {
  std::unique_ptr<Animation> anim(CreateAnimation(0));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::PAUSED, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::WAITING_FOR_TARGET_AVAILABILITY,
                    TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
  anim->SetRunState(Animation::ABORTED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->IsFinishedAt(TicksFromSecondsF(0.0)));
}

TEST(AnimationTest, IsFinished) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::PAUSED, TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::WAITING_FOR_TARGET_AVAILABILITY,
                    TicksFromSecondsF(0.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
  anim->SetRunState(Animation::ABORTED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
}

TEST(AnimationTest, IsFinishedNeedsSynchronizedStartTime) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::PAUSED, TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::WAITING_FOR_TARGET_AVAILABILITY,
                    TicksFromSecondsF(2.0));
  EXPECT_FALSE(anim->is_finished());
  anim->SetRunState(Animation::FINISHED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
  anim->SetRunState(Animation::ABORTED, TicksFromSecondsF(0.0));
  EXPECT_TRUE(anim->is_finished());
}

TEST(AnimationTest, RunStateChangesIgnoredWhileSuspended) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->Suspend(TicksFromSecondsF(0));
  EXPECT_EQ(Animation::PAUSED, anim->run_state());
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(Animation::PAUSED, anim->run_state());
  anim->Resume(TicksFromSecondsF(0));
  anim->SetRunState(Animation::RUNNING, TicksFromSecondsF(0.0));
  EXPECT_EQ(Animation::RUNNING, anim->run_state());
}

TEST(AnimationTest, TrimTimePlaybackNormal) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 1, 1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackSlow) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 1, 0.5));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                      .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackFast) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 4, 2));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)).InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)).InSecondsF());
  EXPECT_EQ(
      4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
  EXPECT_EQ(
      4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackNormalReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 2, -1));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)).InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackSlowReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 2, -0.5));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)).InSecondsF());
  EXPECT_EQ(1.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                      .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(1.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                      .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
  EXPECT_EQ(0.75, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                      .InSecondsF());
  EXPECT_EQ(
      0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3)).InSecondsF());
  EXPECT_EQ(0.25, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                      .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackFastReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 2, -2));
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0)).InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                     .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackFastInfiniteIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(-1, 4, 4));
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)).InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)).InSecondsF());
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.0))
                   .InSecondsF());
  EXPECT_EQ(2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1000.5))
                   .InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackNormalDoubleReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 1, -1));
  anim->set_direction(Animation::Direction::REVERSE);
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
}

TEST(AnimationTest, TrimTimePlaybackFastDoubleReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 4, -2));
  anim->set_direction(Animation::Direction::REVERSE);
  EXPECT_EQ(0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                   .InSecondsF());
  EXPECT_EQ(
      0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0)).InSecondsF());
  EXPECT_EQ(
      1, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5)).InSecondsF());
  EXPECT_EQ(
      2, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0)).InSecondsF());
  EXPECT_EQ(
      3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5)).InSecondsF());
  EXPECT_EQ(
      4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0)).InSecondsF());
  EXPECT_EQ(
      4, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5)).InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFast) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 2, 2));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFastReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 2, 2));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeAlternateTwoIterationsPlaybackFastDoubleReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 2, -2));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
}

TEST(AnimationTest,
     TrimTimeAlternateReverseThreeIterationsPlaybackFastAlternateReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(3, 2, -2));
  anim->set_direction(Animation::Direction::ALTERNATE_REVERSE);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.75))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.75))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.25))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.75))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.25))
                     .InSecondsF());
}

TEST(AnimationTest,
     TrimTimeAlternateReverseTwoIterationsPlaybackNormalAlternate) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 2, -1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(2.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(1.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(4.5))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeIterationStart) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 1, 1));
  anim->set_iteration_start(0.5);
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeIterationStartAlternate) {
  std::unique_ptr<Animation> anim(CreateAnimation(2, 1, 1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  anim->set_iteration_start(0.3);
  EXPECT_EQ(0.3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(0.3, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.8, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.7))
                     .InSecondsF());
  EXPECT_EQ(0.7, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.2))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.7))
                     .InSecondsF());
}

TEST(AnimationTest, TrimTimeIterationStartAlternateThreeIterations) {
  std::unique_ptr<Animation> anim(CreateAnimation(3, 1, 1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  anim->set_iteration_start(1);
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(-1.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.5))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.5))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(0.5, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.5))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                     .InSecondsF());
}

TEST(AnimationTest,
     TrimTimeIterationStartAlternateThreeIterationsPlaybackReverse) {
  std::unique_ptr<Animation> anim(CreateAnimation(3, 1, -1));
  anim->set_direction(Animation::Direction::ALTERNATE_NORMAL);
  anim->set_iteration_start(1);
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(0.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(1.0))
                     .InSecondsF());
  EXPECT_EQ(0.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(2.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.0))
                     .InSecondsF());
  EXPECT_EQ(1.0, anim->TrimTimeToCurrentIteration(TicksFromSecondsF(3.5))
                     .InSecondsF());
}

TEST(AnimationTest, InEffectFillMode) {
  std::unique_ptr<Animation> anim(CreateAnimation(1));
  anim->set_fill_mode(Animation::FillMode::NONE);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::FORWARDS);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::BACKWARDS);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::BOTH);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));
}

TEST(AnimationTest, InEffectFillModePlayback) {
  std::unique_ptr<Animation> anim(CreateAnimation(1, 1, -1));
  anim->set_fill_mode(Animation::FillMode::NONE);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::FORWARDS);
  EXPECT_FALSE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::BACKWARDS);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));

  anim->set_fill_mode(Animation::FillMode::BOTH);
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(-1.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(0.0)));
  EXPECT_TRUE(anim->InEffect(TicksFromSecondsF(1.0)));
}

}  // namespace
}  // namespace cc
