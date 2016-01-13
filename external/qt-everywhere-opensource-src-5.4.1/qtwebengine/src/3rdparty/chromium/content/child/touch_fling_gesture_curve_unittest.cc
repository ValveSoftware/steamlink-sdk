  // Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the TouchFlingGestureCurve.

#include "content/child/touch_fling_gesture_curve.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "third_party/WebKit/public/platform/WebSize.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureCurve;
using blink::WebGestureCurveTarget;
using blink::WebSize;

namespace {

class MockGestureCurveTarget : public WebGestureCurveTarget {
 public:
  virtual bool scrollBy(const WebFloatSize& delta,
                        const WebFloatSize& velocity) OVERRIDE {
    cumulative_delta_.width += delta.width;
    cumulative_delta_.height += delta.height;
    current_velocity_ = velocity;
    return true;
  }

  WebFloatSize cumulative_delta() const { return cumulative_delta_; }
  void resetCumulativeDelta() { cumulative_delta_ = WebFloatSize(); }

  WebFloatSize current_velocity() const { return current_velocity_; }

 private:
  WebFloatSize cumulative_delta_;
  WebFloatSize current_velocity_;
};

} // namespace anonymous

TEST(TouchFlingGestureCurve, flingCurveTouch)
{
  double initialVelocity = 5000;
  MockGestureCurveTarget target;

  scoped_ptr<WebGestureCurve> curve(content::TouchFlingGestureCurve::Create(
      WebFloatPoint(initialVelocity, 0),
      -5.70762e+03f, 1.72e+02f, 3.7e+00f, WebSize()));

  // Note: the expectations below are dependent on the curve parameters hard
  // coded into the create call above.
  EXPECT_TRUE(curve->apply(0, &target));
  EXPECT_TRUE(curve->apply(0.25, &target));
  EXPECT_NEAR(target.current_velocity().width, 1878, 1);
  EXPECT_EQ(target.current_velocity().height, 0);
  EXPECT_TRUE(curve->apply(0.45f, &target)); // Use non-uniform tick spacing.
  EXPECT_TRUE(curve->apply(1, &target));
  EXPECT_FALSE(curve->apply(1.5, &target));
  EXPECT_NEAR(target.cumulative_delta().width, 1193, 1);
  EXPECT_EQ(target.cumulative_delta().height, 0);
  EXPECT_EQ(target.current_velocity().width, 0);
  EXPECT_EQ(target.current_velocity().height, 0);
}

