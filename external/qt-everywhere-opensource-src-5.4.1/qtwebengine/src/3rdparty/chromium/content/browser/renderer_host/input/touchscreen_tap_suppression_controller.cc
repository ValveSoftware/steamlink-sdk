// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

#include "content/browser/renderer_host/input/gesture_event_queue.h"

using blink::WebInputEvent;

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventQueue* geq,
    const TapSuppressionController::Config& config)
    : gesture_event_queue_(geq), controller_(this, config) {
}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

void TouchscreenTapSuppressionController::GestureFlingCancel() {
  controller_.GestureFlingCancel();
}

void TouchscreenTapSuppressionController::GestureFlingCancelAck(
    bool processed) {
  controller_.GestureFlingCancelAck(processed);
}

bool TouchscreenTapSuppressionController::FilterTapEvent(
    const GestureEventWithLatencyInfo& event) {
  switch (event.event.type) {
    case WebInputEvent::GestureTapDown:
      if (!controller_.ShouldDeferTapDown())
        return false;
      stashed_tap_down_.reset(new GestureEventWithLatencyInfo(event));
      return true;

    case WebInputEvent::GestureShowPress:
      if (!stashed_tap_down_)
        return false;
      stashed_show_press_.reset(new GestureEventWithLatencyInfo(event));
      return true;

    case WebInputEvent::GestureTapUnconfirmed:
      return stashed_tap_down_;

    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureDoubleTap:
      return controller_.ShouldSuppressTapEnd();

    default:
      break;
  }
  return false;
}

void TouchscreenTapSuppressionController::DropStashedTapDown() {
  stashed_tap_down_.reset();
  stashed_show_press_.reset();
}

void TouchscreenTapSuppressionController::ForwardStashedTapDown() {
  DCHECK(stashed_tap_down_);
  ScopedGestureEvent tap_down = stashed_tap_down_.Pass();
  ScopedGestureEvent show_press = stashed_show_press_.Pass();
  gesture_event_queue_->ForwardGestureEvent(*tap_down);
  if (show_press)
    gesture_event_queue_->ForwardGestureEvent(*show_press);
}

}  // namespace content
