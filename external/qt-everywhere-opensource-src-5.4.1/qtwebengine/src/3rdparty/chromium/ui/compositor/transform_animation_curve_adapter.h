// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_
#define UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_

#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace ui {

class COMPOSITOR_EXPORT TransformAnimationCurveAdapter
    : public cc::TransformAnimationCurve {
 public:
  TransformAnimationCurveAdapter(gfx::Tween::Type tween_type,
                                 gfx::Transform intial_value,
                                 gfx::Transform target_value,
                                 base::TimeDelta duration);

  virtual ~TransformAnimationCurveAdapter();

  // TransformAnimationCurve implementation.
  virtual double Duration() const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;
  virtual gfx::Transform GetValue(double t) const OVERRIDE;
  virtual bool AnimatedBoundsForBox(const gfx::BoxF& box,
                                    gfx::BoxF* bounds) const OVERRIDE;
  virtual bool AffectsScale() const OVERRIDE;
  virtual bool IsTranslation() const OVERRIDE;
  virtual bool MaximumScale(float* max_scale) const OVERRIDE;

 private:
  gfx::Tween::Type tween_type_;
  gfx::Transform initial_value_;
  gfx::Transform target_value_;
  gfx::DecomposedTransform decomposed_initial_value_;
  gfx::DecomposedTransform decomposed_target_value_;
  base::TimeDelta duration_;

  DISALLOW_ASSIGN(TransformAnimationCurveAdapter);
};

class COMPOSITOR_EXPORT InverseTransformCurveAdapter
    : public cc::TransformAnimationCurve {
 public:
  InverseTransformCurveAdapter(TransformAnimationCurveAdapter base_curve,
                               gfx::Transform initial_value,
                               base::TimeDelta duration);

  virtual ~InverseTransformCurveAdapter();

  virtual double Duration() const OVERRIDE;
  virtual scoped_ptr<AnimationCurve> Clone() const OVERRIDE;
  virtual gfx::Transform GetValue(double t) const OVERRIDE;
  virtual bool AnimatedBoundsForBox(const gfx::BoxF& box,
                                    gfx::BoxF* bounds) const OVERRIDE;
  virtual bool AffectsScale() const OVERRIDE;
  virtual bool IsTranslation() const OVERRIDE;
  virtual bool MaximumScale(float* max_scale) const OVERRIDE;

 private:
  TransformAnimationCurveAdapter base_curve_;
  gfx::Transform initial_value_;
  gfx::Transform effective_initial_value_;
  base::TimeDelta duration_;

  DISALLOW_ASSIGN(InverseTransformCurveAdapter);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TRANSFORM_ANIMATION_CURVE_ADAPTER_H_

