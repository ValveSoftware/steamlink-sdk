// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "platform/animation/CompositorTransformOperations.h"

namespace blink {

CompositorTransformAnimationCurve::CompositorTransformAnimationCurve()
    : m_curve(cc::KeyframedTransformAnimationCurve::Create()) {}

CompositorTransformAnimationCurve::~CompositorTransformAnimationCurve() {}

void CompositorTransformAnimationCurve::addKeyframe(
    const CompositorTransformKeyframe& keyframe) {
  m_curve->AddKeyframe(keyframe.cloneToCC());
}

void CompositorTransformAnimationCurve::setTimingFunction(
    const TimingFunction& timingFunction) {
  m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

void CompositorTransformAnimationCurve::setScaledDuration(
    double scaledDuration) {
  m_curve->set_scaled_duration(scaledDuration);
}

std::unique_ptr<cc::AnimationCurve>
CompositorTransformAnimationCurve::cloneToAnimationCurve() const {
  return m_curve->Clone();
}

}  // namespace blink
