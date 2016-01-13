// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/float_animation_curve_adapter.h"

namespace ui {

FloatAnimationCurveAdapter::FloatAnimationCurveAdapter(
    gfx::Tween::Type tween_type,
    float initial_value,
    float target_value,
    base::TimeDelta duration)
    : tween_type_(tween_type),
      initial_value_(initial_value),
      target_value_(target_value),
      duration_(duration) {
}

double FloatAnimationCurveAdapter::Duration() const {
  return duration_.InSecondsF();
}

scoped_ptr<cc::AnimationCurve> FloatAnimationCurveAdapter::Clone() const {
  scoped_ptr<FloatAnimationCurveAdapter> to_return(
      new FloatAnimationCurveAdapter(tween_type_,
                                     initial_value_,
                                     target_value_,
                                     duration_));
  return to_return.PassAs<cc::AnimationCurve>();
}

float FloatAnimationCurveAdapter::GetValue(double t) const {
  if (t >= duration_.InSecondsF())
    return target_value_;
  if (t <= 0.0)
    return initial_value_;
  double progress = t / duration_.InSecondsF();
  return gfx::Tween::FloatValueBetween(
      gfx::Tween::CalculateValue(tween_type_, progress),
      initial_value_,
      target_value_);
}

}  // namespace ui
