// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterAnimationCurve_h
#define CompositorFilterAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace cc {
class KeyframedFilterAnimationCurve;
}

namespace blink {
class CompositorFilterKeyframe;
}

namespace blink {

// A keyframed filter animation curve.
class PLATFORM_EXPORT CompositorFilterAnimationCurve
    : public CompositorAnimationCurve {
  WTF_MAKE_NONCOPYABLE(CompositorFilterAnimationCurve);

 public:
  static std::unique_ptr<CompositorFilterAnimationCurve> create() {
    return wrapUnique(new CompositorFilterAnimationCurve());
  }
  ~CompositorFilterAnimationCurve() override;

  void addKeyframe(const CompositorFilterKeyframe&);
  void setTimingFunction(const TimingFunction&);
  void setScaledDuration(double);

  // blink::CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

 private:
  CompositorFilterAnimationCurve();

  std::unique_ptr<cc::KeyframedFilterAnimationCurve> m_curve;
};

}  // namespace blink

#endif  // CompositorFilterAnimationCurve_h
