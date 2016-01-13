// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"

namespace content {

TouchpadTapSuppressionController::TouchpadTapSuppressionController(
    TouchpadTapSuppressionControllerClient* client,
    const TapSuppressionController::Config& config)
    : client_(client), controller_(this, config) {
}

TouchpadTapSuppressionController::~TouchpadTapSuppressionController() {}

void TouchpadTapSuppressionController::GestureFlingCancel() {
  controller_.GestureFlingCancel();
}

void TouchpadTapSuppressionController::GestureFlingCancelAck(bool processed) {
  controller_.GestureFlingCancelAck(processed);
}

bool TouchpadTapSuppressionController::ShouldDeferMouseDown(
    const MouseEventWithLatencyInfo& event) {
  bool should_defer = controller_.ShouldDeferTapDown();
  if (should_defer)
    stashed_mouse_down_ = event;
  return should_defer;
}

bool TouchpadTapSuppressionController::ShouldSuppressMouseUp() {
  return controller_.ShouldSuppressTapEnd();
}

void TouchpadTapSuppressionController::DropStashedTapDown() {
}

void TouchpadTapSuppressionController::ForwardStashedTapDown() {
  // Mouse downs are not handled by gesture event filter; so, they are
  // immediately forwarded to the renderer.
  client_->SendMouseEventImmediately(stashed_mouse_down_);
}

}  // namespace content
