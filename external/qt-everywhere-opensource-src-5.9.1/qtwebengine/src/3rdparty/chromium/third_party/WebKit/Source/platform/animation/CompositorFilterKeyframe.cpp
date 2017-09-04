// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterKeyframe.h"

#include "platform/animation/TimingFunction.h"
#include <memory>

namespace blink {

CompositorFilterKeyframe::CompositorFilterKeyframe(
    double time,
    CompositorFilterOperations value,
    const TimingFunction& timingFunction)
    : m_filterKeyframe(
          cc::FilterKeyframe::Create(base::TimeDelta::FromSecondsD(time),
                                     value.releaseCcFilterOperations(),
                                     timingFunction.cloneToCC())) {}

CompositorFilterKeyframe::~CompositorFilterKeyframe() {}

double CompositorFilterKeyframe::time() const {
  return m_filterKeyframe->Time().InSecondsF();
}

const cc::TimingFunction* CompositorFilterKeyframe::ccTimingFunction() const {
  return m_filterKeyframe->timing_function();
}

std::unique_ptr<cc::FilterKeyframe> CompositorFilterKeyframe::cloneToCC()
    const {
  return m_filterKeyframe->Clone();
}

}  // namespace blink
