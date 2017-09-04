// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/output/filter_operations.h"
#include "platform/graphics/CompositorFilterOperations.h"

namespace blink {

CompositorFilterAnimationCurve::CompositorFilterAnimationCurve()
    : m_curve(cc::KeyframedFilterAnimationCurve::Create()) {}

CompositorFilterAnimationCurve::~CompositorFilterAnimationCurve() {}

void CompositorFilterAnimationCurve::addKeyframe(
    const CompositorFilterKeyframe& keyframe) {
  m_curve->AddKeyframe(keyframe.cloneToCC());
}

void CompositorFilterAnimationCurve::setTimingFunction(
    const TimingFunction& timingFunction) {
  m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

void CompositorFilterAnimationCurve::setScaledDuration(double scaledDuration) {
  m_curve->set_scaled_duration(scaledDuration);
}

std::unique_ptr<cc::AnimationCurve>
CompositorFilterAnimationCurve::cloneToAnimationCurve() const {
  return m_curve->Clone();
}

}  // namespace blink
