// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationDelegate_h
#define CompositorAnimationDelegate_h

#include "cc/animation/animation_curve.h"
#include "platform/PlatformExport.h"

#include <memory>

namespace blink {

class PLATFORM_EXPORT CompositorAnimationDelegate {
 public:
  virtual ~CompositorAnimationDelegate() {}

  virtual void notifyAnimationStarted(double monotonicTime, int group) = 0;
  virtual void notifyAnimationFinished(double monotonicTime, int group) = 0;
  virtual void notifyAnimationAborted(double monotonicTime, int group) = 0;
  // In the current state of things, notifyAnimationTakeover only applies to
  // scroll offset animations since main thread scrolling reasons can be added
  // while the compositor is animating. Keeping this non-pure virtual since
  // it doesn't apply to CSS animations.
  virtual void notifyAnimationTakeover(
      double monotonicTime,
      double animationStartTime,
      std::unique_ptr<cc::AnimationCurve> curve) {}
};

}  // namespace blink

#endif  // CompositorAnimationDelegate_h
