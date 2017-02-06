// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_animation_curve_adapter.h"

#include "base/memory/ptr_util.h"
#include "cc/base/time_util.h"

namespace ui {

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    gfx::Tween::Type tween_type,
    gfx::Transform initial_value,
    gfx::Transform target_value,
    base::TimeDelta duration)
    : tween_type_(tween_type),
      initial_value_(initial_value),
      target_value_(target_value),
      duration_(duration) {
  gfx::DecomposeTransform(&decomposed_initial_value_, initial_value_);
  gfx::DecomposeTransform(&decomposed_target_value_, target_value_);
}

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    const TransformAnimationCurveAdapter& other) = default;

TransformAnimationCurveAdapter::~TransformAnimationCurveAdapter() {
}

base::TimeDelta TransformAnimationCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<cc::AnimationCurve> TransformAnimationCurveAdapter::Clone()
    const {
  return base::WrapUnique(new TransformAnimationCurveAdapter(
      tween_type_, initial_value_, target_value_, duration_));
}

gfx::Transform TransformAnimationCurveAdapter::GetValue(
    base::TimeDelta t) const {
  if (t >= duration_)
    return target_value_;
  if (t <= base::TimeDelta())
    return initial_value_;
  double progress = cc::TimeUtil::Divide(t, duration_);

  gfx::DecomposedTransform to_return;
  gfx::BlendDecomposedTransforms(&to_return,
                                 decomposed_target_value_,
                                 decomposed_initial_value_,
                                 gfx::Tween::CalculateValue(tween_type_,
                                                            progress));
  return gfx::ComposeTransform(to_return);
}

bool TransformAnimationCurveAdapter::AnimatedBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  // TODO(ajuma): Once cc::TransformOperation::BlendedBoundsForBox supports
  // computing bounds for TransformOperationMatrix, use that to compute
  // the bounds we need here.
  return false;
}

bool TransformAnimationCurveAdapter::AffectsScale() const {
  return !initial_value_.IsIdentityOrTranslation() ||
         !target_value_.IsIdentityOrTranslation();
}

bool TransformAnimationCurveAdapter::IsTranslation() const {
  return initial_value_.IsIdentityOrTranslation() &&
         target_value_.IsIdentityOrTranslation();
}

bool TransformAnimationCurveAdapter::PreservesAxisAlignment() const {
  return (initial_value_.IsIdentity() ||
          initial_value_.IsScaleOrTranslation()) &&
         (target_value_.IsIdentity() || target_value_.IsScaleOrTranslation());
}

bool TransformAnimationCurveAdapter::AnimationStartScale(
    bool forward_direction,
    float* start_scale) const {
  return false;
}

bool TransformAnimationCurveAdapter::MaximumTargetScale(
    bool forward_direction,
    float* max_scale) const {
  return false;
}

InverseTransformCurveAdapter::InverseTransformCurveAdapter(
    TransformAnimationCurveAdapter base_curve,
    gfx::Transform initial_value,
    base::TimeDelta duration)
    : base_curve_(base_curve),
      initial_value_(initial_value),
      duration_(duration) {
  effective_initial_value_ =
      base_curve_.GetValue(base::TimeDelta()) * initial_value_;
}

InverseTransformCurveAdapter::~InverseTransformCurveAdapter() {
}

base::TimeDelta InverseTransformCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<cc::AnimationCurve> InverseTransformCurveAdapter::Clone()
    const {
  return base::WrapUnique(
      new InverseTransformCurveAdapter(base_curve_, initial_value_, duration_));
}

gfx::Transform InverseTransformCurveAdapter::GetValue(base::TimeDelta t) const {
  if (t <= base::TimeDelta())
    return initial_value_;

  gfx::Transform base_transform = base_curve_.GetValue(t);
  // Invert base
  gfx::Transform to_return(gfx::Transform::kSkipInitialization);
  bool is_invertible = base_transform.GetInverse(&to_return);
  DCHECK(is_invertible);

  to_return.PreconcatTransform(effective_initial_value_);
  return to_return;
}

bool InverseTransformCurveAdapter::AnimatedBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  // TODO(ajuma): Once cc::TransformOperation::BlendedBoundsForBox supports
  // computing bounds for TransformOperationMatrix, use that to compute
  // the bounds we need here.
  return false;
}

bool InverseTransformCurveAdapter::AffectsScale() const {
  return !initial_value_.IsIdentityOrTranslation() ||
         base_curve_.AffectsScale();
}

bool InverseTransformCurveAdapter::IsTranslation() const {
  return initial_value_.IsIdentityOrTranslation() &&
         base_curve_.IsTranslation();
}

bool InverseTransformCurveAdapter::PreservesAxisAlignment() const {
  return (initial_value_.IsIdentity() ||
          initial_value_.IsScaleOrTranslation()) &&
         (base_curve_.PreservesAxisAlignment());
}

bool InverseTransformCurveAdapter::AnimationStartScale(
    bool forward_direction,
    float* start_scale) const {
  return false;
}

bool InverseTransformCurveAdapter::MaximumTargetScale(bool forward_direction,
                                                      float* max_scale) const {
  return false;
}

}  // namespace ui
