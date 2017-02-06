// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/timing_function.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"

namespace cc {

TimingFunction::TimingFunction() {}

TimingFunction::~TimingFunction() {}

std::unique_ptr<TimingFunction> CubicBezierTimingFunction::CreatePreset(
    EaseType ease_type) {
  // These numbers come from
  // http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
  switch (ease_type) {
    case EaseType::EASE:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.25, 0.1, 0.25, 1.0));
    case EaseType::EASE_IN:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 1.0, 1.0));
    case EaseType::EASE_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.0, 0.0, 0.58, 1.0));
    case EaseType::EASE_IN_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 0.58, 1));
    case EaseType::EASE_OUT_NATURAL:
      // Try to hit natural quadratic ease-out
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.26, 0.46, 0.45, 0.94));
    default:
      NOTREACHED();
      return nullptr;
  }
}
std::unique_ptr<CubicBezierTimingFunction>
CubicBezierTimingFunction::Create(double x1, double y1, double x2, double y2) {
  return base::WrapUnique(
      new CubicBezierTimingFunction(EaseType::CUSTOM, x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(EaseType ease_type,
                                                     double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : bezier_(x1, y1, x2, y2), ease_type_(ease_type) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() {}

TimingFunction::Type CubicBezierTimingFunction::GetType() const {
  return Type::CUBIC_BEZIER;
}

float CubicBezierTimingFunction::GetValue(double x) const {
  return static_cast<float>(bezier_.Solve(x));
}

float CubicBezierTimingFunction::Velocity(double x) const {
  return static_cast<float>(bezier_.Slope(x));
}

void CubicBezierTimingFunction::Range(float* min, float* max) const {
  *min = static_cast<float>(bezier_.range_min());
  *max = static_cast<float>(bezier_.range_max());
}

std::unique_ptr<TimingFunction> CubicBezierTimingFunction::Clone() const {
  return base::WrapUnique(new CubicBezierTimingFunction(*this));
}


std::unique_ptr<StepsTimingFunction> StepsTimingFunction::Create(
    int steps,
    StepPosition step_position) {
  return base::WrapUnique(new StepsTimingFunction(steps, step_position));
}

StepsTimingFunction::StepsTimingFunction(int steps, StepPosition step_position)
    : steps_(steps) {
  switch (step_position) {
    case StepPosition::START:
      steps_start_offset_ = 1;
      break;
    case StepPosition::MIDDLE:
      steps_start_offset_ = 0.5;
      break;
    case StepPosition::END:
      steps_start_offset_ = 0;
      break;
  }
}

StepsTimingFunction::~StepsTimingFunction() {
}

TimingFunction::Type StepsTimingFunction::GetType() const {
  return Type::STEPS;
}

float StepsTimingFunction::GetValue(double t) const {
  const double steps = static_cast<double>(steps_);
  const double value = MathUtil::ClampToRange(
      std::floor((steps * t) + steps_start_offset_) / steps, 0.0, 1.0);
  return static_cast<float>(value);
}

std::unique_ptr<TimingFunction> StepsTimingFunction::Clone() const {
  return base::WrapUnique(new StepsTimingFunction(*this));
}

void StepsTimingFunction::Range(float* min, float* max) const {
  *min = 0.0f;
  *max = 1.0f;
}

float StepsTimingFunction::Velocity(double x) const {
  return 0.0f;
}

}  // namespace cc
