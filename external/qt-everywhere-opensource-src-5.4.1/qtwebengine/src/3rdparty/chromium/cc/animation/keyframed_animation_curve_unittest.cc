// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframed_animation_curve.h"

#include "cc/animation/transform_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/box_f.h"
#include "ui/gfx/test/gfx_util.h"

namespace cc {
namespace {

void ExpectTranslateX(SkMScalar translate_x, const gfx::Transform& transform) {
  EXPECT_FLOAT_EQ(translate_x, transform.matrix().get(0, 3));
}

void ExpectBrightness(double brightness, const FilterOperations& filter) {
  EXPECT_EQ(1u, filter.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, filter.at(0).type());
  EXPECT_FLOAT_EQ(brightness, filter.at(0).amount());
}

// Tests that a color animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneColorKeyFrame) {
  SkColor color = SkColorSetARGB(255, 255, 255, 255);
  scoped_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(0.0, color, scoped_ptr<TimingFunction>()));

  EXPECT_SKCOLOR_EQ(color, curve->GetValue(-1.f));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(0.f));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(0.5f));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(1.f));
  EXPECT_SKCOLOR_EQ(color, curve->GetValue(2.f));
}

// Tests that a color animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 255, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 0, 255, 0);
  SkColor color_midpoint = gfx::Tween::ColorValueBetween(0.5, color_a, color_b);
  scoped_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(0.0, color_a, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(1.0, color_b, scoped_ptr<TimingFunction>()));

  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(-1.f));
  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(0.f));
  EXPECT_SKCOLOR_EQ(color_midpoint, curve->GetValue(0.5f));
  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(1.f));
  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(2.f));
}

// Tests that a color animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 255, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 0, 255, 0);
  SkColor color_c = SkColorSetARGB(255, 0, 0, 255);
  SkColor color_midpoint1 =
      gfx::Tween::ColorValueBetween(0.5, color_a, color_b);
  SkColor color_midpoint2 =
      gfx::Tween::ColorValueBetween(0.5, color_b, color_c);
  scoped_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(0.0, color_a, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(1.0, color_b, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(2.0, color_c, scoped_ptr<TimingFunction>()));

  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(-1.f));
  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(0.f));
  EXPECT_SKCOLOR_EQ(color_midpoint1, curve->GetValue(0.5f));
  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(1.f));
  EXPECT_SKCOLOR_EQ(color_midpoint2, curve->GetValue(1.5f));
  EXPECT_SKCOLOR_EQ(color_c, curve->GetValue(2.f));
  EXPECT_SKCOLOR_EQ(color_c, curve->GetValue(3.f));
}

// Tests that a colro animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedColorKeyFrame) {
  SkColor color_a = SkColorSetARGB(255, 64, 0, 0);
  SkColor color_b = SkColorSetARGB(255, 192, 0, 0);

  scoped_ptr<KeyframedColorAnimationCurve> curve(
      KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      ColorKeyframe::Create(0.0, color_a, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(1.0, color_a, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(1.0, color_b, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      ColorKeyframe::Create(2.0, color_b, scoped_ptr<TimingFunction>()));

  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(-1.f));
  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(0.f));
  EXPECT_SKCOLOR_EQ(color_a, curve->GetValue(0.5f));

  SkColor value = curve->GetValue(1.0f);
  EXPECT_EQ(255u, SkColorGetA(value));
  int red_value = SkColorGetR(value);
  EXPECT_LE(64, red_value);
  EXPECT_GE(192, red_value);

  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(1.5f));
  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(2.f));
  EXPECT_SKCOLOR_EQ(color_b, curve->GetValue(3.f));
}

// Tests that a float animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneFloatKeyframe) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(0.0, 2.f, scoped_ptr<TimingFunction>()));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(-1.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(0.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(0.5f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(1.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(2.f));
}

// Tests that a float animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoFloatKeyframe) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(0.0, 2.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 4.f, scoped_ptr<TimingFunction>()));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(-1.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(0.f));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(0.5f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(1.f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(2.f));
}

