// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatKeyframe_h
#define CompositorFloatKeyframe_h

#include "cc/animation/keyframed_animation_curve.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorKeyframe.h"
#include "wtf/Noncopyable.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorFloatKeyframe : public CompositorKeyframe {
  WTF_MAKE_NONCOPYABLE(CompositorFloatKeyframe);

 public:
  CompositorFloatKeyframe(double time, float value, const TimingFunction&);
  CompositorFloatKeyframe(std::unique_ptr<cc::FloatKeyframe>);
  ~CompositorFloatKeyframe() override;

  // CompositorKeyframe implementation.
  double time() const override;
  const cc::TimingFunction* ccTimingFunction() const override;

  float value() { return m_floatKeyframe->Value(); }
  std::unique_ptr<cc::FloatKeyframe> cloneToCC() const;

 private:
  std::unique_ptr<cc::FloatKeyframe> m_floatKeyframe;
};

}  // namespace blink

#endif  // CompositorFloatKeyframe_h
