// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/animation/timing_function.h"

namespace cc {

TimingFunction::TimingFunction() {}

TimingFunction::~TimingFunction() {}

double TimingFunction::Duration() const {
  return 1.0;
}

scoped_ptr<CubicBezierTimingFunction> CubicBezierTimingFunction::Create(
    double x1, double y1, double x2, double y2) {
  return make_scoped_ptr(new CubicBezierTimingFunction(x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : bezier_(x1, y1, x2, y2) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() {}

float CubicBezierTimingFunction::GetValue(double x) const {
  return static_cast<float>(bezier_.Solve(x));
}

scoped_ptr<AnimationCurve> CubicBezierTimingFunction::Clone() const {
  return make_scoped_ptr(
      new CubicBezierTimingFunction(*this)).PassAs<AnimationCurve>();
}

void CubicBezierTimingFunction::Range(float* min, float* max) const {
  double min_d = 0;
  double max_d = 1;
  bezier_.Range(&min_d, &max_d);
  *min = static_cast<float>(min_d);
  *max = static_cast<float>(max_d);
}

// These numbers come from
// http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
scoped_ptr<TimingFunction> EaseTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.25, 0.1, 0.25, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.42, 0.0, 1.0, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseOutTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.0, 0.0, 0.58, 1.0).PassAs<TimingFunction>();
}

scoped_ptr<TimingFunction> EaseInOutTimingFunction::Create() {
  return CubicBezierTimingFunction::Create(
      0.42, 0.0, 0.58, 1).PassAs<TimingFunction>();
}

}  // namespace cc
