// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_input_event_traits.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace content {
namespace {

class WebInputEventTraitsTest : public testing::Test {
 protected:
  static WebTouchPoint CreateTouchPoint(WebTouchPoint::State state, int id) {
    WebTouchPoint touch;
    touch.state = state;
    touch.id = id;
    return touch;
  }

  static WebTouchEvent CreateTouch(WebInputEvent::Type type) {
    return CreateTouch(type, 1);
  }

  static WebTouchEvent CreateTouch(WebInputEvent::Type type,
                                   unsigned touch_count) {
    WebTouchEvent event;
    event.touchesLength = touch_count;
    event.type = type;
    return event;
  }

  static WebGestureEvent CreateGesture(WebInputEvent::Type type,
                                       float x,
                                       float y) {
    WebGestureEvent event;
    event.type = type;
    event.x = x;
    event.y = y;
    return event;
  }
};

TEST_F(WebInputEventTraitsTest, TouchEventCoalescing) {
  WebTouchEvent touch0 = CreateTouch(WebInputEvent::TouchStart);
  WebTouchEvent touch1 = CreateTouch(WebInputEvent::TouchMove);

  // Non touch-moves won't coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch0));

  // Touches of different types won't coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with idential touch lengths and touch ids should coalesce.
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(touch1, touch1));

  // Touch moves with different touch ids should not coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove);
  touch1 = CreateTouch(WebInputEvent::TouchMove);
  touch0.touches[0].id = 7;
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.touches[0].id = 1;
  touch1.touches[0].id = 0;
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with different touch lengths should not coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 1);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Touch moves with identical touch ids in different orders should coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = touch1.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch0.touches[1] = touch1.touches[0] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(touch0, touch1));

  // Pointers with the same ID's should coalesce.
  touch0 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = touch1.touches[1] =
      CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  WebInputEventTraits::Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.touches[0].id);
  ASSERT_EQ(0, touch1.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateUndefined, touch1.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[0].state);

  // Movement from now-stationary pointers should be preserved.
  touch0 = touch1 = CreateTouch(WebInputEvent::TouchMove, 2);
  touch0.touches[0] = CreateTouchPoint(WebTouchPoint::StateMoved, 1);
  touch1.touches[1] = CreateTouchPoint(WebTouchPoint::StateStationary, 1);
  touch0.touches[1] = CreateTouchPoint(WebTouchPoint::StateStationary, 0);
  touch1.touches[0] = CreateTouchPoint(WebTouchPoint::StateMoved, 0);
  WebInputEventTraits::Coalesce(touch0, &touch1);
  ASSERT_EQ(1, touch1.touches[0].id);
  ASSERT_EQ(0, touch1.touches[1].id);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, touch1.touches[1].state);
}

TEST_F(WebInputEventTraitsTest, PinchEventCoalescing) {
  WebGestureEvent pinch0 =
      CreateGesture(WebInputEvent::GesturePinchBegin, 1, 1);
  WebGestureEvent pinch1 =
      CreateGesture(WebInputEvent::GesturePinchUpdate, 2, 2);

  // Only GesturePinchUpdate's coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(pinch0, pinch0));

  // Pinch gestures of different types should not coalesce.
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(pinch0, pinch1));

  // Pinches with different focal points should not coalesce.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 2, 2);
  EXPECT_FALSE(WebInputEventTraits::CanCoalesce(pinch0, pinch1));
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(pinch0, pinch0));

  // Coalesced scales are multiplicative.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.data.pinchUpdate.scale = 2.f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.data.pinchUpdate.scale = 3.f;
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(pinch0, pinch0));
  WebInputEventTraits::Coalesce(pinch0, &pinch1);
  EXPECT_EQ(2.f * 3.f, pinch1.data.pinchUpdate.scale);

  // Scales have a minimum value and can never reach 0.
  ASSERT_GT(numeric_limits<float>::min(), 0);
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.data.pinchUpdate.scale = numeric_limits<float>::min() * 2.0f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.data.pinchUpdate.scale = numeric_limits<float>::min() * 5.0f;
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(pinch0, pinch1));
  WebInputEventTraits::Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::min(), pinch1.data.pinchUpdate.scale);

  // Scales have a maximum value and can never reach Infinity.
  pinch0 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch0.data.pinchUpdate.scale = numeric_limits<float>::max() / 2.0f;
  pinch1 = CreateGesture(WebInputEvent::GesturePinchUpdate, 1, 1);
  pinch1.data.pinchUpdate.scale = 10.0f;
  EXPECT_TRUE(WebInputEventTraits::CanCoalesce(pinch0, pinch1));
  WebInputEventTraits::Coalesce(pinch0, &pinch1);
  EXPECT_EQ(numeric_limits<float>::max(), pinch1.data.pinchUpdate.scale);
}

}  // namespace
}  // namespace content