// Tests that a float animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeFloatKeyframe) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(0.0, 2.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 4.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(2.0, 8.f, scoped_ptr<TimingFunction>()));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(-1.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(0.f));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(0.5f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(1.f));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(1.5f));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(2.f));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(3.f));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedFloatKeyTimes) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(0.0, 4.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 4.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 6.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(2.0, 6.f, scoped_ptr<TimingFunction>()));

  EXPECT_FLOAT_EQ(4.f, curve->GetValue(-1.f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(0.f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(0.5f));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  float value = curve->GetValue(1.f);
  EXPECT_TRUE(value >= 4 && value <= 6);

  EXPECT_FLOAT_EQ(6.f, curve->GetValue(1.5f));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(2.f));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(3.f));
}

// Tests that a transform animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneTransformKeyframe) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations;
  operations.AppendTranslate(2.f, 0.f, 0.f);
  curve->AddKeyframe(
      TransformKeyframe::Create(0.f, operations, scoped_ptr<TimingFunction>()));

  ExpectTranslateX(2.f, curve->GetValue(-1.f));
  ExpectTranslateX(2.f, curve->GetValue(0.f));
  ExpectTranslateX(2.f, curve->GetValue(0.5f));
  ExpectTranslateX(2.f, curve->GetValue(1.f));
  ExpectTranslateX(2.f, curve->GetValue(2.f));
}

// Tests that a transform animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoTransformKeyframe) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendTranslate(2.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);

  curve->AddKeyframe(TransformKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  ExpectTranslateX(2.f, curve->GetValue(-1.f));
  ExpectTranslateX(2.f, curve->GetValue(0.f));
  ExpectTranslateX(3.f, curve->GetValue(0.5f));
  ExpectTranslateX(4.f, curve->GetValue(1.f));
  ExpectTranslateX(4.f, curve->GetValue(2.f));
}

// Tests that a transform animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeTransformKeyframe) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  TransformOperations operations1;
  operations1.AppendTranslate(2.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations3;
  operations3.AppendTranslate(8.f, 0.f, 0.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      2.f, operations3, scoped_ptr<TimingFunction>()));
  ExpectTranslateX(2.f, curve->GetValue(-1.f));
  ExpectTranslateX(2.f, curve->GetValue(0.f));
  ExpectTranslateX(3.f, curve->GetValue(0.5f));
  ExpectTranslateX(4.f, curve->GetValue(1.f));
  ExpectTranslateX(6.f, curve->GetValue(1.5f));
  ExpectTranslateX(8.f, curve->GetValue(2.f));
  ExpectTranslateX(8.f, curve->GetValue(3.f));
}

// Tests that a transform animation with multiple keys at a given time works
// sanely.
TEST(KeyframedAnimationCurveTest, RepeatedTransformKeyTimes) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());
  // A step function.
  TransformOperations operations1;
  operations1.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations2;
  operations2.AppendTranslate(4.f, 0.f, 0.f);
  TransformOperations operations3;
  operations3.AppendTranslate(6.f, 0.f, 0.f);
  TransformOperations operations4;
  operations4.AppendTranslate(6.f, 0.f, 0.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      1.f, operations3, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(TransformKeyframe::Create(
      2.f, operations4, scoped_ptr<TimingFunction>()));

  ExpectTranslateX(4.f, curve->GetValue(-1.f));
  ExpectTranslateX(4.f, curve->GetValue(0.f));
  ExpectTranslateX(4.f, curve->GetValue(0.5f));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  gfx::Transform value = curve->GetValue(1.f);
  EXPECT_GE(value.matrix().get(0, 3), 4.f);
  EXPECT_LE(value.matrix().get(0, 3), 6.f);

  ExpectTranslateX(6.f, curve->GetValue(1.5f));
  ExpectTranslateX(6.f, curve->GetValue(2.f));
  ExpectTranslateX(6.f, curve->GetValue(3.f));
}

// Tests that a filter animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneFilterKeyframe) {
  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations;
  operations.Append(FilterOperation::CreateBrightnessFilter(2.f));
  curve->AddKeyframe(
      FilterKeyframe::Create(0.f, operations, scoped_ptr<TimingFunction>()));

  ExpectBrightness(2.f, curve->GetValue(-1.f));
  ExpectBrightness(2.f, curve->GetValue(0.f));
  ExpectBrightness(2.f, curve->GetValue(0.5f));
  ExpectBrightness(2.f, curve->GetValue(1.f));
  ExpectBrightness(2.f, curve->GetValue(2.f));
}

