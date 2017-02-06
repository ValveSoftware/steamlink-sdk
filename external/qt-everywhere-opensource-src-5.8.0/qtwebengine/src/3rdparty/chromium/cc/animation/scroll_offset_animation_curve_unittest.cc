// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include "cc/animation/timing_function.h"
#include "cc/base/time_util.h"
#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

const double kConstantDuration = 9.0;
const double kDurationDivisor = 60.0;

namespace cc {
namespace {

TEST(ScrollOffsetAnimationCurveTest, DeltaBasedDuration) {
  gfx::ScrollOffset target_value(100.f, 200.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));

  curve->SetInitialValue(target_value);
  EXPECT_DOUBLE_EQ(0.0, curve->Duration().InSecondsF());

  // x decreases, y stays the same.
  curve->SetInitialValue(gfx::ScrollOffset(136.f, 200.f));
  EXPECT_DOUBLE_EQ(0.1, curve->Duration().InSecondsF());

  // x increases, y stays the same.
  curve->SetInitialValue(gfx::ScrollOffset(19.f, 200.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration().InSecondsF());

  // x stays the same, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(100.f, 344.f));
  EXPECT_DOUBLE_EQ(0.2, curve->Duration().InSecondsF());

  // x stays the same, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(100.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration().InSecondsF());

  // x decreases, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(32500.f, 500.f));
  EXPECT_DOUBLE_EQ(3.0, curve->Duration().InSecondsF());

  // x decreases, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(150.f, 119.f));
  EXPECT_DOUBLE_EQ(0.15, curve->Duration().InSecondsF());

  // x increases, y decreases.
  curve->SetInitialValue(gfx::ScrollOffset(0.f, 14600.f));
  EXPECT_DOUBLE_EQ(2.0, curve->Duration().InSecondsF());

  // x increases, y increases.
  curve->SetInitialValue(gfx::ScrollOffset(95.f, 191.f));
  EXPECT_DOUBLE_EQ(0.05, curve->Duration().InSecondsF());
}

TEST(ScrollOffsetAnimationCurveTest, GetValue) {
  gfx::ScrollOffset initial_value(2.f, 40.f);
  gfx::ScrollOffset target_value(10.f, 20.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);

  base::TimeDelta duration = curve->Duration();
  EXPECT_GT(curve->Duration().InSecondsF(), 0);
  EXPECT_LT(curve->Duration().InSecondsF(), 0.1);

  EXPECT_EQ(AnimationCurve::SCROLL_OFFSET, curve->Type());
  EXPECT_EQ(duration, curve->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value,
                      curve->GetValue(base::TimeDelta::FromSecondsD(-1.0)));
  EXPECT_VECTOR2DF_EQ(initial_value, curve->GetValue(base::TimeDelta()));
  EXPECT_VECTOR2DF_NEAR(gfx::ScrollOffset(6.f, 30.f),
                        curve->GetValue(TimeUtil::Scale(duration, 0.5f)),
                        0.00025);
  EXPECT_VECTOR2DF_EQ(target_value, curve->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(
      target_value,
      curve->GetValue(duration + base::TimeDelta::FromSecondsD(1.0)));

  // Verify that GetValue takes the timing function into account.
  gfx::ScrollOffset value = curve->GetValue(TimeUtil::Scale(duration, 0.25f));
  EXPECT_NEAR(3.0333f, value.x(), 0.0002f);
  EXPECT_NEAR(37.4168f, value.y(), 0.0002f);
}

// Verify that a clone behaves exactly like the original.
TEST(ScrollOffsetAnimationCurveTest, Clone) {
  gfx::ScrollOffset initial_value(2.f, 40.f);
  gfx::ScrollOffset target_value(10.f, 20.f);
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->SetInitialValue(initial_value);
  base::TimeDelta duration = curve->Duration();

  std::unique_ptr<AnimationCurve> clone(curve->Clone());

  EXPECT_EQ(AnimationCurve::SCROLL_OFFSET, clone->Type());
  EXPECT_EQ(duration, clone->Duration());

  EXPECT_VECTOR2DF_EQ(initial_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(
                          base::TimeDelta::FromSecondsD(-1.0)));
  EXPECT_VECTOR2DF_EQ(
      initial_value,
      clone->ToScrollOffsetAnimationCurve()->GetValue(base::TimeDelta()));
  EXPECT_VECTOR2DF_NEAR(gfx::ScrollOffset(6.f, 30.f),
                        clone->ToScrollOffsetAnimationCurve()->GetValue(
                            TimeUtil::Scale(duration, 0.5f)),
                        0.00025);
  EXPECT_VECTOR2DF_EQ(
      target_value, clone->ToScrollOffsetAnimationCurve()->GetValue(duration));
  EXPECT_VECTOR2DF_EQ(target_value,
                      clone->ToScrollOffsetAnimationCurve()->GetValue(
                          duration + base::TimeDelta::FromSecondsD(1.f)));

  // Verify that the timing function was cloned correctly.
  gfx::ScrollOffset value = clone->ToScrollOffsetAnimationCurve()->GetValue(
      TimeUtil::Scale(duration, 0.25f));
  EXPECT_NEAR(3.0333f, value.x(), 0.0002f);
  EXPECT_NEAR(37.4168f, value.y(), 0.0002f);
}

TEST(ScrollOffsetAnimationCurveTest, UpdateTarget) {
  gfx::ScrollOffset initial_value(0.f, 0.f);
  gfx::ScrollOffset target_value(0.f, 3600.f);
  double duration = kConstantDuration / kDurationDivisor;
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          target_value, CubicBezierTimingFunction::CreatePreset(
                            CubicBezierTimingFunction::EaseType::EASE_IN_OUT),
          ScrollOffsetAnimationCurve::DurationBehavior::CONSTANT));
  curve->SetInitialValue(initial_value);
  EXPECT_NEAR(duration, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(
      1800.0,
      curve->GetValue(base::TimeDelta::FromSecondsD(duration / 2.0)).y(),
      0.0002f);
  EXPECT_NEAR(3600.0,
              curve->GetValue(base::TimeDelta::FromSecondsD(duration)).y(),
              0.0002f);

  curve->UpdateTarget(duration / 2, gfx::ScrollOffset(0.0, 9900.0));

  EXPECT_NEAR(duration * 1.5, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(
      1800.0,
      curve->GetValue(base::TimeDelta::FromSecondsD(duration / 2.0)).y(),
      0.0002f);
  EXPECT_NEAR(6827.6,
              curve->GetValue(base::TimeDelta::FromSecondsD(duration)).y(),
              0.1f);
  EXPECT_NEAR(
      9900.0,
      curve->GetValue(base::TimeDelta::FromSecondsD(duration * 1.5)).y(),
      0.0002f);

  curve->UpdateTarget(duration, gfx::ScrollOffset(0.0, 7200.0));

  // A closer target at high velocity reduces the duration.
  EXPECT_NEAR(duration * 1.0794, curve->Duration().InSecondsF(), 0.0002f);
  EXPECT_NEAR(6827.6,
              curve->GetValue(base::TimeDelta::FromSecondsD(duration)).y(),
              0.1f);
  EXPECT_NEAR(
      7200.0,
      curve->GetValue(base::TimeDelta::FromSecondsD(duration * 1.08)).y(),
      0.0002f);
}

TEST(ScrollOffsetAnimationCurveTest, InverseDeltaDuration) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurve::Create(
          gfx::ScrollOffset(0.f, 100.f),
          CubicBezierTimingFunction::CreatePreset(
              CubicBezierTimingFunction::EaseType::EASE_IN_OUT),
          ScrollOffsetAnimationCurve::DurationBehavior::INVERSE_DELTA));

  curve->SetInitialValue(gfx::ScrollOffset());
  double smallDeltaDuration = curve->Duration().InSecondsF();

  curve->UpdateTarget(0.f, gfx::ScrollOffset(0.f, 300.f));
  double mediumDeltaDuration = curve->Duration().InSecondsF();

  curve->UpdateTarget(0.f, gfx::ScrollOffset(0.f, 500.f));
  double largeDeltaDuration = curve->Duration().InSecondsF();

  EXPECT_GT(smallDeltaDuration, mediumDeltaDuration);
  EXPECT_GT(mediumDeltaDuration, largeDeltaDuration);

  curve->UpdateTarget(0.f, gfx::ScrollOffset(0.f, 5000.f));
  EXPECT_EQ(largeDeltaDuration, curve->Duration().InSecondsF());
}

}  // namespace
}  // namespace cc
