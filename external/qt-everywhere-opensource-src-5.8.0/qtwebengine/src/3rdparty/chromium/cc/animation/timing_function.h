// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TIMING_FUNCTION_H_
#define CC_ANIMATION_TIMING_FUNCTION_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace cc {

// See http://www.w3.org/TR/css3-transitions/.
class CC_EXPORT TimingFunction {
 public:
  virtual ~TimingFunction();

  // Note that LINEAR is a nullptr TimingFunction (for now).
  enum class Type { LINEAR, CUBIC_BEZIER, STEPS };

  virtual Type GetType() const = 0;
  virtual float GetValue(double t) const = 0;
  virtual float Velocity(double time) const = 0;
  // The smallest and largest values returned by GetValue for inputs in [0, 1].
  virtual void Range(float* min, float* max) const = 0;
  virtual std::unique_ptr<TimingFunction> Clone() const = 0;

 protected:
  TimingFunction();

 private:
  DISALLOW_ASSIGN(TimingFunction);
};

class CC_EXPORT CubicBezierTimingFunction : public TimingFunction {
 public:
  enum class EaseType { EASE, EASE_IN, EASE_OUT, EASE_IN_OUT, EASE_OUT_NATURAL, CUSTOM };

  static std::unique_ptr<TimingFunction> CreatePreset(EaseType ease_type);
  static std::unique_ptr<CubicBezierTimingFunction> Create(double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2);
  ~CubicBezierTimingFunction() override;

  // TimingFunction implementation.
  Type GetType() const override;
  float GetValue(double time) const override;
  float Velocity(double time) const override;
  void Range(float* min, float* max) const override;
  std::unique_ptr<TimingFunction> Clone() const override;

  EaseType ease_type() const { return ease_type_; }

 protected:
  CubicBezierTimingFunction(EaseType ease_type,
                            double x1,
                            double y1,
                            double x2,
                            double y2);

  gfx::CubicBezier bezier_;
  EaseType ease_type_;

 private:
  DISALLOW_ASSIGN(CubicBezierTimingFunction);
};

class CC_EXPORT StepsTimingFunction : public TimingFunction {
 public:
  // Web Animations specification, 3.12.4. Timing in discrete steps.
  enum class StepPosition { START, MIDDLE, END };

  static std::unique_ptr<StepsTimingFunction> Create(
      int steps,
      StepPosition step_position);
  ~StepsTimingFunction() override;

  // TimingFunction implementation.
  Type GetType() const override;
  float GetValue(double t) const override;
  std::unique_ptr<TimingFunction> Clone() const override;
  void Range(float* min, float* max) const override;
  float Velocity(double time) const override;

 protected:
  StepsTimingFunction(int steps, StepPosition step_position);

 private:
  int steps_;
  float steps_start_offset_;

  DISALLOW_ASSIGN(StepsTimingFunction);
};

}  // namespace cc

#endif  // CC_ANIMATION_TIMING_FUNCTION_H_
