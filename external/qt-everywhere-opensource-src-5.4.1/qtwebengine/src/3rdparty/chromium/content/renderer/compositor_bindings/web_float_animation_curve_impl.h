// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebFloatAnimationCurve.h"

namespace cc {
class AnimationCurve;
class KeyframedFloatAnimationCurve;
}

namespace blink {
struct WebFloatKeyframe;
}

namespace content {

class WebFloatAnimationCurveImpl : public blink::WebFloatAnimationCurve {
 public:
  CONTENT_EXPORT WebFloatAnimationCurveImpl();
  virtual ~WebFloatAnimationCurveImpl();

  // WebAnimationCurve implementation.
  virtual AnimationCurveType type() const;

  // WebFloatAnimationCurve implementation.
  virtual void add(const blink::WebFloatKeyframe& keyframe);
  virtual void add(const blink::WebFloatKeyframe& keyframe,
                   TimingFunctionType type);
  virtual void add(const blink::WebFloatKeyframe& keyframe,
                   double x1,
                   double y1,
                   double x2,
                   double y2);

  virtual float getValue(double time) const;

  scoped_ptr<cc::AnimationCurve> CloneToAnimationCurve() const;

 private:
  scoped_ptr<cc::KeyframedFloatAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(WebFloatAnimationCurveImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_FLOAT_ANIMATION_CURVE_IMPL_H_

