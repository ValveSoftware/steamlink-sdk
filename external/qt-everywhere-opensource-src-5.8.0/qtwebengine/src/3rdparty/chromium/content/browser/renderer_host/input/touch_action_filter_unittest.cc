// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {
namespace {

const blink::WebGestureDevice kSourceDevice =
    blink::WebGestureDeviceTouchscreen;

}  // namespace

static void PanTest(TouchAction action,
                    float scroll_x,
                    float scroll_y,
                    float dx,
                    float dy,
                    float fling_x,
                    float fling_y,
                    float expected_dx,
                    float expected_dy,
                    float expected_fling_x,
                    float expected_fling_y) {
  TouchActionFilter filter;
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);

  {
    // Scrolls with no direction hint are permitted in the |action| direction.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(action);

    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(0, 0, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                           kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
    EXPECT_EQ(expected_dx, scroll_update.data.scrollUpdate.deltaX);
    EXPECT_EQ(expected_dy, scroll_update.data.scrollUpdate.deltaY);

    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));
  }

  {
    // Scrolls hinted mostly in the larger axis are permitted in that axis.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(action);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_x, scroll_y,
                                                          kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                           kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
    EXPECT_EQ(expected_dx, scroll_update.data.scrollUpdate.deltaX);
    EXPECT_EQ(expected_dy, scroll_update.data.scrollUpdate.deltaY);

    // Ensure that scrolls in the opposite direction are not filtered once
    // scrolling has started. (Once scrolling is started, the direction may
    // be reversed by the user even if scrolls that start in the reversed
    // direction are disallowed.
    WebGestureEvent scroll_update2 =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(-dx, -dy, 0,
                                                           kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update2));
    EXPECT_EQ(-expected_dx, scroll_update2.data.scrollUpdate.deltaX);
    EXPECT_EQ(-expected_dy, scroll_update2.data.scrollUpdate.deltaY);

    WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
        fling_x, fling_y, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&fling_start));
    EXPECT_EQ(expected_fling_x, fling_start.data.flingStart.velocityX);
    EXPECT_EQ(expected_fling_y, fling_start.data.flingStart.velocityY);
  }

  {
    // Scrolls hinted mostly in the opposite direction are suppressed entirely.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(action);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_y, scroll_x,
                                                          kSourceDevice);
    EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                           kSourceDevice);
    EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
    EXPECT_EQ(dx, scroll_update.data.scrollUpdate.deltaX);
    EXPECT_EQ(dy, scroll_update.data.scrollUpdate.deltaY);

    EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));
  }
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, SimpleFilter) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTap, kSourceDevice);

  // No events filtered by default.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_EQ(kDeltaX, scroll_update.data.scrollUpdate.deltaX);
  EXPECT_EQ(kDeltaY, scroll_update.data.scrollUpdate.deltaY);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&tap));

  // TOUCH_ACTION_AUTO doesn't cause any filtering.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_EQ(kDeltaX, scroll_update.data.scrollUpdate.deltaX);
  EXPECT_EQ(kDeltaY, scroll_update.data.scrollUpdate.deltaY);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // TOUCH_ACTION_NONE filters out all scroll events, but no other events.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_EQ(kDeltaX, scroll_update.data.scrollUpdate.deltaX);
  EXPECT_EQ(kDeltaY, scroll_update.data.scrollUpdate.deltaY);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));

  // When a new touch sequence begins, the state is reset.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Setting touch action doesn't impact any in-progress gestures.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // And the state is still cleared for the next gesture.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Changing the touch action during a gesture has no effect.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_EQ(kDeltaX, scroll_update.data.scrollUpdate.deltaX);
  EXPECT_EQ(kDeltaY, scroll_update.data.scrollUpdate.deltaY);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, Fling) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(5, 10, 0,
                                                         kSourceDevice);
  const float kFlingX = 7;
  const float kFlingY = -4;
  WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
      kFlingX, kFlingY, kSourceDevice);
  WebGestureEvent pad_fling = SyntheticWebGestureEventBuilder::BuildFling(
      kFlingX, kFlingY, blink::WebGestureDeviceTouchpad);

  // TOUCH_ACTION_NONE filters out fling events.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&fling_start));
  EXPECT_EQ(kFlingX, fling_start.data.flingStart.velocityX);
  EXPECT_EQ(kFlingY, fling_start.data.flingStart.velocityY);

  // touchpad flings aren't filtered.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pad_fling));
  EXPECT_TRUE(filter.FilterGestureEvent(&fling_start));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, PanLeft) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_LEFT, kScrollX, kScrollY, kDX, kDY, kFlingX, kFlingY,
          kDX, 0, kFlingX, 0);
}

