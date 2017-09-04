// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatKeyframe.h"

#include "platform/animation/TimingFunction.h"

namespace blink {

CompositorFloatKeyframe::CompositorFloatKeyframe(
    double time,
    float value,
    const TimingFunction& timingFunction)
    : m_floatKeyframe(
          cc::FloatKeyframe::Create(base::TimeDelta::FromSecondsD(time),
                                    value,
                                    timingFunction.cloneToCC())) {}

CompositorFloatKeyframe::CompositorFloatKeyframe(
    std::unique_ptr<cc::FloatKeyframe> floatKeyframe)
    : m_floatKeyframe(std::move(floatKeyframe)) {}

CompositorFloatKeyframe::~CompositorFloatKeyframe() {}

double CompositorFloatKeyframe::time() const {
  return m_floatKeyframe->Time().InSecondsF();
}

const cc::TimingFunction* CompositorFloatKeyframe::ccTimingFunction() const {
  return m_floatKeyframe->timing_function();
}

std::unique_ptr<cc::FloatKeyframe> CompositorFloatKeyframe::cloneToCC() const {
  return m_floatKeyframe->Clone();
}

}  // namespace blink
