// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatAnimationCurve.h"

#include "cc/animation/timing_function.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

using blink::CompositorAnimationCurve;
using blink::CompositorFloatAnimationCurve;
using blink::CompositorFloatKeyframe;

namespace blink {

// Tests that a float animation with one keyframe works as expected.
TEST(WebFloatAnimationCurveTest, OneFloatKeyframe) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::shared()));
  EXPECT_FLOAT_EQ(2, curve->getValue(-1));
  EXPECT_FLOAT_EQ(2, curve->getValue(0));
  EXPECT_FLOAT_EQ(2, curve->getValue(0.5));
  EXPECT_FLOAT_EQ(2, curve->getValue(1));
  EXPECT_FLOAT_EQ(2, curve->getValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(WebFloatAnimationCurveTest, TwoFloatKeyframe) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::shared()));
  EXPECT_FLOAT_EQ(2, curve->getValue(-1));
  EXPECT_FLOAT_EQ(2, curve->getValue(0));
  EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->getValue(1));
  EXPECT_FLOAT_EQ(4, curve->getValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(WebFloatAnimationCurveTest, ThreeFloatKeyframe) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(2, 8, *LinearTimingFunction::shared()));
  EXPECT_FLOAT_EQ(2, curve->getValue(-1));
  EXPECT_FLOAT_EQ(2, curve->getValue(0));
  EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->getValue(1));
  EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
  EXPECT_FLOAT_EQ(8, curve->getValue(2));
  EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(WebFloatAnimationCurveTest, RepeatedFloatKeyTimes) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 4, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 6, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(2, 6, *LinearTimingFunction::shared()));

  EXPECT_FLOAT_EQ(4, curve->getValue(-1));
  EXPECT_FLOAT_EQ(4, curve->getValue(0));
  EXPECT_FLOAT_EQ(4, curve->getValue(0.5));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  float value = curve->getValue(1);
  EXPECT_TRUE(value >= 4 && value <= 6);

  EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
  EXPECT_FLOAT_EQ(6, curve->getValue(2));
  EXPECT_FLOAT_EQ(6, curve->getValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(WebFloatAnimationCurveTest, UnsortedKeyframes) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(2, 8, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 2, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 4, *LinearTimingFunction::shared()));

  EXPECT_FLOAT_EQ(2, curve->getValue(-1));
  EXPECT_FLOAT_EQ(2, curve->getValue(0));
  EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
  EXPECT_FLOAT_EQ(4, curve->getValue(1));
  EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
  EXPECT_FLOAT_EQ(8, curve->getValue(2));
  EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(WebFloatAnimationCurveTest, CubicBezierTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  RefPtr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::create(0.25, 0, 0.75, 1);
  curve->addKeyframe(CompositorFloatKeyframe(0, 0, *cubic));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  EXPECT_FLOAT_EQ(0, curve->getValue(0));
  EXPECT_LT(0, curve->getValue(0.25));
  EXPECT_GT(0.25, curve->getValue(0.25));
  EXPECT_NEAR(curve->getValue(0.5), 0.5, 0.00015);
  EXPECT_LT(0.75, curve->getValue(0.75));
  EXPECT_GT(1, curve->getValue(0.75));
  EXPECT_FLOAT_EQ(1, curve->getValue(1));
}

// Tests that an ease timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(CompositorFloatKeyframe(
      0, 0, *CubicBezierTimingFunction::preset(
                CubicBezierTimingFunction::EaseType::EASE)));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

// Tests using a linear timing function.
TEST(WebFloatAnimationCurveTest, LinearTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(
      CompositorFloatKeyframe(0, 0, *LinearTimingFunction::shared()));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(time, curve->getValue(time));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(CompositorFloatKeyframe(
      0, 0, *CubicBezierTimingFunction::preset(
                CubicBezierTimingFunction::EaseType::EASE_IN)));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseOutTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(CompositorFloatKeyframe(
      0, 0, *CubicBezierTimingFunction::preset(
                CubicBezierTimingFunction::EaseType::EASE_OUT)));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, EaseInOutTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(CompositorFloatKeyframe(
      0, 0, *CubicBezierTimingFunction::preset(
                CubicBezierTimingFunction::EaseType::EASE_IN_OUT)));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

// Tests that an ease in timing function works as expected.
TEST(WebFloatAnimationCurveTest, CustomBezierTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  double x1 = 0.3;
  double y1 = 0.2;
  double x2 = 0.8;
  double y2 = 0.7;
  RefPtr<CubicBezierTimingFunction> cubic =
      CubicBezierTimingFunction::create(x1, y1, x2, y2);
  curve->addKeyframe(CompositorFloatKeyframe(0, 0, *cubic));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

// Tests that the default timing function is indeed ease.
TEST(WebFloatAnimationCurveTest, DefaultTimingFunction) {
  std::unique_ptr<CompositorFloatAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  curve->addKeyframe(CompositorFloatKeyframe(
      0, 0, *CubicBezierTimingFunction::preset(
                CubicBezierTimingFunction::EaseType::EASE)));
  curve->addKeyframe(
      CompositorFloatKeyframe(1, 1, *LinearTimingFunction::shared()));

  std::unique_ptr<cc::TimingFunction> timingFunction(
      cc::CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE));
  for (int i = 0; i <= 4; ++i) {
    const double time = i * 0.25;
    EXPECT_FLOAT_EQ(timingFunction->GetValue(time), curve->getValue(time));
  }
}

}  // namespace blink