// Tests that a filter animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoFilterKeyframe) {
  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(2.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));

  curve->AddKeyframe(FilterKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  ExpectBrightness(2.f, curve->GetValue(-1.f));
  ExpectBrightness(2.f, curve->GetValue(0.f));
  ExpectBrightness(3.f, curve->GetValue(0.5f));
  ExpectBrightness(4.f, curve->GetValue(1.f));
  ExpectBrightness(4.f, curve->GetValue(2.f));
}

// Tests that a filter animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeFilterKeyframe) {
  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(2.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations3;
  operations3.Append(FilterOperation::CreateBrightnessFilter(8.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      2.f, operations3, scoped_ptr<TimingFunction>()));
  ExpectBrightness(2.f, curve->GetValue(-1.f));
  ExpectBrightness(2.f, curve->GetValue(0.f));
  ExpectBrightness(3.f, curve->GetValue(0.5f));
  ExpectBrightness(4.f, curve->GetValue(1.f));
  ExpectBrightness(6.f, curve->GetValue(1.5f));
  ExpectBrightness(8.f, curve->GetValue(2.f));
  ExpectBrightness(8.f, curve->GetValue(3.f));
}

// Tests that a filter animation with multiple keys at a given time works
// sanely.
TEST(KeyframedAnimationCurveTest, RepeatedFilterKeyTimes) {
  scoped_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());
  // A step function.
  FilterOperations operations1;
  operations1.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations2;
  operations2.Append(FilterOperation::CreateBrightnessFilter(4.f));
  FilterOperations operations3;
  operations3.Append(FilterOperation::CreateBrightnessFilter(6.f));
  FilterOperations operations4;
  operations4.Append(FilterOperation::CreateBrightnessFilter(6.f));
  curve->AddKeyframe(FilterKeyframe::Create(
      0.f, operations1, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      1.f, operations2, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      1.f, operations3, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(FilterKeyframe::Create(
      2.f, operations4, scoped_ptr<TimingFunction>()));

  ExpectBrightness(4.f, curve->GetValue(-1.f));
  ExpectBrightness(4.f, curve->GetValue(0.f));
  ExpectBrightness(4.f, curve->GetValue(0.5f));

  // There is a discontinuity at 1. Any value between 4 and 6 is valid.
  FilterOperations value = curve->GetValue(1.f);
  EXPECT_EQ(1u, value.size());
  EXPECT_EQ(FilterOperation::BRIGHTNESS, value.at(0).type());
  EXPECT_GE(value.at(0).amount(), 4);
  EXPECT_LE(value.at(0).amount(), 6);

  ExpectBrightness(6.f, curve->GetValue(1.5f));
  ExpectBrightness(6.f, curve->GetValue(2.f));
  ExpectBrightness(6.f, curve->GetValue(3.f));
}

// Tests that the keyframes may be added out of order.
TEST(KeyframedAnimationCurveTest, UnsortedKeyframes) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      FloatKeyframe::Create(2.0, 8.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(0.0, 2.f, scoped_ptr<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 4.f, scoped_ptr<TimingFunction>()));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(-1.f));
  EXPECT_FLOAT_EQ(2.f, curve->GetValue(0.f));
  EXPECT_FLOAT_EQ(3.f, curve->GetValue(0.5f));
  EXPECT_FLOAT_EQ(4.f, curve->GetValue(1.f));
  EXPECT_FLOAT_EQ(6.f, curve->GetValue(1.5f));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(2.f));
  EXPECT_FLOAT_EQ(8.f, curve->GetValue(3.f));
}

