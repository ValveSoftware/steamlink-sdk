// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatAnimationCurve.h"

#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve()
    : m_curve(cc::KeyframedFloatAnimationCurve::Create()) {}

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve(
    std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve)
    : m_curve(std::move(curve)) {}

CompositorFloatAnimationCurve::~CompositorFloatAnimationCurve() {}

std::unique_ptr<CompositorFloatAnimationCurve>
CompositorFloatAnimationCurve::createForTesting(
    std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve) {
  return wrapUnique(new CompositorFloatAnimationCurve(std::move(curve)));
}

CompositorFloatAnimationCurve::Keyframes
CompositorFloatAnimationCurve::keyframesForTesting() const {
  Keyframes keyframes;
  for (const auto& ccKeyframe : m_curve->keyframes_for_testing())
    keyframes.append(
        wrapUnique(new CompositorFloatKeyframe(ccKeyframe->Clone())));
  return keyframes;
}

PassRefPtr<TimingFunction>
CompositorFloatAnimationCurve::getTimingFunctionForTesting() const {
  return createCompositorTimingFunctionFromCC(
      m_curve->timing_function_for_testing());
}

void CompositorFloatAnimationCurve::addKeyframe(
    const CompositorFloatKeyframe& keyframe) {
  m_curve->AddKeyframe(keyframe.cloneToCC());
}

void CompositorFloatAnimationCurve::setTimingFunction(
    const TimingFunction& timingFunction) {
  m_curve->SetTimingFunction(timingFunction.cloneToCC());
}

void CompositorFloatAnimationCurve::setScaledDuration(double scaledDuration) {
  m_curve->set_scaled_duration(scaledDuration);
}

float CompositorFloatAnimationCurve::getValue(double time) const {
  return m_curve->GetValue(base::TimeDelta::FromSecondsD(time));
}

std::unique_ptr<cc::AnimationCurve>
CompositorFloatAnimationCurve::cloneToAnimationCurve() const {
  return m_curve->Clone();
}

}  // namespace blink
