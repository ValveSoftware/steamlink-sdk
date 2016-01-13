// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "cc/animation/timing_function.h"
#include "ui/gfx/animation/tween.h"

const double kDurationDivisor = 60.0;

namespace cc {

scoped_ptr<ScrollOffsetAnimationCurve> ScrollOffsetAnimationCurve::Create(
    const gfx::Vector2dF& target_value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new ScrollOffsetAnimationCurve(target_value, timing_function.Pass()));
}

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    const gfx::Vector2dF& target_value,
    scoped_ptr<TimingFunction> timing_function)
    : target_value_(target_value), timing_function_(timing_function.Pass()) {
}

ScrollOffsetAnimationCurve::~ScrollOffsetAnimationCurve() {}

void ScrollOffsetAnimationCurve::SetInitialValue(
    const gfx::Vector2dF& initial_value) {
  initial_value_ = initial_value;

  // The duration of a scroll animation depends on the size of the scroll.
  // The exact relationship between the size and the duration isn't specified
  // by the CSSOM View smooth scroll spec and is instead left up to user agents
  // to decide. The calculation performed here will very likely be further
  // tweaked before the smooth scroll API ships.
  float delta_x = std::abs(target_value_.x() - initial_value_.x());
  float delta_y = std::abs(target_value_.y() - initial_value_.y());
  float max_delta = std::max(delta_x, delta_y);
  duration_ = base::TimeDelta::FromMicroseconds(
      (std::sqrt(max_delta) / kDurationDivisor) *
      base::Time::kMicrosecondsPerSecond);
}

gfx::Vector2dF ScrollOffsetAnimationCurve::GetValue(double t) const {
  double duration = duration_.InSecondsF();

  if (t <= 0)
    return initial_value_;

  if (t >= duration)
    return target_value_;

  double progress = (timing_function_->GetValue(t / duration));
  return gfx::Vector2dF(gfx::Tween::FloatValueBetween(
                            progress, initial_value_.x(), target_value_.x()),
                        gfx::Tween::FloatValueBetween(
                            progress, initial_value_.y(), target_value_.y()));
}

double ScrollOffsetAnimationCurve::Duration() const {
  return duration_.InSecondsF();
}

AnimationCurve::CurveType ScrollOffsetAnimationCurve::Type() const {
  return ScrollOffset;
}

scoped_ptr<AnimationCurve> ScrollOffsetAnimationCurve::Clone() const {
  scoped_ptr<TimingFunction> timing_function(
      static_cast<TimingFunction*>(timing_function_->Clone().release()));
  scoped_ptr<ScrollOffsetAnimationCurve> curve_clone =
      Create(target_value_, timing_function.Pass());
  curve_clone->initial_value_ = initial_value_;
  curve_clone->duration_ = duration_;
  return curve_clone.PassAs<AnimationCurve>();
}

}  // namespace cc