TEST(TouchActionFilterTest, PanRight) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = -7;
  const float kScrollY = 6;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_RIGHT, kScrollX, kScrollY, kDX, kDY, kFlingX,
          kFlingY, kDX, 0, kFlingX, 0);
}

TEST(TouchActionFilterTest, PanX) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_X, kScrollX, kScrollY, kDX, kDY, kFlingX, kFlingY,
          kDX, 0, kFlingX, 0);
}

TEST(TouchActionFilterTest, PanUp) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_UP, kScrollX, kScrollY, kDX, kDY, kFlingX, kFlingY,
          0, kDY, 0, kFlingY);
}

TEST(TouchActionFilterTest, PanDown) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = -7;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_DOWN, kScrollX, kScrollY, kDX, kDY, kFlingX, kFlingY,
          0, kDY, 0, kFlingY);
}

TEST(TouchActionFilterTest, PanY) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;
  const float kFlingX = 7;
  const float kFlingY = -4;

  PanTest(TOUCH_ACTION_PAN_Y, kScrollX, kScrollY, kDX, kDY, kFlingX, kFlingY, 0,
          kDY, 0, kFlingY);
}

TEST(TouchActionFilterTest, PanXY) {
  TouchActionFilter filter;
  const float kDX = 5;
  const float kDY = 10;
  const float kFlingX = 7;
  const float kFlingY = -4;

  {
    // Scrolls hinted in the X axis are permitted and unmodified.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(TOUCH_ACTION_PAN);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-7, 6, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
    EXPECT_EQ(kDX, scroll_update.data.scrollUpdate.deltaX);
    EXPECT_EQ(kDY, scroll_update.data.scrollUpdate.deltaY);

    WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
        kFlingX, kFlingY, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&fling_start));
    EXPECT_EQ(kFlingX, fling_start.data.flingStart.velocityX);
    EXPECT_EQ(kFlingY, fling_start.data.flingStart.velocityY);
  }

  {
    // Scrolls hinted in the Y axis are permitted and unmodified.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(TOUCH_ACTION_PAN);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_update));
    EXPECT_EQ(kDX, scroll_update.data.scrollUpdate.deltaX);
    EXPECT_EQ(kDY, scroll_update.data.scrollUpdate.deltaY);

    WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
        kFlingX, kFlingY, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&fling_start));
    EXPECT_EQ(kFlingX, fling_start.data.flingStart.velocityX);
    EXPECT_EQ(kFlingY, fling_start.data.flingStart.velocityY);
  }
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, BitMath) {
  // Verify that the simple flag mixing properties we depend on are now
  // trivially true.
  EXPECT_EQ(TOUCH_ACTION_NONE, TOUCH_ACTION_NONE & TOUCH_ACTION_AUTO);
  EXPECT_EQ(TOUCH_ACTION_NONE, TOUCH_ACTION_PAN_Y & TOUCH_ACTION_PAN_X);
  EXPECT_EQ(TOUCH_ACTION_PAN, TOUCH_ACTION_AUTO & TOUCH_ACTION_PAN);
  EXPECT_EQ(TOUCH_ACTION_MANIPULATION,
            TOUCH_ACTION_AUTO & ~TOUCH_ACTION_DOUBLE_TAP_ZOOM);
  EXPECT_EQ(TOUCH_ACTION_PAN_X, TOUCH_ACTION_PAN_LEFT | TOUCH_ACTION_PAN_RIGHT);
  EXPECT_EQ(TOUCH_ACTION_AUTO,
            TOUCH_ACTION_MANIPULATION | TOUCH_ACTION_DOUBLE_TAP_ZOOM);
}

