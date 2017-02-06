// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class TimingFunction;

class CC_EXPORT ScrollOffsetAnimationCurve : public AnimationCurve {
 public:
  enum class DurationBehavior { DELTA_BASED, CONSTANT, INVERSE_DELTA };
  static std::unique_ptr<ScrollOffsetAnimationCurve> Create(
      const gfx::ScrollOffset& target_value,
      std::unique_ptr<TimingFunction> timing_function,
      DurationBehavior = DurationBehavior::DELTA_BASED);

  ~ScrollOffsetAnimationCurve() override;

  void SetInitialValue(const gfx::ScrollOffset& initial_value);
  bool HasSetInitialValue() const;
  gfx::ScrollOffset GetValue(base::TimeDelta t) const;
  gfx::ScrollOffset target_value() const { return target_value_; }
  void UpdateTarget(double t, const gfx::ScrollOffset& new_target);
  void ApplyAdjustment(const gfx::Vector2dF& adjustment);

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  CurveType Type() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  std::unique_ptr<ScrollOffsetAnimationCurve>
  CloneToScrollOffsetAnimationCurve() const;

 private:
  ScrollOffsetAnimationCurve(const gfx::ScrollOffset& target_value,
                             std::unique_ptr<TimingFunction> timing_function,
                             DurationBehavior);

  gfx::ScrollOffset initial_value_;
  gfx::ScrollOffset target_value_;
  base::TimeDelta total_animation_duration_;

  // Time from animation start to most recent UpdateTarget.
  base::TimeDelta last_retarget_;

  std::unique_ptr<TimingFunction> timing_function_;
  DurationBehavior duration_behavior_;

  bool has_set_initial_value_;

  DISALLOW_COPY_AND_ASSIGN(ScrollOffsetAnimationCurve);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