// Tests that a cubic bezier timing function works as expected.
TEST(KeyframedAnimationCurveTest, CubicBezierTimingFunction) {
  scoped_ptr<KeyframedFloatAnimationCurve> curve(
      KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(FloatKeyframe::Create(
      0.0,
      0.f,
      CubicBezierTimingFunction::Create(0.25f, 0.f, 0.75f, 1.f)
          .PassAs<TimingFunction>()));
  curve->AddKeyframe(
      FloatKeyframe::Create(1.0, 1.f, scoped_ptr<TimingFunction>()));

  EXPECT_FLOAT_EQ(0.f, curve->GetValue(0.f));
  EXPECT_LT(0.f, curve->GetValue(0.25f));
  EXPECT_GT(0.25f, curve->GetValue(0.25f));
  EXPECT_NEAR(curve->GetValue(0.5f), 0.5f, 0.00015f);
  EXPECT_LT(0.75f, curve->GetValue(0.75f));
  EXPECT_GT(1.f, curve->GetValue(0.75f));
  EXPECT_FLOAT_EQ(1.f, curve->GetValue(1.f));
}

// Tests that animated bounds are computed as expected.
TEST(KeyframedAnimationCurveTest, AnimatedBounds) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(TransformKeyframe::Create(
      0.0, operations1, scoped_ptr<TimingFunction>()));
  operations1.AppendTranslate(2.0, 3.0, -1.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      0.5, operations1, scoped_ptr<TimingFunction>()));
  TransformOperations operations2;
  operations2.AppendTranslate(4.0, 1.0, 2.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      1.0, operations2, EaseTimingFunction::Create()));

  gfx::BoxF box(2.f, 3.f, 4.f, 1.f, 3.f, 2.f);
  gfx::BoxF bounds;

  EXPECT_TRUE(curve->AnimatedBoundsForBox(box, &bounds));
  EXPECT_EQ(gfx::BoxF(2.f, 3.f, 3.f, 5.f, 6.f, 5.f).ToString(),
            bounds.ToString());
}

// Tests that animations that affect scale are correctly identified.
TEST(KeyframedAnimationCurveTest, AffectsScale) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(TransformKeyframe::Create(
      0.0, operations1, scoped_ptr<TimingFunction>()));
  operations1.AppendTranslate(2.0, 3.0, -1.0);
  TransformOperations operations2;
  operations2.AppendTranslate(4.0, 1.0, 2.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      1.0, operations2, scoped_ptr<TimingFunction>()));

  EXPECT_FALSE(curve->AffectsScale());

  TransformOperations operations3;
  operations3.AppendScale(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      2.0, operations3, scoped_ptr<TimingFunction>()));

  EXPECT_TRUE(curve->AffectsScale());

  TransformOperations operations4;
  operations3.AppendTranslate(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      3.0, operations4, scoped_ptr<TimingFunction>()));

  EXPECT_TRUE(curve->AffectsScale());
}

// Tests that animations that are translations are correctly identified.
TEST(KeyframedAnimationCurveTest, IsTranslation) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(TransformKeyframe::Create(
      0.0, operations1, scoped_ptr<TimingFunction>()));
  operations1.AppendTranslate(2.0, 3.0, -1.0);
  TransformOperations operations2;
  operations2.AppendTranslate(4.0, 1.0, 2.0);
  curve->AddKeyframe(TransformKeyframe::Create(
      1.0, operations2, scoped_ptr<TimingFunction>()));

  EXPECT_TRUE(curve->IsTranslation());

  TransformOperations operations3;
  operations3.AppendScale(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      2.0, operations3, scoped_ptr<TimingFunction>()));

  EXPECT_FALSE(curve->IsTranslation());

  TransformOperations operations4;
  operations3.AppendTranslate(2.f, 2.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      3.0, operations4, scoped_ptr<TimingFunction>()));

  EXPECT_FALSE(curve->IsTranslation());
}

// Tests that maximum scale is computed as expected.
TEST(KeyframedAnimationCurveTest, MaximumScale) {
  scoped_ptr<KeyframedTransformAnimationCurve> curve(
      KeyframedTransformAnimationCurve::Create());

  TransformOperations operations1;
  curve->AddKeyframe(TransformKeyframe::Create(
      0.0, operations1, scoped_ptr<TimingFunction>()));
  operations1.AppendScale(2.f, -3.f, 1.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      1.0, operations1, EaseTimingFunction::Create()));

  float maximum_scale = 0.f;
  EXPECT_TRUE(curve->MaximumScale(&maximum_scale));
  EXPECT_EQ(3.f, maximum_scale);

  TransformOperations operations2;
  operations2.AppendScale(6.f, 3.f, 2.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      2.0, operations2, EaseTimingFunction::Create()));

  EXPECT_TRUE(curve->MaximumScale(&maximum_scale));
  EXPECT_EQ(6.f, maximum_scale);

  TransformOperations operations3;
  operations3.AppendRotate(1.f, 0.f, 0.f, 90.f);
  curve->AddKeyframe(TransformKeyframe::Create(
      3.0, operations3, EaseTimingFunction::Create()));

  EXPECT_FALSE(curve->MaximumScale(&maximum_scale));
}

}  // namespace
}  // namespace cc