TEST(TouchActionFilterTest, MultiTouch) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);

  // For multiple points, the intersection is what matters.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_EQ(kDeltaX, scroll_update.data.scrollUpdate.deltaX);
  EXPECT_EQ(kDeltaY, scroll_update.data.scrollUpdate.deltaY);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));

  // Intersection of PAN_X and PAN_Y is NONE.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_PAN_X);
  filter.OnSetTouchAction(TOUCH_ACTION_PAN_Y);
  filter.OnSetTouchAction(TOUCH_ACTION_PAN);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, Pinch) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);

  // Pinch is allowed with touch-action: auto.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Pinch is not allowed with touch-action: none.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));

  // Pinch is not allowed with touch-action: pan-x pan-y.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_PAN);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Pinch is allowed with touch-action: manipulation.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_MANIPULATION);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Pinch state is automatically reset at the end of a scroll.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Pinching can become disallowed during a single scroll gesture, but
  // can't become allowed again until the scroll terminates.
  // Note that the current TouchEventQueue design makes this scenario
  // impossible in practice (no touch events are sent to the renderer
  // while scrolling) and so no SetTouchAction can occur.  But this
  // could change in the future, so it's still worth verifying in this
  // unit test.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));

  // Once a pinch has started, any change in state won't affect the current
  // pinch gesture, but can affect a future one within the same scroll.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, DoubleTapWithTouchActionAuto) {
  TouchActionFilter filter;

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureDoubleTap, kSourceDevice);

  // Double tap is allowed with touch action auto.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&unconfirmed_tap));
  EXPECT_EQ(unconfirmed_tap.type, WebInputEvent::GestureTapUnconfirmed);
  // The tap cancel will come as part of the next touch sequence.
  filter.ResetTouchAction();
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event.
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_cancel));
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&double_tap));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, DoubleTap) {
  TouchActionFilter filter;

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureDoubleTap, kSourceDevice);

  // Double tap is disabled with any touch action other than auto.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_MANIPULATION);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&unconfirmed_tap));
  EXPECT_EQ(WebInputEvent::GestureTap, unconfirmed_tap.type);
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event. The tap cancel will come as part of the next touch sequence.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_AUTO);
  EXPECT_TRUE(filter.FilterGestureEvent(&tap_cancel));
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&double_tap));
  EXPECT_EQ(WebInputEvent::GestureTap, double_tap.type);
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, SingleTapWithTouchActionAuto) {
  TouchActionFilter filter;

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTap, kSourceDevice);

  // Single tap is allowed with touch action auto.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&unconfirmed_tap1));
  EXPECT_EQ(WebInputEvent::GestureTapUnconfirmed, unconfirmed_tap1.type);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, SingleTap) {
  TouchActionFilter filter;

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTap, kSourceDevice);

  // With touch action other than auto, tap unconfirmed is turned into tap.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap_down));
  EXPECT_FALSE(filter.FilterGestureEvent(&unconfirmed_tap1));
  EXPECT_EQ(WebInputEvent::GestureTap, unconfirmed_tap1.type);
  EXPECT_TRUE(filter.FilterGestureEvent(&tap));
  filter.ResetTouchAction();
}

TEST(TouchActionFilterTest, TouchActionResetsOnResetTouchAction) {
  TouchActionFilter filter;

  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureTap, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);

  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));

  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&tap));

  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
}

TEST(TouchActionFilterTest, TouchActionResetMidSequence) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::GestureScrollEnd, kSourceDevice);

  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));

  // Even though the allowed action is auto after the reset, the remaining
  // scroll and pinch events should be suppressed.
  filter.ResetTouchAction();
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_TRUE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_TRUE(filter.FilterGestureEvent(&scroll_end));

  // A new scroll and pinch sequence should be allowed.
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_begin));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));

  // Resetting from auto to auto mid-stream should have no effect.
  filter.ResetTouchAction();
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_update));
  EXPECT_FALSE(filter.FilterGestureEvent(&pinch_end));
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_end));
}

TEST(TouchActionFilterTest, ZeroVelocityFlingsConvertedToScrollEnd) {
  TouchActionFilter filter;
  const float kFlingX = 7;
  const float kFlingY = -4;

  {
    // Scrolls hinted mostly in the Y axis will suppress flings with a
    // component solely on the X axis, converting them to a GestureScrollEnd.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(TOUCH_ACTION_PAN_Y);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
        kFlingX, 0, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&fling_start));
    EXPECT_EQ(WebInputEvent::GestureScrollEnd, fling_start.type);
  }

  filter.ResetTouchAction();

  {
    // Scrolls hinted mostly in the X axis will suppress flings with a
    // component solely on the Y axis, converting them to a GestureScrollEnd.
    filter.ResetTouchAction();
    filter.OnSetTouchAction(TOUCH_ACTION_PAN_X);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-7, 6, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));

    WebGestureEvent fling_start = SyntheticWebGestureEventBuilder::BuildFling(
        0, kFlingY, kSourceDevice);
    EXPECT_FALSE(filter.FilterGestureEvent(&fling_start));
    EXPECT_EQ(WebInputEvent::GestureScrollEnd, fling_start.type);
  }
}

TEST(TouchActionFilterTest, TouchpadScroll) {
  TouchActionFilter filter;

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          2, 3, blink::WebGestureDeviceTouchpad);

  // TOUCH_ACTION_NONE filters out only touchscreen scroll events.
  filter.ResetTouchAction();
  filter.OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(filter.FilterGestureEvent(&scroll_begin));
}

}  // namespace content
