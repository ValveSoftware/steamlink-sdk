// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_

#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class TapSuppressionController;

class CONTENT_EXPORT TouchpadTapSuppressionControllerClient {
 public:
  virtual ~TouchpadTapSuppressionControllerClient() {}
  virtual void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& event) = 0;
};

// Controls the suppression of touchpad taps immediately following the dispatch
// of a GestureFlingCancel event.
class TouchpadTapSuppressionController : public TapSuppressionControllerClient {
 public:
  // The |client| must outlive the TouchpadTapSupressionController.
  TouchpadTapSuppressionController(
      TouchpadTapSuppressionControllerClient* client,
      const TapSuppressionController::Config& config);
  virtual ~TouchpadTapSuppressionController();

  // Should be called on arrival of GestureFlingCancel events.
  void GestureFlingCancel();

  // Should be called on arrival of ACK for a GestureFlingCancel event.
  // |processed| is true if the GestureFlingCancel successfully stopped a fling.
  void GestureFlingCancelAck(bool processed);

  // Should be called on arrival of MouseDown events. Returns true if the caller
  // should stop normal handling of the MouseDown. In this case, the caller is
  // responsible for saving the event for later use, if needed.
  bool ShouldDeferMouseDown(const MouseEventWithLatencyInfo& event);

  // Should be called on arrival of MouseUp events. Returns true if the caller
  // should stop normal handling of the MouseUp.
  bool ShouldSuppressMouseUp();

 private:
  friend class MockRenderWidgetHost;

  // TapSuppressionControllerClient implementation.
  virtual void DropStashedTapDown() OVERRIDE;
  virtual void ForwardStashedTapDown() OVERRIDE;

  TouchpadTapSuppressionControllerClient* client_;
  MouseEventWithLatencyInfo stashed_mouse_down_;

  // The core controller of tap suppression.
  TapSuppressionController controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchpadTapSuppressionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
