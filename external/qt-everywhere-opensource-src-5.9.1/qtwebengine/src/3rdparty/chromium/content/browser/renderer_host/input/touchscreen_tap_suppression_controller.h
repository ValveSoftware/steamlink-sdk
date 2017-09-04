// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"

namespace content {

class GestureEventQueue;

// Controls the suppression of touchscreen taps immediately following the
// dispatch of a GestureFlingCancel event.
class TouchscreenTapSuppressionController
    : public TapSuppressionControllerClient {
 public:
  TouchscreenTapSuppressionController(
      GestureEventQueue* geq,
      const TapSuppressionController::Config& config);
  ~TouchscreenTapSuppressionController() override;

  // Should be called on arrival of GestureFlingCancel events.
  void GestureFlingCancel();

  // Should be called on arrival of ACK for a GestureFlingCancel event.
  // |processed| is true if the GestureFlingCancel successfully stopped a fling.
  void GestureFlingCancelAck(bool processed);

  // Should be called on arrival of any tap-related events. Returns true if the
  // caller should stop normal handling of the gesture.
  bool FilterTapEvent(const GestureEventWithLatencyInfo& event);

 private:
  // TapSuppressionControllerClient implementation.
  void DropStashedTapDown() override;
  void ForwardStashedTapDown() override;

  GestureEventQueue* gesture_event_queue_;

  typedef std::unique_ptr<GestureEventWithLatencyInfo> ScopedGestureEvent;
  ScopedGestureEvent stashed_tap_down_;
  ScopedGestureEvent stashed_show_press_;

  // The core controller of tap suppression.
  TapSuppressionController controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHSCREEN_TAP_SUPPRESSION_CONTROLLER_H_
