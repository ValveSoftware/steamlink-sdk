// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformKeyframe.h"

#include <memory>

namespace blink {

CompositorTransformKeyframe::CompositorTransformKeyframe(
    double time,
    CompositorTransformOperations value,
    const TimingFunction& timingFunction)
    : m_transformKeyframe(
          cc::TransformKeyframe::Create(base::TimeDelta::FromSecondsD(time),
                                        value.releaseCcTransformOperations(),
                                        timingFunction.cloneToCC())) {}

CompositorTransformKeyframe::~CompositorTransformKeyframe() {}

double CompositorTransformKeyframe::time() const {
  return m_transformKeyframe->Time().InSecondsF();
}

const cc::TimingFunction* CompositorTransformKeyframe::ccTimingFunction()
    const {
  return m_transformKeyframe->timing_function();
}

std::unique_ptr<cc::TransformKeyframe> CompositorTransformKeyframe::cloneToCC()
    const {
  return m_transformKeyframe->Clone();
}

}  // namespace blink
