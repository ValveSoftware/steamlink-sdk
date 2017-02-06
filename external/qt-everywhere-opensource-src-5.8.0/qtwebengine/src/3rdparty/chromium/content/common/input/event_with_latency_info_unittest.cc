// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/event_with_latency_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {
namespace {

class EventWithLatencyInfoTest : public testing::Test {
 protected:
  TouchEventWithLatencyInfo CreateTouchEvent(WebInputEvent::Type type,
                                             double timestamp) {
    TouchEventWithLatencyInfo touch;
    touch.event.touchesLength = 1;
    touch.event.type = type;
    touch.event.timeStampSeconds = timestamp;
    return touch;
  }

  MouseEventWithLatencyInfo CreateMouseEvent(WebInputEvent::Type type,
                                             double timestamp) {
    MouseEventWithLatencyInfo mouse;
    mouse.event.type = type;
    mouse.event.timeStampSeconds = timestamp;
    return mouse;
  }

  MouseWheelEventWithLatencyInfo CreateMouseWheelEvent(double timestamp) {
    MouseWheelEventWithLatencyInfo mouse_wheel;
    mouse_wheel.event.type = WebInputEvent::MouseWheel;
    mouse_wheel.event.timeStampSeconds = timestamp;
    return mouse_wheel;
  }

  GestureEventWithLatencyInfo CreateGestureEvent(WebInputEvent::Type type,
                                                 double timestamp) {
    GestureEventWithLatencyInfo gesture;
    gesture.event.type = type;
    gesture.event.timeStampSeconds = timestamp;
    return gesture;
  }
};

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForMouseEvent) {
  MouseEventWithLatencyInfo mouse_0 = CreateMouseEvent(
      WebInputEvent::MouseMove, 5.0);
  MouseEventWithLatencyInfo mouse_1 = CreateMouseEvent(
      WebInputEvent::MouseMove, 10.0);

  ASSERT_TRUE(mouse_0.CanCoalesceWith(mouse_1));
  mouse_0.CoalesceWith(mouse_1);
  // Coalescing WebMouseEvent preserves newer timestamp.
  EXPECT_EQ(10.0, mouse_0.event.timeStampSeconds);
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForMouseWheelEvent) {
  MouseWheelEventWithLatencyInfo mouse_wheel_0 = CreateMouseWheelEvent(5.0);
  MouseWheelEventWithLatencyInfo mouse_wheel_1 = CreateMouseWheelEvent(10.0);

  ASSERT_TRUE(mouse_wheel_0.CanCoalesceWith(mouse_wheel_1));
  mouse_wheel_0.CoalesceWith(mouse_wheel_1);
  // Coalescing WebMouseWheelEvent preserves newer timestamp.
  EXPECT_EQ(10.0, mouse_wheel_0.event.timeStampSeconds);
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForTouchEvent) {
  TouchEventWithLatencyInfo touch_0 = CreateTouchEvent(
      WebInputEvent::TouchMove, 5.0);
  TouchEventWithLatencyInfo touch_1 = CreateTouchEvent(
      WebInputEvent::TouchMove, 10.0);

  ASSERT_TRUE(touch_0.CanCoalesceWith(touch_1));
  touch_0.CoalesceWith(touch_1);
  // Coalescing WebTouchEvent preserves newer timestamp.
  EXPECT_EQ(10.0, touch_0.event.timeStampSeconds);
}

TEST_F(EventWithLatencyInfoTest, TimestampCoalescingForGestureEvent) {
  GestureEventWithLatencyInfo scroll_0 = CreateGestureEvent(
      WebInputEvent::GestureScrollUpdate, 5.0);
  GestureEventWithLatencyInfo scroll_1 = CreateGestureEvent(
      WebInputEvent::GestureScrollUpdate, 10.0);

  ASSERT_TRUE(scroll_0.CanCoalesceWith(scroll_1));
  scroll_0.CoalesceWith(scroll_1);
  // Coalescing WebGestureEvent preserves newer timestamp.
  EXPECT_EQ(10.0, scroll_0.event.timeStampSeconds);
}

TEST_F(EventWithLatencyInfoTest, LatencyInfoCoalescing) {
    MouseEventWithLatencyInfo mouse_0 = CreateMouseEvent(
      WebInputEvent::MouseMove, 5.0);
  mouse_0.latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, 0, base::TimeTicks(), 1);
  MouseEventWithLatencyInfo mouse_1 = CreateMouseEvent(
      WebInputEvent::MouseMove, 10.0);

  ASSERT_TRUE(mouse_0.CanCoalesceWith(mouse_1));

  ui::LatencyInfo::LatencyComponent component;
  EXPECT_FALSE(mouse_1.latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, &component));

  mouse_0.CoalesceWith(mouse_1);

  // Coalescing WebMouseEvent preservers older LatencyInfo.
  EXPECT_TRUE(mouse_1.latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, &component));
}

}  // namespace
}  // namespace content
