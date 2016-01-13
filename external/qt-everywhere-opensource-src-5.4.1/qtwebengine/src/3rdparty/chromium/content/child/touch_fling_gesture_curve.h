// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_TOUCH_FLING_GESTURE_CURVE_H_
#define CONTENT_CHILD_TOUCH_FLING_GESTURE_CURVE_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebSize.h"

namespace blink {
class WebGestureCurveTarget;
}

namespace content {

// Implementation of WebGestureCurve suitable for touch pad/screen-based
// fling scroll. Starts with a flat velocity profile based on 'velocity', which
// tails off to zero. Time is scaled to that duration of the fling is
// proportional to the initial velocity.
class TouchFlingGestureCurve : public blink::WebGestureCurve {
 public:

  static CONTENT_EXPORT WebGestureCurve* Create(
      const blink::WebFloatPoint& initial_velocity,
      float p0, float p1, float p2,
      const blink::WebSize& cumulativeScroll);

 virtual bool apply(double monotonicTime,
                    blink::WebGestureCurveTarget*) OVERRIDE;

 private:
  TouchFlingGestureCurve(const blink::WebFloatPoint& initial_velocity,
                         float p0,
                         float p1,
                         float p2,
                         const blink::WebSize& cumulativeScroll);
  virtual ~TouchFlingGestureCurve();

  blink::WebFloatPoint displacement_ratio_;
  blink::WebFloatSize cumulative_scroll_;
  float coefficients_[3];
  float time_offset_;
  float curve_duration_;
  float position_offset_;

  DISALLOW_COPY_AND_ASSIGN(TouchFlingGestureCurve);
};

} // namespace content

#endif // CONTENT_CHILD_TOUCH_FLING_GESTURE_CURVE_H_
