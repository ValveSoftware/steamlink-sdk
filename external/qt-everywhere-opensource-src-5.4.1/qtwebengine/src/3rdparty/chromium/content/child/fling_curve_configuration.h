// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FLING_CURVE_CONFIGURATION_H_
#define CONTENT_CHILD_FLING_CURVE_CONFIGURATION_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebSize.h"

namespace blink {
class WebGestureCurve;
}

namespace content {

// A class to manage dynamically adjustable parameters controlling the
// shape of the fling deacceleration function.
class FlingCurveConfiguration {
 public:
  FlingCurveConfiguration();
  virtual ~FlingCurveConfiguration();

  // Create a touchpad fling curve using the current parameters.
  blink::WebGestureCurve* CreateForTouchPad(
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulativeScroll);

  // Create a touchscreen fling curve using the current parameters.
  blink::WebGestureCurve* CreateForTouchScreen(
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulativeScroll);

  // Set the curve parameters.
  void SetCurveParameters(
      const std::vector<float>& new_touchpad,
      const std::vector<float>& new_touchscreen);

 private:
  blink::WebGestureCurve* CreateCore(
    const std::vector<float>& coefs,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulativeScroll);

  // Protect access to touchpad_coefs_ and touchscreen_coefs_.
  base::Lock lock_;
  std::vector<float> touchpad_coefs_;
  std::vector<float> touchscreen_coefs_;

  DISALLOW_COPY_AND_ASSIGN(FlingCurveConfiguration);
};

} // namespace content

#endif // CONTENT_CHILD_FLING_CURVE_CONFIGURATION_H_
