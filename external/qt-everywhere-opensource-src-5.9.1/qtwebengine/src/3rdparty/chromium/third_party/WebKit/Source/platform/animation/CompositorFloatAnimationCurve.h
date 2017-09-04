// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatAnimationCurve_h
#define CompositorFloatAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace cc {
class KeyframedFloatAnimationCurve;
}

namespace blink {
class CompositorFloatKeyframe;
}

namespace blink {

// A keyframed float animation curve.
class PLATFORM_EXPORT CompositorFloatAnimationCurve
    : public CompositorAnimationCurve {
  WTF_MAKE_NONCOPYABLE(CompositorFloatAnimationCurve);

 public:
  static std::unique_ptr<CompositorFloatAnimationCurve> create() {
    return wrapUnique(new CompositorFloatAnimationCurve());
  }

  ~CompositorFloatAnimationCurve() override;

  void addKeyframe(const CompositorFloatKeyframe&);
  void setTimingFunction(const TimingFunction&);
  void setScaledDuration(double);
  float getValue(double time) const;

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

  static std::unique_ptr<CompositorFloatAnimationCurve> createForTesting(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  using Keyframes = Vector<std::unique_ptr<CompositorFloatKeyframe>>;
  Keyframes keyframesForTesting() const;

  PassRefPtr<TimingFunction> getTimingFunctionForTesting() const;

 private:
  CompositorFloatAnimationCurve();
  CompositorFloatAnimationCurve(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  std::unique_ptr<cc::KeyframedFloatAnimationCurve> m_curve;
};

}  // namespace blink

#endif  // CompositorFloatAnimationCurve_h
