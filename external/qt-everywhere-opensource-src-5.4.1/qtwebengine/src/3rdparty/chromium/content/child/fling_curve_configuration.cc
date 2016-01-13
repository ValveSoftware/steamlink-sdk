// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fling_curve_configuration.h"

#include "base/logging.h"
#include "content/child/touch_fling_gesture_curve.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"

namespace content {

FlingCurveConfiguration::FlingCurveConfiguration() { }

FlingCurveConfiguration::~FlingCurveConfiguration() { }

void FlingCurveConfiguration::SetCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  DCHECK(new_touchpad.size() >= 3);
  DCHECK(new_touchscreen.size() >= 3);
  base::AutoLock scoped_lock(lock_);
  touchpad_coefs_ = new_touchpad;
  touchscreen_coefs_ = new_touchscreen;
}

blink::WebGestureCurve* FlingCurveConfiguration::CreateCore(
    const std::vector<float>& coefs,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulativeScroll) {
  float p0, p1, p2;

  {
    base::AutoLock scoped_lock(lock_);
    p0 = coefs[0];
    p1 = coefs[1];
    p2 = coefs[2];
  }

  return TouchFlingGestureCurve::Create(velocity, p0, p1, p2,
      cumulativeScroll);
}

blink::WebGestureCurve* FlingCurveConfiguration::CreateForTouchPad(
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulativeScroll) {
  return CreateCore(touchpad_coefs_, velocity, cumulativeScroll);
}

blink::WebGestureCurve* FlingCurveConfiguration::CreateForTouchScreen(
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulativeScroll) {
  return CreateCore(touchscreen_coefs_, velocity, cumulativeScroll);
}

} // namespace content
